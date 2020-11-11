#!/bin/bash
color()(set -o pipefail;"$@" 2>&1>&3|sed $'s,.*,\e[31m&\e[m,'>&2)3>&1

color g++ ./data/formatted_code.cc -o ./data/binary
printf "Running binary\n\n"

timeout 2 ./data/binary
if [ $? -ne 0 ]; then
    echo -e "\033[31m Program does not halt\033[0m"
fi

rm ./data/binary*
