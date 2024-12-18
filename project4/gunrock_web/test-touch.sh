#!/bin/bash
cp tests/disk_images/a.img a2.img

# Test script for ds3touch
DISK_IMAGE="a2.img"  # Path to your disk image
DS3TOUCH="./ds3touch"  # Path to your ds3touch binary

run_test() {
    echo "Running: $@"
    $@ && echo "Test passed." || echo "Test failed."
    echo "------------------------------------"
}

# 1. Test creating a new file in the root directory
echo "Test 1: Create a new file in the root directory"
run_test $DS3TOUCH $DISK_IMAGE 0 testfile.txt

# 2. Test creating a file inside another directory
echo "Test 2: Create a file inside another directory"
run_test $DS3TOUCH $DISK_IMAGE 1 nested_file.txt  # Assuming inode 1 corresponds to an existing directory

# 3. Test creating a file that already exists
echo "Test 3: Create a file that already exists"
run_test $DS3TOUCH $DISK_IMAGE 0 testfile.txt

# 4. Test creating a file in a non-existent directory
echo "Test 4: Create a file in a non-existent directory"
run_test $DS3TOUCH $DISK_IMAGE 999 nonexistent_file.txt

# 5. Test creating a file with an invalid name
echo "Test 5: Create a file with an invalid name"
run_test $DS3TOUCH $DISK_IMAGE 0 "invalid:file.txt"

echo "All tests completed."
