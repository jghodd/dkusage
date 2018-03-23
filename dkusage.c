/* dkusage by jeff hodd
 *
 * This source code is in the public domain.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>      /* flags for read and write */
#include <gdbm.h>
#include <sys/types.h>  /* typedefs */
#include <sys/stat.h>   /* structure returned by stat */
#include <sys/dir.h>   /* local directory structure */

/***************************/
/* CONSTANT DEFINITIONS    */
/***************************/

#define WS_NONE         0
#define WS_RECURSIVE    (1 << 0)
#define WS_DEFAULT      WS_RECURSIVE
#define WS_FOLLOWLINK   (1 << 1)        /* follow symlinks */
#define WS_DOTFILES     (1 << 2)        /* per unix convention, .file is hidden */
#define WS_TARFILES     (1 << 3)        /* add tarfile overhead */

#define TAR_REC_SIZE	512
#define TAR_BLK_SIZE	20
#define TAR_BLK_FACTOR	(TAR_REC_SIZE * TAR_BLK_SIZE)

#define TAR_STDHDR_SZ	TAR_REC_SIZE
#define TAR_LRGHDR_SZ	(TAR_STDHDR_SZ * 3)

#define TAR_STD_PATHSZ	100

#define DB_FILENAME	"/tmp/inode.db"

#define FALSE		0
#define TRUE		1

/***************************/
/* FUNCTION PROTOTYPES     */
/***************************/

void fsize(char *, int);
void print_usage(void);
void dirwalk(char *, int, void (*fcn)(char *, int));

char * adjleading(char *);

GDBM_FILE opendb(char *);
void closedb(GDBM_FILE, char *);

/***************************/
/* STATIC VARIABLES        */
/***************************/

static unsigned long long total_size = 0;
static GDBM_FILE inode_db = NULL;
static int verbose = FALSE;
static char tmpname[FILENAME_MAX];

/*
 * calculate disk usage
*/
int main(int argc, char **argv)
{
    /*
     * set up the default options
    */
    int spec = WS_DEFAULT|WS_DOTFILES;

    /*
     * create and open the temporary inode db
    */
    inode_db = opendb(DB_FILENAME);
    if (inode_db == NULL)
        return 1;

    if (argc == 1)  /* default: current directory */
    {
        fsize(".", spec);
    }
    else
    {
        while (--argc > 0)
        {
            if ((*++argv)[0] == '-')
            {
                /*
                 * process options here : -f, -d, -t -v -h
                */
                if (strcmp(*argv, "-f") == 0)
                    spec |= WS_FOLLOWLINK;
                else if (strcmp(*argv, "-d") == 0)
                    spec &= ~WS_DOTFILES;
                else if (strcmp(*argv, "-t") == 0)
                    spec |= WS_TARFILES;
                else if (strcmp(*argv, "-v") == 0)
                    verbose = TRUE;
                else if (strcmp(*argv, "-h") == 0)
                {
                    /*
                     * print out the help text and close the inode db
                    */
                    print_usage();
                    closedb(inode_db, DB_FILENAME);

                    return 0;
                }
            }
            else
            {
                /*
                 * check for -f -t combo. disable -f if true
                */
                if ((spec & WS_TARFILES) && (spec & WS_FOLLOWLINK))
                {
                    fprintf(stderr, "Follow-links option (-f) invalid in tarfile mode : FOLLOWLINK disabled\n");
                    spec &= ~WS_FOLLOWLINK;
                }

                fsize(*argv, spec);
            }
        }
    }

    /*
     * adjust tarfile size estimate for 10240 byte blocking
    */
    if (spec & WS_TARFILES)
        total_size = (((total_size / TAR_BLK_FACTOR) + 1) * TAR_BLK_FACTOR);

    /*
     * print out the total size
    */
    printf("%lld\n", total_size);

    /*
     * close the inode db
    */
    closedb(inode_db, DB_FILENAME);

    return 0;
}

/*
 * open the inode db
*/
GDBM_FILE opendb(char * db_filename)
{
    GDBM_FILE idb = gdbm_open(DB_FILENAME, 512, GDBM_NEWDB, 0600, NULL);
    if (idb == NULL)
    {
        perror("gdbm_open");
    }

    return idb;
}

/*
 * close the inode db and remove the file
*/
void closedb(GDBM_FILE idb, char * db_filename)
{
    /*
     * clean up the temp db
    */
    if (idb != NULL)
    {
        gdbm_close(idb);
        unlink(db_filename);
    }
}

/*
 * print out the usage instructions
*/
void print_usage()
{
    printf("\n");
    printf("dkusage v0.3.5\n");
    printf("\n");
    printf("Usage: dkusage [-d][[-f]|[-t]][-v][-h] [directory_names]\n");
    printf("Options:\n");
    printf("    -d  do not include dotfiles\n");
    printf("    -f  follow symbolic links\n");
    printf("    -t  calculate tarfile size\n");
    printf("    -v  verbose mode - print filenames\n");
    printf("    -h  help - usage\n");
    printf("\n");
    printf("If no directory names are provided, dkusage defaults\n");
    printf("to the current directory. The default behaviour is\n");
    printf("not to follow symlinks and to include dotfiles.\n");
    printf("\n");
    printf("Note that the tarfile size calculation assumes a standard,\n");
    printf("non-compressed, tar file with standard blocking (20x512).\n");
    printf("\n");
    printf("The -f and -t options are incompatible. The -f option will be\n");
    printf("disabled if both options are used together.\n");
    printf("\n");
}

/*
 * fsize:  accumulate file sizes and make adjustments for tarfiles
*/
void fsize(char *name, int spec)
{
    struct stat stbuf;

    /*
     * get the appropriate stat structure
    */
    int rtstat = (spec & WS_FOLLOWLINK) ? stat(name, &stbuf) : lstat(name, &stbuf);
    if (rtstat == -1)
    {
        fprintf(stderr, "fsize: can't access %s\n", name);
        return;
    }

    /*
     * tar won't archive socket files
    */
    if ((spec & WS_TARFILES) && S_ISSOCK(stbuf.st_mode))
    {
        fprintf(stderr, "socket: %s - ignored\n", name);
        return;
    }

    /*
     * removing leading /, ./ and/or ../
    */
    char * pname = adjleading(name);

    /*
     * add a trailing / to directory names
    */
    if (S_ISDIR(stbuf.st_mode) && (pname[strlen(pname)] != '/'))
        pname[strlen(pname)] = '/';

    /*
     * print the filename if verbose
    */
    if (verbose)
        printf("%s\n", pname);

    /*
     * set up for inode db access
    */
    datum key, data; // Key and value
    char tmpkey[32];

    /*
     * create key and data elements
    */
    memset(tmpkey, 0, sizeof(tmpkey));
    sprintf(tmpkey, "%lu", stbuf.st_ino);

    key.dptr = tmpkey;
    key.dsize = strlen(tmpkey);

    data.dptr = key.dptr;
    data.dsize = key.dsize;

    /*
     * has this inode already been found
     * if yes, then we have a hard link
    */
    if (gdbm_store(inode_db, key, data, GDBM_INSERT) == 1)
    {
        /*
         * if we're doing tarfile size, add the correct size
         * 
         * 512 bytes header for filenames up to 100 characters long
         * 1536 byte header for filenames over 100 characters long
         * no data body
        */
        if (spec & WS_TARFILES)
        {
            if (strlen(pname) > TAR_STD_PATHSZ)
                total_size += TAR_LRGHDR_SZ;
            else
                total_size += TAR_STDHDR_SZ;
        }
    }
    else
    {
        /*
         * if we're doing tarfile size, add the correct size
         * 
         * 512 bytes header for filenames up to 100 characters long
         * 1536 byte header for filenames over 100 characters long
         * st_size for size if st_size divisible by 512
         * st_size rounded up to the nearest multiple of 512
        */
        if (spec & WS_TARFILES)
        {
            if (S_ISLNK(stbuf.st_mode))
            {
                char lnkdata[FILENAME_MAX];
                memset(lnkdata, 0, FILENAME_MAX);

                /*
                 * get the name of the file pointed to by the symlink
                */
                if (readlink(name, lnkdata, FILENAME_MAX) == -1)
                {
                    perror("readlink");
                    return;
                }

                /*
                 * if the symlink name or the name of the file it points to
                 * is longer than 100 bytes, then this is a LONGLINK header.
                */
                if ((strlen(lnkdata) > TAR_STD_PATHSZ) || (strlen(pname) > TAR_STD_PATHSZ))
                    total_size += TAR_LRGHDR_SZ;
                else
                    total_size += TAR_STDHDR_SZ;
            }
            else
            {
                /*
                 * add in file header
                */
                if (strlen(pname) > TAR_STD_PATHSZ)
                    total_size += TAR_LRGHDR_SZ;
                else
                    total_size += TAR_STDHDR_SZ;

                /*
                 * add the file size adjusted to a 512 byte blocking factor
                */
                if (S_ISREG(stbuf.st_mode))
                {
                    if (stbuf.st_size > 0)
                    {
                        off_t t_size = (stbuf.st_size % TAR_REC_SIZE) ? (((stbuf.st_size / TAR_REC_SIZE) + 1) * TAR_REC_SIZE) : stbuf.st_size;
                        total_size  += t_size;
                    }
                }
            }
        }
        else
        {
            /*
             * not doing tarfiles - just add st_size
            */
            if (stbuf.st_size > 0)
                total_size += stbuf.st_size;
        }
    }

    /*
     * walk the directory
    */
    if (S_ISDIR(stbuf.st_mode))
        dirwalk(name, spec, fsize);
}

/*
 * dirwalk:  apply fcn (fsize) to all files in dir
*/
void dirwalk(char *dir, int spec, void (*fcn)(char *, int))
{
    char name[FILENAME_MAX];
    struct dirent *dp;
    DIR *dfd;

    /*
     * open the directory
    */
    if ((dfd = opendir(dir)) == NULL)
    {
        fprintf(stderr, "dirwalk: can't open %s\n", dir);
        return;
    }

    /*
     * iterate through the directory entries
    */
    while ((dp = readdir(dfd)) != NULL)
    {
        /*
         * check for dot-files/-dirs, the . and .. directories
        */
        if (dp->d_name[0] == '.')
        {
            if (!(spec & WS_DOTFILES))
                continue;
            if ((!strcmp(dp->d_name, ".")) || (!strcmp(dp->d_name, "..")))
                continue;
        }

        /*
         * build the full path name and recurse into fsize
        */
        if (strlen(dir)+strlen(dp->d_name)+2 > FILENAME_MAX)
            fprintf(stderr, "dirwalk: name %s %s too long\n", dir, dp->d_name);
        else
        {
            sprintf(name, "%s/%s", dir, dp->d_name);
            (*fcn)(name, spec);
        }
    }

    /*
     * close the directory
    */
    closedir(dfd);
}

/*
 * strip the leading /, ./ and ../ from the file path
*/
char * adjleading(char * nmstr)
{
    memset(tmpname, 0, FILENAME_MAX);

    /*
     * strip the leading / 
    */
    if (nmstr[0] == '/')
        memcpy(tmpname, &nmstr[1], strlen(nmstr) - 1);
    else
        memcpy(tmpname, nmstr, strlen(nmstr));

    /*
     * strip any leading ./
    */
    char * pname = tmpname;

    if (pname[0] == '.')
    {
        if (strncmp(pname, "./", 2) == 0)
        {
            pname = &tmpname[2];
        }
        else
        {
            /*
             * strip any leading ../
            */
            int i;

            for (i = 0; i < strlen(tmpname); i+=3)
            {
                if (strncmp("../", &tmpname[i], 3) == 0)
                    pname = &tmpname[i+3];
                else
                    break;
            }
        }
    }

    return pname;
}

