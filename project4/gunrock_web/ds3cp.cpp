#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
using namespace std;
int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3"
         << endl;
    return 1;
  }

  unique_ptr<Disk> disk = make_unique<Disk>(argv[1], UFS_BLOCK_SIZE);
  unique_ptr<LocalFileSystem> fileSystem = make_unique<LocalFileSystem>(disk.get());
  string sourceFile = argv[2];
  int dstInode = stoi(argv[3]);

  // Open local file
  int fp = open(sourceFile.c_str(), O_RDONLY);
  if (fp < 0)
  {
    cerr << "Failed to open file" << endl;
    return 1;
  }

  // Read local file into write buffer
  char r_buf[UFS_BLOCK_SIZE];
  string w_buf;
  int bytes_read = 0;
  while ((bytes_read = read(fp, r_buf, sizeof(r_buf))) > 0)
  {
    w_buf.append(r_buf, bytes_read);
  }

  close(fp);

  if (bytes_read < 0)
  {
    cerr << "Read error" << endl;
    return 1;
  }

  // Write write buffer to destination inode
  disk->beginTransaction();
  if (fileSystem->write(dstInode, w_buf.c_str(), w_buf.length()) < 0)
  {
    disk->rollback();
    cerr << "Could not write to dst_file" << endl;
    return 1;
  }
  disk->commit();

  return 0;
}
