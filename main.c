#include "fsHelpers.h"

void test1(int handle, void* superblock) {
  printf("\n~~~~~~~~~~ TESTING DIRECTORY CREATION ~~~~~~~~~~\n\n");
  printf("Creating root directory...\n");
  int root_dir = createdirectory(handle);
  // printf("Creating terminating inode for root directory inode list\n");
  dumpfileinfo(handle, root_dir);
  diskdump(handle);
  dumpfileinfo(handle, root_dir);
  printf("\n");

  printf("\n~~~~~~~~~~ TESTING FILE CREATION ~~~~~~~~~~\n\n");
  printf("Creating file 1...\n");
  int file_1 = createfile(handle, 512, 0);
  assert(file_1 > -1);
  printf("Done! Created file w/ inode %d\n", file_1);
  dumpfileinfo(handle, file_1);
  diskdump(handle);
  printf("\n");

  printf("Creating file 2...\n");
  int file_2 = createfile(handle, 512, 0);
  assert(file_2 > -1);
  printf("Done! Created file w/ inode %d\n", file_2);
  dumpfileinfo(handle, file_2);
  diskdump(handle);
  printf("\n");

  printf("\n~~~~~~~~~~ TESTING DIRECTORY ENTRY CREATION ~~~~~~~~~~\n\n");
  printf("Linking file 1 to root directory...\n");
  int f1entry = adddirentry(handle, root_dir, file_1, "home");
  assert(f1entry >= 0);
  printf("Success!\n");

  printf("\n~~~~~~~~~~ TESTING FILE SIZE CHANGES ~~~~~~~~~~\n\n");
  enlargefile(handle, file_1, 1024);
  dumpfileinfo(handle, file_1);
  diskdump(handle);
  printf("\n");

  shrinkfile(handle, file_1, 1024);
  dumpfileinfo(handle, file_1);
  diskdump(handle);
  printf("\n");

  printf("\n~~~~~~~~~~ TESTING FILE READ/WRITE OPERATIONS ~~~~~~~~~~\n\n");
  printf("Writing to file 1...\n");

  char* word = "Hello!\n";
  printf("writing file...\n");
  writetofile(handle, file_1, word, 8);
  char newword[8];
  printf("reading file...\n");
  readfile(handle, file_1, &newword, 8);
  printf("File contents: %s\n", newword);
  dumpfileinfo(handle, file_1);

  printf("\n~~~~~~~~~~ TESTING FILE FIND BY INODE ~~~~~~~~~~\n\n");
  int abc = findinodebyfilename(handle, root_dir, "home");
  printf("Inode of 'home': %d\n\n", abc);

  // printf("\n~~~~~~~~~~ TESTING LS ~~~~~~~~~~\n\n");
  // char* entries = ls(handle, root_dir);
  // int i = 0;
  // while (i < 204) {
  //   printf("%s\n", entries[i]);
  // }
  // diskdump(handle);

  // fails when file uses more than 4095 bytes

  printf("\n~~~~~~~~~~ TESTING FILE DELETION ~~~~~~~~~~\n\n");
  printf("Deleting file 1...\n");
  deletefile(handle, file_1);
  diskdump(handle);
  printf("\n");

  printf("Deleting file 2...\n");
  deletefile(handle, file_2);
  diskdump(handle);
  printf("\n");


  printf("\n~~~~~~~~~~ TESTING DIRECTORY DELETION ~~~~~~~~~~\n\n");
  printf("Deleting directory 1...\n");
  deletedirectory(handle, root_dir);
  deletefile(handle, root_dir);
  diskdump(handle);
  printf("\n");

  printf("Closing disk...\n");
  closedisk(handle);
}

int main() {
  // open disk
  printf("Opening disk...\n");
  char* filePath = "./testDisk.disk";
  int handle = opendisk(filePath, BLOCK_SIZE * 64);
  if (handle < 0) {
    printf("Error opening disk: %d\n\n", handle);
  }

  // check if the superblock exists on the disk
  // if not, format the disk and add it
  uint64_t superblock[BLOCK_SIZE];
  readblock(handle, 0, superblock);
  diskformat(handle);
  diskdump(handle);

  // check if the bit at idx 0 of the free block list is set
  // int superblockIsSet = checkbitset(0);
  // if (superblockIsSet < 1) {
  //   setbit(0);
  // }
  //
  // // checkbitset(0);
  //
  // // now the same for the free block list bit
  // int freeBlockIsSet = checkbitset(1);
  // if (freeBlockIsSet < 1) {
  //   setbit(1);
  // }

  test1(handle, superblock);

  return 0;
}
