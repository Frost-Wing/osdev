#!/bin/bash

# Function to get version of a command
get_version() {
    version=$($1 --version | head -n 1 | awk '{print $NF}')
    echo "${2} \"${version}\""
}

# Get versions
ld_version=$(get_version ld "ld_version")
gcc_version=$(get_version gcc "gcc_version")
cc_version=$(get_version cc "cc_version")
xorriso_version=$(get_version xorriso "xorriso_version")
tar_version=$(get_version tar "tar_version")
make_version=$(get_version make "make_version")

# Get the time when the compilation started.
current_datetime=$(date +"%d-%m-%Y %I:%M:%S.%3N %p")

# Format the output with escaped double quotes
output="//! This is an auto-generated code and is not meant to be messed with.
#include <versions.h>

cstring versions = \"${ld_version//\"/\\\"}\\n${gcc_version//\"/\\\"}\\n${cc_version//\"/\\\"}\\n${xorriso_version//\"/\\\"}\\n${tar_version//\"/\\\"}\\n${make_version//\"/\\\"}\\n\";
cstring date = \"${current_datetime//\"/\\\"}\";"

mkdir -p ./kernel/C/misc

echo "$output" > ./kernel/C/misc/versions.c
