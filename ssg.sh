#!/bin/bash
original_dir="$(pwd)"
cd "$(dirname "$0")"

exe_name=ssg

# build
rm $exe_name 2>/dev/null

command="$exe_name.c -o $exe_name -g"
bear -- gcc $command
gcc $command

# run
if [ -e "./$exe_name" ]; then
  cd "$original_dir"
  "$(dirname "$0")/$exe_name" "${@:1}"
fi
