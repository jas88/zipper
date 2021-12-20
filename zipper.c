#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "archive.h"
#include "archive_entry.h"

/*
 * Turn directory 123 into 123.7z then delete 123
 */
static void process(const char *dir) {
    void *buff;
    char *zName;
    int originDir,targetDir,len,buffSize=1 << 20;
    struct archive *a;
    struct stat statBuff;

    if ((buff=malloc(buffSize)) == NULL) {
        perror("malloc");
        return;
    }
    len=strlen(dir)+7;
    zName=malloc(len);    // tmp ... .7z
    if (!zName) {
        perror("malloc");
        return;
    }
    snprintf(zName, len, "tmp%s.7z", dir);
    if (!stat(zName + 3, &statBuff)) {
        DIR *d;
        struct dirent *de;
        struct archive_entry *e;

        // Final zip does not exist, so create it as tmp123.zip first
        int fd = open(zName, O_RDWR | O_CREAT | O_TRUNC);
        if (fd==-1) {
            perror(zName);
            return;
        }

        if (chdir(dir)) {
            perror(dir);
            return;
        }

        a = archive_write_disk_new();
        archive_write_set_format_7zip(a);
        archive_write_open_fd(a,fd);
        e = archive_entry_new();

        d=opendir(".");
        if (d==NULL) {
            perror("dir");
            return;
        }
        while ((de=readdir(d))!=NULL) {
            int fd,len;
            if (de->d_type!=DT_REG)
                continue;
            if ((fd=open(de->d_name,O_RDONLY))==-1) {
                perror("de->d_name");
                closedir(d);
                return;
            }
            if (fstat(fd,&statBuff)) {
                perror("fstat");
                closedir(d);
                return;
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
                return;
            }
            close(fd);
        }
        closedir(d);
        archive_entry_free(e);
        archive_write_close(a);
        archive_write_free(a);

        // Now return to original directory and rename the resulting file:

    }

    // Now the final zip exists, delete the source files+dir
}

/*
  Check if string is all-numeric, e.g. 123
*/
static int isnumeric(const char *s) {
    if (*s)
        return isdigit(*s) && isnumeric(s+1);
    return 1;
}

static int test_isnumeric(const char *s,const int v) {
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
        if (isnumeric(de->d_name))
            printf("%s\n",de->d_name);
    }
    if (closedir(d))
        perror("closedir");
    return 0;
}

int main(int argc,char **argv) {
    if (!tests()) {
        fprintf(stderr,"Self-test failed\n");
        return -1;
    }
  return listem(".");
}