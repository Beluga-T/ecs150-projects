#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <cstring>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

LocalFileSystem::LocalFileSystem(Disk *disk)
{
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super)
{
  // Read the superblock (block 0) into the provided `super` structure
  char buffer[UFS_BLOCK_SIZE];
  disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
  for (int i = 0; i < super->inode_bitmap_len; i++)
  {
    disk->readBlock(super->inode_bitmap_addr + i, inodeBitmap + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap)
{
}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap)
{
  for (int i = 0; i < super->data_bitmap_len; i++)
  {
    disk->readBlock(super->data_bitmap_addr + i, dataBitmap + i * UFS_BLOCK_SIZE);
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap)
{
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
}

int LocalFileSystem::lookup(int parentInodeNumber, string entryName)
{
  // Read the superblock to get filesystem metadata
  super_t superBlock;
  inode_t parentInode;
  readSuperBlock(&superBlock);

  // Ensure the parent inode is a directory
  if (parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Not a directory
  }

  // Fetch the inode for the parent directory
  if (stat(parentInodeNumber, &parentInode) != 0)
  {
    return -EINVALIDINODE; // Invalid parent inode
  }

  // Read all directory entries from the parent inode
  vector<char> directoryBuffer(parentInode.size);
  if (read(parentInodeNumber, directoryBuffer.data(), parentInode.size) != parentInode.size)
  {
    return -EINVALIDINODE; // Failed to read directory data
  }

  int entriesCount = parentInode.size / sizeof(dir_ent_t);
  dir_ent_t *dirEntries = reinterpret_cast<dir_ent_t *>(directoryBuffer.data()); // Cast readin data to directory entries

  // Search for the entry name in the directory entries
  for (int i = 0; i < entriesCount; ++i)
  {
    if (dirEntries[i].inum != -1 && entryName == dirEntries[i].name)
    {
      return dirEntries[i].inum; // Found matching entry
    }
  }

  return -ENOTFOUND; // Entry not found
}

int LocalFileSystem::stat(int inodeID, inode_t *inodeData)
{
  // Step 1: Read the superblock to fetch filesystem metadata
  super_t superBlock;
  readSuperBlock(&superBlock);

  // Step 2: Ensure the inode ID is within the valid range
  if (inodeID < 0 || inodeID >= superBlock.num_inodes)
  {
    return -EINVALIDINODE; // Invalid inode ID
  }

  // Step 3: Compute block and offset for the inode
  int inodeEntriesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  int containingBlock = superBlock.inode_region_addr + (inodeID / inodeEntriesPerBlock);
  int positionInBlock = (inodeID % inodeEntriesPerBlock) * sizeof(inode_t);

  // Step 4: Read the block containing the inode
  vector<unsigned char> inodeBlockBuffer(UFS_BLOCK_SIZE);
  disk->readBlock(containingBlock, inodeBlockBuffer.data());

  // Step 5: Extract the inode from the block
  memcpy(inodeData, inodeBlockBuffer.data() + positionInBlock, sizeof(inode_t));

  // Step 6: Validate inode type
  if (inodeData->type != UFS_DIRECTORY && inodeData->type != UFS_REGULAR_FILE)
  {
    return -EINVALIDINODE; // Invalid inode type
  }

  return 0; // Operation succeeded
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size)
{
  // Step 1: Validate the requested size
  if (size < 0)
  {
    return -EINVALIDSIZE; // Invalid size
  }

  // Step 2: Read the superblock to get file system layout information
  super_t super;
  readSuperBlock(&super);

  // Step 3: Read the inode
  inode_t inode;
  if (stat(inodeNumber, &inode) != 0)
  {
    return -EINVALIDINODE; // Invalid inode
  }

  // Step 4: Validate the requested size does not exceed the file size
  int bytesToRead = min(size, inode.size);

  // Step 5: Read data blocks
  int bytesRead = 0;
  int blockIndex = 0;
  vector<char> blockBuffer(UFS_BLOCK_SIZE);

  while (bytesRead < bytesToRead && blockIndex < DIRECT_PTRS)
  {
    // Check if the block is valid
    if (inode.direct[blockIndex] == 0)
    {
      break; // No more data blocks
    }

    // Read the block
    disk->readBlock(inode.direct[blockIndex], blockBuffer.data());

    // Calculate how many bytes to copy from this block
    int bytesInBlock = min(UFS_BLOCK_SIZE, bytesToRead - bytesRead);
    memcpy(static_cast<char *>(buffer) + bytesRead, blockBuffer.data(), bytesInBlock);

    bytesRead += bytesInBlock;
    blockIndex++;
  }

  return bytesRead; // Return the number of bytes read
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name)
{
  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size)
{
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
  return 0;
}
