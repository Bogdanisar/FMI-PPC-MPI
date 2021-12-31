#!/bin/bash

otherMachineIP='192.168.133.132'
current_dir=$(realpath .)
parent_dir=$(dirname "$current_dir")
username=$(whoami)

set -x

ssh $otherMachineIP rm -rf "$current_dir"
scp -r "$current_dir" $username@$otherMachineIP:"$parent_dir"
