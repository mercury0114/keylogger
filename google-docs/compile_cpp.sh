#!/bin/bash
color()(set -o pipefail;"$@" 2>&1>&3|sed $'s,.*,\e[31m&\e[m,'>&2)3>&1

if test -z "$1"
then
    echo "EMPTY"
else
    rm ./data/binary
    color g++ $1 -o ./docs/binary
    ./data/binary
fi
