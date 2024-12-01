#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib> // for std::stoi

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

int main(int argc, char *argv[])
{
  // Validate arguments
  if (argc != 3)
  {
    cerr << argv[0] << ": Usage: diskImageFile inodeNumber" << endl;
    return 1;
  }

  // Parse arguments
  string diskImageFile = argv[1];
  int inodeNumber;
  try
  {
    inodeNumber = stoi(argv[2]);
  }
  catch (invalid_argument &)
  {
    cerr << "Error reading file" << endl;
    return 1;
  }

  // Create Disk and FileSystem instances
  Disk *disk = new Disk(diskImageFile, UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);

  // Validate inode
  inode_t inodeData;
  if (fileSystem->stat(inodeNumber, &inodeData) != 0)
  {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  // Ensure the inode represents a file
  if (inodeData.type != UFS_REGULAR_FILE)
  {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  // Read file contents
  vector<char> fileContents(inodeData.size);
  int bytesRead = fileSystem->read(inodeNumber, fileContents.data(), inodeData.size);
  if (bytesRead != inodeData.size)
  {
    cerr << "Error reading file" << endl;
    delete fileSystem;
    delete disk;
    return 1;
  }

  // Output file contents
  cout << "File blocks" << endl;
  // print each of the disk block numbers
  for (int i = 0; i < inodeData.size; i += UFS_BLOCK_SIZE)
  {
    cout << inodeData.direct[i / UFS_BLOCK_SIZE] << endl;
  }
  cout << endl;
  cout << "File data" << endl;
  cout.write(fileContents.data(), bytesRead);

  // Cleanup
  delete fileSystem;
  delete disk;

  return 0;
}
