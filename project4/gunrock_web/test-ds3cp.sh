#!/bin/bash

cp tests/disk_images/a.img a2.img
DISK_IMAGE="a2.img"
DS3CP="./ds3cp"

# Test 1: Copy a valid file to an existing inode
echo "Test 1: Copying a valid file to inode 5"
$DS3CP $DISK_IMAGE testfile.txt 5 && echo "Test passed." || echo "Test failed."

# Test 2: Copy a file to a non-existent inode
echo "Test 2: Copying to a non-existent inode"
$DS3CP $DISK_IMAGE testfile.txt 999 && echo "Test failed." || echo "Test passed."

# Test 3: Copy a non-existent file
echo "Test 3: Copying a non-existent file"
$DS3CP $DISK_IMAGE non_existent.txt 5 && echo "Test failed." || echo "Test passed."

# Test 4: Copy a file too large for the inode
echo "Test 4: Copying a large file"
truncate -s 100M largefile.txt
$DS3CP $DISK_IMAGE largefile.txt 5 && echo "Test failed." || echo "Test passed."
rm largefile.txt

echo "All tests completed."
