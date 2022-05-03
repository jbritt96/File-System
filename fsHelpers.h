#include <inttypes.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 4096
#define INODES 128
#define MAGIC_NUM 0x1234BEAD
#define countof( arr) (sizeof(arr)/sizeof(*arr))

struct inode {
    uint64_t size;          /* size in bytes */
    uint64_t mtime;         /* same as returned by time(NULL) */
    uint64_t type;          /* regular or directory */
    uint64_t blocks[509];   /* list of data block numbers */
};

struct dirent {
  char name[16]; // use strlen; when storing, add 1 to that bc youre adding a null terminator
  uint64_t finode;
};

int opendisk(char *filename, uint64_t size);
int readblock(int handle, uint64_t blocknum, void *buffer);
int writeblock(int handle, uint64_t blocknum, void *buffer);
int syncdisk(int handle);
int closedisk(int handle);
int diskformat(int handle);
void diskdump(int handle);
int checkbitset(int n);
void setbit(int n);
void clearbit(int n);
int createfile(int handle, uint64_t filesize, uint64_t filetype);
void dumpfileinfo(int handle, uint64_t inode);
void deletefile(int handle, uint64_t inode);
int enlargefile(int handle, uint64_t inode, uint64_t size);
int shrinkfile(int handle, uint64_t inode, uint64_t size);
int readfile(int handle, uint64_t blocknum, void *buffer, uint64_t sz);
int writetofile(int handle, uint64_t inode, void* buffer, uint64_t size);
int createdirectory(int handle);
void deletedirectory(int handle, uint64_t dir_inode);
int adddirentry(int handle, uint64_t dir_inode, uint64_t file_inode, char* filename);
void dumpdirectory(int handle, uint64_t inode);
char* ls(int handle, uint64_t dir_inode);
int findinodebyfilename(int handle, uint64_t dir_inode, char* name);
void removedirentry(int handle, uint64_t dir_inode);
int hierdirsearch(int handle, char* name, int root_inode);
