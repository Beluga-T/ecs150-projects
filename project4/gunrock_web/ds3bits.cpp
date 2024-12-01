#include <iostream>
#include <vector>
#include <iomanip>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

// Helper function to print a bitmap in hex format
void printBitmap(const unsigned char *bitmap, int numBits)
{
  int numBytes = (numBits + 7) / 8; // Round up to include all bits
  for (int i = 0; i < numBytes; i++)
  {
    cout << static_cast<int>(bitmap[i]) << " ";
  }
  cout << endl;
}

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    cerr << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  // Initialize Disk and LocalFileSystem
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);

  // Read and display the superblock
  super_t superBlock;
  fileSystem->readSuperBlock(&superBlock);

  cout << "Super" << endl;
  cout << "inode_region_addr " << superBlock.inode_region_addr << endl;
  cout << "inode_region_len " << superBlock.inode_region_len << endl;
  cout << "num_inodes " << superBlock.num_inodes << endl;
  cout << "data_region_addr " << superBlock.data_region_addr << endl;
  cout << "data_region_len " << superBlock.data_region_len << endl;
  cout << "num_data " << superBlock.num_data << endl;
  cout << endl;

  // Read and display the inode bitmap
  int inodeBitmapSize = superBlock.inode_bitmap_len * UFS_BLOCK_SIZE;
  vector<unsigned char> inodeBitmap(inodeBitmapSize);
  fileSystem->readInodeBitmap(&superBlock, inodeBitmap.data());

  cout << "Inode bitmap" << endl;
  printBitmap(inodeBitmap.data(), superBlock.num_inodes);

  // Read and display the data bitmap
  int dataBitmapSize = superBlock.data_bitmap_len * UFS_BLOCK_SIZE;
  vector<unsigned char> dataBitmap(dataBitmapSize);
  fileSystem->readDataBitmap(&superBlock, dataBitmap.data());

  cout << "\nData bitmap" << endl;
  printBitmap(dataBitmap.data(), superBlock.num_data);

  // Cleanup
  delete fileSystem;
  delete disk;

  return 0;
}
