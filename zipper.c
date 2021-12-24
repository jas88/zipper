#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <archive.h>
#include <archive_entry.h>

struct zippingStats { uint64_t raw, files, zips, packed; };
static struct zippingStats stats={0, 0, 0, 0};
static signed int workers=4,level=3;
static int verbose=0;
static size_t buffSize=(size_t)1<<20;
static int childPipe;
static struct pollfd childPoller={0,POLLIN,0};

/*
 * Report a fatal error and quit
 */
static void __attribute__((__noreturn__)) /*@noreturn@*/ die(const char *f, ...) /*@globals errno,stderr@*/ {
    char buffer[1024];
    va_list args;
    va_start(args,f);
    (void)vsnprintf(buffer,sizeof(buffer),f,args);
    perror(buffer);
    va_end(args);
    exit(EXIT_FAILURE);
}

/*
 * Reap children, updating stats as we go. Return 1 if no children left.
 */
static int harvest() /*@globals errno,stderr;@*/ {
    if (wait(NULL)==-1 && errno==ECHILD)
        return 1;
    if (poll(&childPoller,1,0)==1) {
        struct zippingStats cs;
        if (read(childPoller.fd,&cs,sizeof(cs))!=(ssize_t)sizeof(cs))
            die("Control pipe corrupted");
        stats.files+=cs.files;
        stats.packed+=cs.packed;
        stats.raw+=cs.raw;
        stats.zips+=cs.zips;
    }
    return 0;
}

/*
 * Fork, possibly waiting until a previous worker exits to meet limits
 */
static int becomeWorker() /*@globals errno,stderr;@*/ {
    while (workers<0) {
        workers++;
        (void)harvest();
    }
    switch(fork()) {
        case 0: // We're the worker now
            if (close(childPoller.fd)!=0)
                die("close internal pipe");
            return 1;
        case -1:    // Error!
            die("Fork for worker");
        default: // Still the parent
            workers--;
            return 0;
    }
}

/*
 * Conditionally report progress
 */
static void logPrint(int vLevel, const char *f, ...) /*@globals errno,stderr;@*/ {
    va_list args;
    va_start(args,f);
    if (verbose>=vLevel)
        (void)vfprintf(stderr,f,args);
    va_end(args);
}

static void makeTempZip(const char *zName, int targetDir, struct zippingStats *zipStats) /*@globals errno,stderr;@*/ {
    DIR *d;
    void *buff;
    unsigned char cLevel[2]={'0','\0'};
    struct dirent *de;
    struct archive *a;
    struct archive_entry *e;
    struct stat statBuff;

    cLevel[0]+=(unsigned char)(level&15);
    if ((buff=malloc(buffSize)) == NULL)
        die("Out of memory allocating buffer");

    a = archive_write_new();
    if (archive_write_set_format_7zip(a)!=ARCHIVE_OK
       || archive_write_set_format_option(a,"7zip","compression","lzma2")!=ARCHIVE_OK
       || archive_write_set_format_option(a,"7zip","compression-level",(const char*)cLevel)!=ARCHIVE_OK
       )
        die("Unable to enable 7zip format '%s' (%x)", archive_error_string(a), archive_errno(a));
    if (archive_write_open_filename(a, zName)!=ARCHIVE_OK)
        die("Unable to configure archive FD due to '%s' (%x)", archive_error_string(a), archive_errno(a));
    e = archive_entry_new();

    // dup since fdopendir consumes the FD
    d=fdopendir(dup(targetDir));
    if (d==NULL)
        die("Unable to open target directory for '%s'",zName);
    while ((de=readdir(d))!=NULL) {
        int fd;
        ssize_t len;

        if (de->d_type!=(unsigned char)DT_REG) {
            if (strcmp(de->d_name,".")!=0 && strcmp(de->d_name,"..")!=0)
                die("Non-regular file '%s' found building '%s'",de->d_name,zName);
            continue;
        }
        zipStats->files++;
        if ((fd=openat(targetDir,de->d_name,O_RDONLY
#ifdef __MINGW32__
                | O_BINARY
#endif
        ))==-1)
            die("Failed to open '%s' for '%s'",de->d_name,zName);
        if (fstat(fd,&statBuff)!=0)
            die("Failed to stat '%s' for '%s'",de->d_name,zName);
        zipStats->raw+=statBuff.st_size;
        (void)archive_entry_clear(e);
        archive_entry_set_pathname(e,de->d_name);
        archive_entry_set_filetype(e,(unsigned int)AE_IFREG);
        archive_entry_set_size(e, statBuff.st_size);
        if (archive_write_header(a,e)!=ARCHIVE_OK)
            die("Error writing archive header");
        len=read(fd, buff, buffSize);
        while(len>0) {
            if (archive_write_data(a,buff,(size_t)len)!=len)
                die("Failed appending data to archive %s",zName);
            len=read(fd, buff, buffSize);
        }
        if (len<0)
            die("Failed reading '%s' for '%s'",de->d_name,zName);
        (void)close(fd);
    }
    (void)closedir(d);
    archive_entry_free(e);
    if (archive_write_close(a)!=ARCHIVE_OK || archive_write_free(a)!=ARCHIVE_OK)
        die("Error closing archive");
    if (stat(zName,&statBuff)!=0)
        die("Failed to stat output file '%s'",zName);
    zipStats->raw=(uint64_t)statBuff.st_size;
    free(buff);
}

/*
 * Turn directory 123 into 123.7z then delete 123
 */
static void process(const char *dir) /*@globals errno,stderr;@*/ {
    DIR *delDir;
    char *zName;
    int targetDir;
    size_t len;
    struct stat statBuff;
    struct dirent *de;
    struct zippingStats zipStats={0, 0, 0, 0};

    if ((targetDir=open(dir,O_RDONLY|O_DIRECTORY))==-1) {
        // Target missing. Race? Move on to the next target.
        // (We're a worker now, so die only applies to this dir)
        die(dir);
    }

    len=strlen(dir)+7;
    zName=malloc(len);    // tmp ... .7z
    if (!zName)
        die("Out of memory allocating buffer");
    if (snprintf(zName, len, "tmp%s.7z", dir)!=(int)(len-1))
        die("Buffer error generating temporary filename");
    logPrint(4, "zName = %s\n", zName);
    if (stat(zName + 3, &statBuff)!=0) {
        // Create temp zip - succeeds or exits:
        makeTempZip(zName, targetDir, &zipStats);

        // Now rename the resulting file:
        if (rename(zName,zName+3)!=0)
            die("Failed renaming '%s' to '%s'",zName,zName+3);
    }
    free(zName);

    // Now the final zip exists, delete the source files+dir
    if ((delDir=fdopendir(targetDir))==NULL)
        die("fdopendir(%s)",dir);
    rewinddir(delDir);
    while((de=readdir(delDir))!=NULL) {
        if (de->d_type==(unsigned char)DT_REG)
            if (unlinkat(targetDir,de->d_name,0)!=0)
                (void)fprintf(stderr,"WARN:Error '%s' deleting '%s\\%s'\n", strerror(errno),dir,de->d_name);
    }
    if (unlinkat(AT_FDCWD,dir,AT_REMOVEDIR)!=0)
        (void)fprintf(stderr,"WARN:Error '%s' deleting '%s'\n",strerror(errno),dir);
    (void)closedir(delDir);
    if (write(childPipe,&zipStats,sizeof(zipStats))!=(ssize_t)sizeof(zipStats))
        die("I/O error reporting progress to parent");
    exit(EXIT_SUCCESS);
}

/*
 * Check if string is all-numeric, e.g. 123
 */
static bool isnumeric(const char *s) {
    for (;;)
        switch (*s++) {
            case '\0':
                return true;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                break;
            default:
                return false;
        }
}

static bool test_isnumeric(const char *s,const bool v) /*@globals errno,stderr@*/ {
    bool r=isnumeric(s);
    logPrint(1, "isnumeric(%s) = %s\n", s, r?"true":"false");
    if (!r==!v)
        return true;
    (void)fprintf(stderr,"isnumeric(%s)!=%s",s,v?"true":"false");
    return false;
}

static bool tests() /*@globals errno,stderr@*/ {
    return test_isnumeric("",true) &&
        test_isnumeric("1234",true) &&
        test_isnumeric(".",false) &&
        test_isnumeric("foo",false) &&
        test_isnumeric("123foo",false);
}

/* Enumerate entries in directory
 */
static int listem(const char *dir) /*@globals errno,stderr@*/ {
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
            if (becomeWorker()==1)
                process(de->d_name);
        }
    }
    if (closedir(d)!=0)
        perror("closedir");
    return 0;
}

static void init() /*@globals errno,stderr;@*/ {
    int pipes[2];

    if (pipe(pipes)==-1) {
        die("pipe");
    }
    childPoller.fd=pipes[0];
    childPipe=pipes[1];
}

int main(int argc,char **argv) /*@globals stderr,errno;@*/ {
    int opt,rv;
    while((opt=getopt(argc,argv,"c:htvw:"))!=-1) {
        switch(opt) {
            case 'c':
                level=(int)strtol(optarg,NULL,10);
                if (level<0 || level>9) {
                    (void)fprintf(stderr,"Invalid compression level %d\n",level);
                    return -1;
                }
                break;

            case 'h':   // Help
                (void)fprintf(stderr,"Usage:%s (switches)\n-c n\t- Compression level n, 0-9\n-h\t- print this help text\n-t\t- run self-test\n-v\t- Increase verbosity\n-w n\t- Use n simultaneous worker processes to use, 1-99, default %d\n",argv[0],workers);
            return 0;

            case 't':   // Self-test
                if (tests()) {
                    (void)fprintf(stderr,"Self-test passed\n");
                    return 0;
                }
                (void)fprintf(stderr,"Self-test failed\n");
                return -1;
            // break; No break needed due to return above

            case 'v':   // Verbose
            verbose++;
            break;

            case 'w':   // Number of workers
                workers=(int)strtol(optarg,NULL,10);
                if (workers<=0 || workers>99) {
                    (void)fprintf(stderr,"Invalid worker count %d\n",workers);
                    return -1;
                }
                break;

            default:
                (void)fprintf(stderr,"WARN:Unknown switch '%c' passed\n",opt);
                break;
        }
    }
    init();
    rv=listem(".");
    while(harvest()==0);
    return rv;
}
