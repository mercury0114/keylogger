#!/bin/bash
color()(set -o pipefail;"$@" 2>&1>&3|sed $'s,.*,\e[31m&\e[m,'>&2)3>&1

printf "Interpreting python code:\n\n"
color python3 ./data/formatted_code.cc
