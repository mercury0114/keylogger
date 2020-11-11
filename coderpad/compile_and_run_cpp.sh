#!/bin/bash
color()(set -o pipefail;"$@" 2>&1>&3|sed $'s,.*,\e[31m&\e[m,'>&2)3>&1

color g++ ./data/modified_code.cc -o ./data/binary
if [ $? -ne 0 ]; then
    exit 1
fi

printf "Running binary\n\n"
timeout 3 ./data/binary
if [ $? -ne 0 ]; then
    echo -e "\033[31m Program does not halt\033[0m"
fi
rm ./data/binary

