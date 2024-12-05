
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
  for (int blockNumber = 0; blockNumber < super->inode_bitmap_len; blockNumber++)
  {
    disk->writeBlock(super->inode_bitmap_addr + blockNumber, inodeBitmap + (blockNumber * UFS_BLOCK_SIZE));
  }
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
  for (int blockNumber = 0; blockNumber < super->data_bitmap_len; blockNumber++)
  {
    disk->writeBlock(super->data_bitmap_addr + blockNumber, dataBitmap + (blockNumber * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes)
{
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  for (int blockNumber = 0; blockNumber < super->inode_region_len; blockNumber++)
  {
    disk->readBlock(super->inode_region_addr + blockNumber, inodes + inodesPerBlock * blockNumber);
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes)
{
  int inodesPerBlock = UFS_BLOCK_SIZE / sizeof(inode_t);
  for (int blockNumber = 0; blockNumber < super->inode_region_len; blockNumber++)
  {
    disk->writeBlock(super->inode_region_addr + blockNumber, inodes + inodesPerBlock * blockNumber);
  }
}

int LocalFileSystem::lookup(int parentInodeNumber, string entryName)
{
  // Read the superblock to get filesystem metadata
  super_t superBlock;
  inode_t parant_node;
  readSuperBlock(&superBlock);

  // Ensure the parent inode is a directory
  if (parant_node.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE; // Not a directory
  }

  // Fetch the inode for the parent directory
  if (stat(parentInodeNumber, &parant_node) != 0)
  {
    return -EINVALIDINODE; // Invalid parent inode
  }

  // Read all directory entries from the parent inode
  vector<char> directoryBuffer(parant_node.size);
  if (read(parentInodeNumber, directoryBuffer.data(), parant_node.size) != parant_node.size)
  {
    return -EINVALIDINODE; // Failed to read directory data
  }

  int entriesCount = parant_node.size / sizeof(dir_ent_t);
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

int LocalFileSystem::create(int parentInodeNumber, int type, std::string name)
{
  super_t super;
  readSuperBlock(&super);

  // Validate parent inode
  inode_t parentInode;
  if (stat(parentInodeNumber, &parentInode) != 0 || parentInode.type != UFS_DIRECTORY)
  {
    return -EINVALIDINODE;
  }

  // Validate name
  if (name.length() >= DIR_ENT_NAME_SIZE || name.find_first_of(":/*?\"<>|") != std::string::npos)
  {
    return -EINVALIDNAME;
  }

  // Check if name already exists
  int existingInode = lookup(parentInodeNumber, name);
  if (existingInode >= 0)
  {
    inode_t existingInodeData;
    stat(existingInode, &existingInodeData);
    if (existingInodeData.type == type)
    {
      return existingInode;
    }
    return -EINVALIDTYPE;
  }

  // Allocate new inode
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);

  int newInodeNumber = -1;
  for (int i = 0; i < super.num_inodes; ++i)
  {
    if (!(inodeBitmap[i / 8] & (1 << (i % 8))))
    {
      newInodeNumber = i;
      inodeBitmap[i / 8] |= (1 << (i % 8));
      break;
    }
  }
  if (newInodeNumber == -1)
  {
    return -ENOTENOUGHSPACE;
  }

  writeInodeBitmap(&super, inodeBitmap);

  // Initialize new inode
  inode_t newInode = {};
  newInode.type = type;
  if (type == UFS_DIRECTORY)
  {
    newInode.size = 2 * sizeof(dir_ent_t);
  }
  else
  {
    newInode.size = 0;
  }

  if (type == UFS_DIRECTORY)
  {
    // Allocate data block and initialize `.` and `..`
    unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
    readDataBitmap(&super, dataBitmap);

    int freeBlock = -1;
    for (int i = 0; i < super.num_data; ++i)
    {
      if (!(dataBitmap[i / 8] & (1 << (i % 8))))
      {
        freeBlock = i;
        dataBitmap[i / 8] |= (1 << (i % 8));
        break;
      }
    }
    if (freeBlock == -1)
    {
      return -ENOTENOUGHSPACE;
    }

    writeDataBitmap(&super, dataBitmap);

    dir_ent_t entries[2] = {
        {".", newInodeNumber},
        {"..", parentInodeNumber}};

    char newDirBlock[UFS_BLOCK_SIZE] = {0};
    memcpy(newDirBlock, entries, sizeof(entries));
    disk->writeBlock(super.data_region_addr + freeBlock, newDirBlock);

    newInode.direct[0] = super.data_region_addr + freeBlock;
  }

  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inodes[newInodeNumber] = newInode;
  writeInodeRegion(&super, inodes);

  // Add entry to parent directory
  dir_ent_t newEntry = {};
  strncpy(newEntry.name, name.c_str(), DIR_ENT_NAME_SIZE - 1);
  newEntry.inum = newInodeNumber;

  char parentDirBlock[UFS_BLOCK_SIZE];
  disk->readBlock(parentInode.direct[0], parentDirBlock);
  memcpy(parentDirBlock + parentInode.size, &newEntry, sizeof(newEntry));
  disk->writeBlock(parentInode.direct[0], parentDirBlock);

  parentInode.size += sizeof(dir_ent_t);
  inodes[parentInodeNumber] = parentInode;
  writeInodeRegion(&super, inodes);

  return newInodeNumber;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size)
{
  // Validate the size
  if (size < 0)
  {
    return -EINVALIDSIZE;
  }
  // Load the superblock
  super_t super;
  readSuperBlock(&super);
  // Validate the inode number
  if (inodeNumber >= super.num_inodes || inodeNumber < 0)
  {
    return -EINVALIDINODE;
  }

  // Load the inode bitmap and validate that the inode is allocated
  unsigned char inode_bitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inode_bitmap);

  int offset = inodeNumber % 8;
  int bitmap_byte = inodeNumber / 8;
  if (!(inode_bitmap[bitmap_byte] & (0b1 << offset)))
  {
    return -ENOTALLOCATED;
  }

  // Load inodes and validate the inode type
  inode_t inodes[super.inode_region_len * UFS_BLOCK_SIZE / sizeof(inode_t)];
  readInodeRegion(&super, inodes);
  inode_t &inode = inodes[inodeNumber];

  if (inode.type != UFS_REGULAR_FILE)
  {
    return -EWRITETODIR; // Cannot write to directories
  }

  // Calculate current and required blocks
  int current_blocks = (inode.size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;
  int required_blocks = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE;

  // Ensure the size does not exceed the maximum file size
  if (required_blocks > DIRECT_PTRS)
  {
    return -EINVALIDSIZE; // File size exceeds maximum allowed
  }

  // Load the data bitmap
  unsigned char data_bitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  readDataBitmap(&super, data_bitmap);

  // Allocate additional blocks if needed
  for (int i = current_blocks; i < required_blocks; ++i)
  {
    int free_block = -1;
    for (int j = 0; j < super.num_data; ++j)
    {
      int byte_idx = j / 8;
      int bit_idx = j % 8;
      if (!(data_bitmap[byte_idx] & (0b1 << bit_idx)))
      {
        free_block = j;
        data_bitmap[byte_idx] |= (0b1 << bit_idx); // Mark block as allocated
        break;
      }
    }
    if (free_block == -1)
    {
      return -ENOTENOUGHSPACE; // Not enough space to allocate blocks
    }
    inode.direct[i] = super.data_region_addr + free_block;
  }

  // Deallocate unused blocks if reducing the file size
  for (int i = required_blocks; i < current_blocks; ++i)
  {
    int relative_block = inode.direct[i] - super.data_region_addr;
    int byte_idx = relative_block / 8;
    int bit_idx = relative_block % 8;
    data_bitmap[byte_idx] &= ~(0b1 << bit_idx); // Mark block as free
    inode.direct[i] = 0;
  }

  // Write the updated data bitmap back to disk
  writeDataBitmap(&super, data_bitmap);

  // Write data to allocated blocks
  const char *data_ptr = static_cast<const char *>(buffer);
  int bytes_written = 0;

  for (int i = 0; i < required_blocks; ++i)
  {
    char block_data[UFS_BLOCK_SIZE] = {0};
    int bytes_to_write = std::min(static_cast<int>(size - bytes_written), UFS_BLOCK_SIZE);
    memcpy(block_data, data_ptr, bytes_to_write);
    disk->writeBlock(inode.direct[i], block_data);
    data_ptr += bytes_to_write;
    bytes_written += bytes_to_write;
  }

  // Update the inode's size
  inode.size = size;
  writeInodeRegion(&super, inodes);

  // Return the number of bytes written
  return bytes_written;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
  return 0;
}
