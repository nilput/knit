#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
! [ -d hasht ] && git clone https://github.com/nilput/hasht "$script_dir"/../hasht || exit 1;
echo 'Cloned dependencies!'
