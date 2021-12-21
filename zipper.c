#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

static int verbose=0;
static int buffSize=1<<20;
static void *buff;

/*
 * Report a fatal error and quit
 */
static void die(const char *f, ...) {
    char buffer[1024];
    va_list args;
    va_start(args,f);
    vsnprintf(buffer,sizeof(buffer),f,args);
    perror(buffer);
    va_end(args);
    exit(EXIT_FAILURE);
}

/*
 * Conditionally report progress
 */
static void logPrint(int level, const char *f, ...) {
    va_list args;
    va_start(args,f);
    if (verbose>=level)
        vfprintf(stderr,f,args);
    va_end(args);
}

static int makeTempZip(const char *zName, int targetDir) {
    DIR *d;
    struct dirent *de;
    struct archive *a;
    struct archive_entry *e;
    struct stat statBuff;

    // Final zip does not exist, so create it as tmp123.zip first
    int fd = open(zName, O_RDWR | O_CREAT | O_TRUNC,0644);
    if (fd==-1) {
        perror(zName);
        return -1;
    }

    a = archive_write_new();
    archive_write_set_format_7zip(a);
    archive_write_open_fd(a,fd);
    e = archive_entry_new();

    // dup since fdopendir consumes the FD
    d=fdopendir(dup(targetDir));
    if (d==NULL) {
        perror("opendir");
        return -1;
    }
    while ((de=readdir(d))!=NULL) {
        int fd,len;
        if (de->d_type!=DT_REG)
            continue;
        if ((fd=openat(targetDir,de->d_name,O_RDONLY))==-1) {
            perror("de->d_name");
            closedir(d);
            return -1;
        }
        if (fstat(fd,&statBuff)) {
            perror("fstat");
            closedir(d);
            return -1;
        }
        archive_entry_clear(e);
        archive_entry_set_pathname(e,de->d_name);
        archive_entry_set_filetype(e,AE_IFREG);
        archive_entry_set_size(e, statBuff.st_size);
        archive_write_header(a,e);
        len=read(fd, buff, buffSize);
        while(len>0) {
            archive_write_data(a,buff,len);
            len=read(fd, buff, buffSize);
        }
        if (len<0) {
            perror("read");
            closedir(d);
            return -1;
        }
        close(fd);
    }
    closedir(d);
    archive_entry_free(e);
    archive_write_close(a);
    archive_write_free(a);
    return 0;
}

/*
 * Turn directory 123 into 123.7z then delete 123
 */
static void process(const char *dir) {
    char *zName;
    int targetDir,len;
    struct stat statBuff;

    if ((targetDir=open(dir,O_RDONLY|O_DIRECTORY))==-1) {
        // Target missing. Race? Move on to the next target.
        perror(dir);
        return;
    }

    len=strlen(dir)+7;
    zName=malloc(len);    // tmp ... .7z
    if (!zName)
        die("Out of memory allocating buffer");
    snprintf(zName, len, "tmp%s.7z", dir);
    logPrint(4, "zName = %s\n", zName);
    if (stat(zName + 3, &statBuff)) {
        // Create temp zip
        if (makeTempZip(zName,targetDir)) {
            free(zName);
            return;
        }

        // Now rename the resulting file:
        if (rename(zName,zName+3))
            die("Failed renaming '%s' to '%s'",zName,zName+3);
    }
    free(zName);

    // Now the final zip exists, delete the source files+dir
}

/*
 * Check if string is all-numeric, e.g. 123
 */
static int isnumeric(const char *s) {
    if (*s)
        return isdigit(*s) && isnumeric(s+1);
    return 1;
}

static int test_isnumeric(const char *s,const int v) {
    logPrint(1, "isnumeric(%s) = %d\n", s, isnumeric(s));
    if (isnumeric(s)!=v) {
        fprintf(stderr,"isnumeric(%s)!=%d",s,v);
        return 0;
    }
    return 1;
}

static int tests() {
    return test_isnumeric("",1) &&
        test_isnumeric("1234",1) &&
        test_isnumeric(".",0) &&
        test_isnumeric("foo",0) &&
        test_isnumeric("123foo",0);
}

/* Enumerate entries in directory
 */
static int listem(const char *dir) {
    DIR *d;
    struct dirent *de;

    if ((d=opendir(dir))==NULL) {
        perror("opendir");
        return -1;
    }
    while ((de=readdir(d))!=NULL) {
        logPrint(2, "Considering '%s'\n", de->d_name);
        if (isnumeric(de->d_name)) {
            logPrint(3, "Processing '%s'\n", de->d_name);
            process(de->d_name);
        }
    }
    if (closedir(d))
        perror("closedir");
    return 0;
}

int main(int argc,char **argv) {
    int opt;
    while((opt=getopt(argc,argv,"htv"))!=-1) {
        switch(opt) {
            case 'h':   // Help
            fprintf(stderr,"Usage:%s\t[-htv]\n-h - print this help text\n-t - run self-test\n-v Increase verbosity\n",argv[0]);
            return 0;

            case 't':   // Self-test
                if (!tests()) {
                    fprintf(stderr,"Self-test failed\n");
                    return -1;
                }
                fprintf(stderr,"Self-test passed\n");
                return 0;
            // break; No break needed due to return above

            case 'v':   // Verbose
            verbose++;
            break;
        }
    }
    if ((buff=malloc(buffSize)) == NULL)
        die("Out of memory allocating buffer");
  return listem(".");
}