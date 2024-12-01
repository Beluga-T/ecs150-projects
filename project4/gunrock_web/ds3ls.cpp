#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <cstring>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t &a, const dir_ent_t &b)
{
	return strcmp(a.name, b.name) < 0;
}

// Helper function to split the path into components
vector<string> splitPath(const string &path)
{
	vector<string> components;
	size_t start = 0;
	size_t end = 0;

	while ((end = path.find('/', start)) != string::npos)
	{
		if (end != start)
		{
			components.push_back(path.substr(start, end - start));
		}
		start = end + 1;
	}

	if (start < path.size())
	{
		components.push_back(path.substr(start));
	}

	return components;
}

// Helper function to resolve the inode number from a path
int resolvePath(LocalFileSystem *fileSystem, const string &path, int &parentInode)
{
	vector<string> pathComponents = splitPath(path);
	int currentInode = UFS_ROOT_DIRECTORY_INODE_NUMBER;
	parentInode = currentInode;

	for (const auto &component : pathComponents)
	{
		parentInode = currentInode;
		int result = fileSystem->lookup(currentInode, component);
		if (result < 0)
		{
			return -1; // Entry not found
		}
		currentInode = result;
	}

	return currentInode;
}

// Helper function to list directory contents
int listDirectoryContents(LocalFileSystem *fileSystem, int directoryInode)
{
	inode_t directoryInodeData;
	if (fileSystem->stat(directoryInode, &directoryInodeData) != 0)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	// Ensure the inode is a directory
	if (directoryInodeData.type != UFS_DIRECTORY)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	int entryCount = directoryInodeData.size / sizeof(dir_ent_t);
	vector<dir_ent_t> entries(entryCount);

	if (fileSystem->read(directoryInode, entries.data(), directoryInodeData.size) != directoryInodeData.size)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	// Sort and display entries
	sort(entries.begin(), entries.end(), compareByName);
	for (const auto &entry : entries)
	{
		cout << entry.inum << '\t' << entry.name << endl;
	}

	return 0;
}

// Displays information about a file by finding its entry in the parent directory.
int displayFileInfo(LocalFileSystem *fileSystem, int fileInode, int parentInode)
{
	inode_t parentInodeData;
	if (fileSystem->stat(parentInode, &parentInodeData) != 0)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	int entryCount = parentInodeData.size / sizeof(dir_ent_t);
	vector<dir_ent_t> entries(entryCount);

	if (fileSystem->read(parentInode, entries.data(), parentInodeData.size) != parentInodeData.size)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	// Find and display the file entry
	for (const auto &entry : entries)
	{
		if (entry.inum == fileInode)
		{
			cout << fileInode << '\t' << entry.name << endl;
			return 0;
		}
	}

	cerr << "Directory not found" << endl;
	return 1;
}

int Lsdir(LocalFileSystem *fileSystem, const string &path)
{
	if (path.empty() || path[0] != '/')
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	int parentInode;
	int targetInode = resolvePath(fileSystem, path, parentInode);

	if (targetInode < 0)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	inode_t targetInodeData;
	if (fileSystem->stat(targetInode, &targetInodeData) != 0)
	{
		cerr << "Directory not found" << endl;
		return 1;
	}

	if (targetInodeData.type == UFS_DIRECTORY)
	{
		// The path refers to a directory
		return listDirectoryContents(fileSystem, targetInode);
	}
	else if (targetInodeData.type == UFS_REGULAR_FILE)
	{
		// The path refers to a file
		return displayFileInfo(fileSystem, targetInode, parentInode);
	}
	else
	{
		cerr << "Directory not found" << endl;
		return 1;
	}
}
int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		cerr << argv[0] << ": Usage: diskImageFile directory" << endl;
		cerr << "Example:" << endl;
		cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
		return 1;
	}

	// dynamic memory for Disk and LocalFileSystem
	Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
	LocalFileSystem *fileSystem = new LocalFileSystem(disk);

	string directoryPath = argv[2];
	int result = Lsdir(fileSystem, directoryPath);

	// Cleanup
	delete fileSystem;
	delete disk;

	return result;
}
