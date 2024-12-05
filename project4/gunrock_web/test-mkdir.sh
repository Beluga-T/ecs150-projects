#!/bin/bash
cp tests/disk_images/a.img a2.img

DISK_IMAGE="a2.img"
DS3MKDIR="./ds3mkdir"
DS3LS="./ds3ls"

# Utility function to run a command and print its result
run_test() {
    echo "Running: $@"
    $@ && echo "Test passed." || echo "Test failed."
    echo "------------------------------------"
}

# Utility function to test expected failures
run_test_expected_failure() {
    echo "Running (expecting failure): $@"
    if $@; then
        echo "Test failed (expected failure but operation succeeded)."
    else
        echo "Test passed (operation failed as expected)."
    fi
    echo "------------------------------------"
}

# 1. Test creating a new directory in the root directory
echo "Test 1: Create a new directory in the root directory"
run_test $DS3MKDIR $DISK_IMAGE 0 testdir1

# Debugging: Check if `testdir1` exists in the root directory
echo "Debugging Test 1: List contents of root directory"
$DS3LS $DISK_IMAGE 0

# 2. Test creating a directory inside another directory
echo "Test 2: Create a directory inside another directory"
run_test $DS3MKDIR $DISK_IMAGE 1 nested_dir

# 3. Test creating a directory that already exists
echo "Test 3: Create a directory that already exists"
run_test $DS3MKDIR $DISK_IMAGE 0 testdir1

# 4. Test creating a directory in a non-existent directory
echo "Test 4: Create a directory in a non-existent directory"
run_test_expected_failure $DS3MKDIR $DISK_IMAGE 999 nonexistent_dir

# 5. Test creating a directory with an invalid name
echo "Test 5: Create a directory with an invalid name"
run_test_expected_failure $DS3MKDIR $DISK_IMAGE 0 "invalid:dir"

echo "Validation: Checking if testdir1 exists in the root directory"
$DS3LS $DISK_IMAGE 0 | grep "testdir1" && echo "Validation passed." || echo "Validation failed."

echo "All tests completed."
