#include "fsHelpers.h"

uint32_t freeblocks[1024];

int opendisk(char *filename, uint64_t size) {
  // given a filename and a file size:
  // Open a virtual disk, or create one if one does not already exist.
  // Open a file with the provided file name.
  // Specify O_RDWR for read/write access.
  int disk = open(filename, O_RDWR);
  // if open() returns -1, then the virtual disk does not yet exist.
  // open disk
  if (disk > -1) {
    return disk;
  } else {
    // create disk
    // Set the file's size to the given size.
    disk = open(filename, O_RDWR | O_CREAT);
    // format the disk.
    ftruncate(disk, size);
    // if there was any error, return -1
    if (disk > -1) {
      return disk;
    } else {
      printf("Error formatting disk: %d\n", disk);
      return -1;
    }
  }
  // Return the file's handle.
}

int readblock(int handle, uint64_t inode, void *buffer) {
    if (lseek(handle, inode * BLOCK_SIZE, SEEK_SET) < 0) {
        // perror("lseek error\n");
        return -1;
    }

    int r = read(handle, buffer, BLOCK_SIZE);
    if (r == BLOCK_SIZE)
        return 0;
    printf("read failed: %d\n", r);
    printf("errno: %d\n", errno);
    // handleerr(false, handle);
    return -1;
}

int writeblock(int handle, uint64_t inode, void *buffer) {
  // Write a block to the virtual disk from the given buffer.
  // The handle is the same one returned by opendisk().
  // inode is a block number.
  // Return 0 if successful, -1 if not.
  if (lseek(handle, inode * BLOCK_SIZE, SEEK_SET) < 0) {
    printf("error while doing lseek in writeblock\n");
    return -1;
  }

  int written = write(handle, buffer, BLOCK_SIZE);
  if (written > 0) {
    return 0;
  } else {
    printf("failed to write to block: %d\n", written);
    return -1;
  }
}

int syncdisk(int handle) {
  // Write all buffers to disk.
  // When done committing buffered data and metadata to disk, return.
  // If successful, return 0.
  // Else return -1.
    int synched = fsync(handle);
    if (synched < 0) {
      printf("error while synching disk\n");
    }
    return synched;
}

int closedisk(int handle) {
  // Close the disk.
  int c = close(handle);
  assert(c >= 0);
  return c;
}

int diskformat(int handle) {
  // write the superblock
  uint64_t superblock[BLOCK_SIZE];

  // save data to superblock
  superblock[0] = MAGIC_NUM;
  superblock[1] = BLOCK_SIZE * 64;
  superblock[2] = INODES;
  writeblock(handle, 0, superblock);

  // clear bitmap identifying used blocks
  // i.e. set block 1 to all zeros,
  // except for first two bits,
  // marking superblock and free list block as used
  setbit(0);
  setbit(1);
  for (int i = 2; i < INODES; i++) {
    clearbit(i);
  }
  writeblock(handle, 1, freeblocks);
  syncdisk(handle);

  return 0;
}

void diskdump(int handle) {
    uint64_t superblock[BLOCK_SIZE];
    readblock(handle, 0, superblock);

    int active = 0;
    int inactive = 0;
    printf("\nBegin Disk Dump...\n");
    char nodes[INODES];
    // show how many inodes are currently being used or free
    for (uint64_t i = 0; i < INODES; i++) {
      if (checkbitset(i)) {
          active++;
          nodes[i] = '+';
      } else {
        inactive++;
        nodes[i] = '-';
      }
    }
    uint64_t magic = superblock[0];
    printf("Magic: %lx\n", magic);
    int64_t diskSize = superblock[1];
    printf("Disk size (bytes): %ld\n", diskSize);
    printf("Active blocks: %d\n", active);
    printf("Inactive blocks: %d\n", inactive);
    printf("Active inodes: %d\n", active - 2);
    printf("Inactive inodes: %d\n", inactive - 2);
    printf("inodes = %s\n", nodes);
    printf("End disk dump\n");
}

int checkbitset(int n) {
  // n is the number of the bit we want to check is set
  uint32_t index = n / (8 * sizeof(uint32_t));
  uint32_t offset = n % (8 * sizeof(uint32_t));
  uint32_t mask = 1 << offset;

  if (freeblocks[index] & mask) {
    // returns true if free
    return true;
  } else {
    // returns false if not free
    return false;
  }
}

void setbit(int n) {
  // n is the number of the bit we want to check is set
  uint32_t index = n / (8 * sizeof(uint32_t));
  uint32_t offset = n % (8 * sizeof(uint32_t));
  uint32_t mask = 1 << offset;

  freeblocks[index] |= mask;
}

void clearbit(int n) {
  // n is the number of the bit we want to check is set
  uint32_t index = n / (8 * sizeof(uint32_t));
  uint32_t offset = n % (8 * sizeof(uint32_t));
  uint32_t mask = 1 << offset;

  freeblocks[index] &= ~mask;
}

void dumpfileinfo(int handle, uint64_t inode) {
  struct inode node;
  readblock(handle, inode, &node);

  printf("\nBegin file dump...\n");
  printf("File size: %ld\n", node.size);
  printf("Last modified: %ld\n", node.mtime);
  if (node.type) {
    printf("Type: Directory\n");
    dumpdirectory(handle, inode);
  } else {
    printf("Type: Regular file\n");
  }
  printf("Inode: %ld\n", node.blocks[0]);
  printf("End file dump\n\n");
}

void deletefile(int handle, uint64_t inode) {
  struct inode node;
  readblock(handle, inode, &node);
  clearbit(inode);
  clearbit(inode + INODES);
  for (int i = 0; i < (node.size / BLOCK_SIZE); i++) {
    clearbit(node.blocks[i + 1]);
    clearbit(node.blocks[i + 1] + INODES);
  }
  writeblock(handle, 1, freeblocks);
  syncdisk(handle);
}

int createfile(int handle, uint64_t filesize, uint64_t filetype) {
  // assert(filesize < 4096);
  struct inode node;
  node.size = filesize;
  node.mtime = time(NULL);
  node.type = filetype;
  uint64_t used = 0;
  uint64_t super[BLOCK_SIZE];
  readblock(handle, 0, super);
  // set file data blocks
  // start after the free block list's block to prevent overwriting it or the sb
  // search for unused bits in bitmap and mark them as belonging to the file
  for (uint64_t i = 2; i < INODES && used <= (filesize / BLOCK_SIZE); i++) {
    if (!checkbitset(i)) {
      node.blocks[used] = i;
      setbit(i);
      setbit(i + INODES);
      used++;
    }
  }

  // write changes to disk
  writeblock(handle, node.blocks[0], &node);
  writeblock(handle, 1, freeblocks);
  syncdisk(handle);
  return node.blocks[0];
}

int enlargefile(int handle, uint64_t inode, uint64_t size) {
  // access the inode at the given block number
  struct inode node;
  readblock(handle, inode, &node);

  if (size == 0) {
    return node.size;
  }
  printf("Increasing size of file w/ inode %ld by %ld bytes...\n", inode, size);

  uint64_t superblock[BLOCK_SIZE];
  readblock(handle, 0, superblock);

  // assert((node.size + size) < 4096);

  if (size < 0) {
    printf("Invalid size, please enter a size of 0 or greater\n");
    return node.size;
  }

  // check that increasing the file size wouldn't go past the limit
  // printf("%ld\n%ld\n", (node.size + size), superblock[1]);
  if ((node.size + size) > superblock[1]) {
    printf("Insufficient space, continuing\n");
    return node.size;
  }

  // calculate how many blocks the node currently uses
  // and how many more it needs in order to store the current file size plus the increase in file size
  uint64_t used = node.size / BLOCK_SIZE;
  uint64_t needed = (node.size + size) / BLOCK_SIZE;

  // if the file needs exactly as many blocks as it is currently using, then just allocate more space to the node
  if (used == needed) {
    node.size += size;
    writeblock(handle, inode, &node);
    printf("Done!\n");
    return node.size;
  } else {
    // otherwise, start allocating more blocks to the node's block list.
    // start setting more data blocks to the file.
    // start after the free block list's block to prevent overwriting it or the sb
    // search for unused bits in bitmap and mark them as belonging to the file
    printf("More blocks needed for size increase by %ld bytes. Attempting...\n", size);
    for (uint64_t i = 2; i < INODES && (used + 1) <= needed; i++) {
      if (!checkbitset(i)) {
        node.blocks[used + 1] = i;
        setbit(i);
        setbit(i + INODES);
        used++;
      }
    }
    printf("Done!\n");
    node.size += size;
    writeblock(handle, inode, &node);
    syncdisk(handle);
    return node.size;
  }
}

int shrinkfile(int handle, uint64_t inode, uint64_t size) {
  printf("Decreasing file size by %ld bytes...\n", size);
  // access the inode at the given block number
  struct inode node;
  readblock(handle, inode, &node);

  // calculate how many blocks the node currently uses
  // and how many more it needs in order to store the current file size plus the increase in file size
  uint64_t used = node.size / BLOCK_SIZE;
  uint64_t needed = (node.size - size) / BLOCK_SIZE;

  if (size < 0) {
    printf("Invalid size, please enter a size of 0 or greater\n");
    return node.size;
  }

  // if we're shrinking past the size of the file:
  if ((node.size - size) < 0) {
    printf("Invalid size, shrink size is greater than file size\n");
    return node.size;
  }

  // check if any fewer blocks are needed
  if (used == needed) {
    node.size -= size;
    writeblock(handle, inode, &node);
    // syncdisk(handle);
    printf("Done!\n");
    return node.size;
  } else {
    // otherwise, start deallocating more blocks from the node's block list.
    // start freeing data blocks from the file.
    // start after the free block list's block to prevent overwriting it or the sb
    // search for unused bits in bitmap and mark them as belonging to the file
    printf("Decreasing number of blocks allocated to file...\n");
    for (uint64_t i = used; i > needed; i--) {
        clearbit(node.blocks[i]);
        clearbit(node.blocks[i] + INODES);
    }
    printf("Done!\n");
    node.size -= size;
    writeblock(handle, inode, &node);
    syncdisk(handle);
    return node.size;
  }
}

int readfile(int handle, uint64_t inode, void *buffer, uint64_t size) {
    struct inode node;
    readblock(handle, inode, &node);

    uint8_t bufferBlock[BLOCK_SIZE];
    int curr = 0;
    for (uint64_t i = 0; i < size; i++) {
        if (i % (BLOCK_SIZE - 1) == 0) {
            readblock(handle, node.blocks[curr] + INODES, bufferBlock);
            curr++;
        }
        ((uint8_t*)buffer)[i] = bufferBlock[i % BLOCK_SIZE];
    }

    return size;
}

// returns the directory's starting inode
int createdirectory(int handle) {
  // assert that string length is 15 or less
  // when translating an inode to a block, it's the block number plus 2
  // struct dirent initdirent;
  // initdirent.name = "init";
  // create a helper function to create a free inode for a terminating dirent
  // initdirent.finode = freenode;
  // struct dirent entries[] = {initdirent};
  int dir_inode = createfile(handle, 4096, 1);

  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;

  // 204 = block size / (finode size (4 ) + name size (16))
  int i = 0;
  memset(bufferBlock, 0, 4096);
  // dirlist[0].finode = 0;
  // strcpy(dirlist[0].name, "init");

  dirlist[0].finode = 0xffffffff;
  // strcpy(dirlist[0].name, "init");

  return dir_inode;
}

void dumpdirectory(int handle, uint64_t dir_inode) {
  // unpack entries, print their file names and inodes
  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;
  readfile(handle, dir_inode, bufferBlock, 4096);

  int dircount = 0;
  printf("--- Directory entries ---\n");
  int i = 0;
  // while (i < 204) {
  //   if (dirlist[i].name != "" && dirlist[i].finode != 0) {
  //     printf("Filename: %s\nInode: %ld\n\n", dirlist[i].name, dirlist[i].finode);
  //     // dircount++;
  //   }
  //   i++;
  // }

  // while (dirlist[i].finode != 0xffffffff) {
  //   // printf("Filename: %s\nInode: %ld\n\n", dirlist[i].name, dirlist[i].finode);
  //     dircount++;
  //   i++;
  // }
  // for (int i = 0; i < length; i++) {
  //   printf("Filename: %s, inode: %ld\n", dirlist[i].name, dirlist[i].finode);
  // }
  printf("--- End directory entries ---\n");
  printf("# of entries: %d\n", dircount);
}

char* ls(int handle, uint64_t dir_inode) {
  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;
  readfile(handle, dir_inode, bufferBlock, 4096);

  char strs[204][16];
  int i = 0;
  while (i < 204) {
    if (dirlist[i].name != "" && dirlist[i].finode != 0) {
      printf("Filename: %s\nInode: %ld\n\n", dirlist[i].name, dirlist[i].finode);
      strcpy(strs[i], dirlist[i].name);
    }
    i++;
  }
  return strs;
}

int findinodebyfilename(int handle, uint64_t dir_inode, char* name) {
  printf("Searching for file with name '%s'...\n", name);
  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;
  readfile(handle, dir_inode, bufferBlock, 4096);

  int i = 0;
  while (i < 204) {
    if (dirlist[i].name == name) {
      return dirlist[i].finode;
    }
    i++;
  }
  printf("Could not find file with name '%s'\n\n", name);
  return -1;
}

void removedirentry(int handle, uint64_t dir_inode) {
  // memmove in strings package
  // you tell it 'here's where to go, wheres where to start'
}

int hierdirsearch(int handle, char* name, int root_inode) {
  // split filename into an array using / as a delimiter
  // for each name in the split filename array, starting at the first inode,
  // find the inode associated with that name
  char *find;
  find = strtok(name, "/");

  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;
  readfile(handle, root_inode, bufferBlock, 4096);
  // start at the root inode.
  int current_directory = root_inode;
  int name_inode = 0;
  // char* current_name = "";
  char* last_name = find[countof(find) - 1];
  for (int i = 0; i < countof(find); i++) {
    // check the current directory for the current name.
    // for each name in the file path, check if that file exists in current_directory.
    // if so, update the name to that file's name, and current_directory to that file's inode.
    name_inode = findinodebyfilename(handle, current_directory, find[i]);
    // if the current name being searched for can't be found in the current directory, it doesn't exist.
    if (name_inode < 0) {
      break;
    } else {
      // if the current name searched for was found,
      // and that name is the same as the last name,
      // return that name's inode.
      if (find[i] == last_name) {
        return name_inode;
      } else {
        // otherwise, update the current directory.
        current_directory = name_inode;
      }
    }
  }
  return -1;
}

int adddirentry(int handle, uint64_t dir_inode, uint64_t file_inode, char* filename) {
  assert(strlen(filename) <= 15);
  // need to have writetofile done first, since you are recording a list of file inodes and their respective names to the dir inode's data blocks.
  // struct dirent dirent;
  // dirent.name = filename; // how to pad for 16 bytes?
  // dirent.finode = ((uint8_t) file_inode);
  // struct inode node;
  // to grow an array, use realloc

  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;
  readfile(handle, dir_inode, bufferBlock, 4096);

  bool nofreespace = false;
  int i = 0;

  while (dirlist[i].finode != dirlist[204].finode) {
    if (dirlist[i].name == filename) {
      printf("%s already exists in directory\n", filename);
      return -1;
    }
    i++;
  }

  assert(nofreespace == false);

  i = 0;
  // iterate through dir list until we find an empty entry
  while (dirlist[i].finode != dirlist[204].finode) {
    if (dirlist[i].finode == 0) {
      // printf("!!!!!!!!!!!!!!free spot found for file entry!!!!!!!!!!!!!!!!\n");
      // struct dirent new_entry;
      // dirlist[i] = new_entry;
      strcpy(dirlist[i].name, filename);
      dirlist[i].finode = file_inode;
      printf("In adddirentry: dirlist[i].name: %s\nExpected name: %s\nInode: %ld\n\n", dirlist[i].name, filename, dirlist[i].finode);
      break;
    }
    i++;
  }
  writetofile(handle, dir_inode, bufferBlock, 4096);
  syncdisk(handle);

  return 0;
}

int writetofile(int handle, uint64_t inode, void *buffer, uint64_t size) {
	struct inode node;
	readblock(handle, inode, &node);

  uint8_t bufferBlock[BLOCK_SIZE];

	if (size <= node.size && (node.size / BLOCK_SIZE) == 0) {
  		writeblock(handle, inode + INODES, buffer);
      syncdisk(handle);
      return size;
	}

  else enlargefile(handle, inode, (size - node.size));

  int curr = 0;
  for (uint64_t i = 0; i < size; i++) {
    // if (i % (BLOCK_SIZE - 1) == 0) {
      if (i % (BLOCK_SIZE) == 0) {
        writeblock(handle, node.blocks[curr] + INODES, bufferBlock);
        curr++;
    }
    bufferBlock[i % BLOCK_SIZE] = ((uint8_t*) buffer)[i];
  }
  syncdisk(handle);
  return size;
}

void deletedirectory(int handle, uint64_t dir_inode) {
  // check if directory is empty.
  uint8_t bufferBlock[BLOCK_SIZE];
  struct dirent *dirlist = (struct dirent*) bufferBlock;
  readfile(handle, dir_inode, bufferBlock, 4096);

  bool empty = true;
  int i = 0;
  if (dirlist[0].finode == 0xffffffff) {
    deletefile(handle, dir_inode);
    printf("Success\n");
    empty = true;
  } else {
    empty = false;
    printf("huh\n");
  }
  // assert(empty == true);
  // struct dirent* entries = unpackdata(handle, inode);
  // if (countof(entries) == 0 || (entries[0].finode == 0)) {
  //   deletefile(handle, inode);
  //   return 0;
  // } else {
  //   printf("Directory with inode %ld is not empty\n", inode);
  //   return -1;
  // }
  // if len(entries) == 0 or entry[0].finode == 0:
    // deletefile(handle, inode);
    // return 0
  // else print error message
}
