#!/bin/bash

set -e # exit on failure of subcommands
set -x # verbose output

script_path=$(realpath "$0")
project_dir=$(dirname "$script_path")
suman_dir="$project_dir/suman"
testing_dir="$project_dir/Testing"
username=$(whoami)

# parent_dir=$(dirname "$project_dir")

myIPWithWhitespace=$(hostname -I)
myIP=${myIPWithWhitespace//[[:blank:]]/}

# iterate CLI arguments
for otherMachineIP in "$@"
do
    if [ "$otherMachineIP" = "$myIP" ]
    then
        echo "Error: Don't list the current IP ($myIP) as an argument to this script."
        exit -1
    fi

    ssh "$otherMachineIP" mkdir -p "$project_dir"
    scp -r "$suman_dir" $username@$otherMachineIP:"$project_dir"
    scp -r "$testing_dir" $username@$otherMachineIP:"$project_dir"
done
