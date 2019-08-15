#!/bin/bash
scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
input_header="$scriptdir/../src/darray.h"
gen() {
    prefix="$1";
    typename="$2";
    output="$3";
    echo "type: '$typename'"
    sed "s/_DELEM_TYPE_/${typename}/g ;s/darr/${prefix}/g ; s/DARR/${prefix^^}/g ; " "$input_header"  > "$output" || exit 1;
}
if [ $# -lt 2 ]; then
    echo "error, script usage:\
./gen_darr [prefix] [C_element_typename] [output_header_name]" >&2;
fi
gen "$1" "$2" "$3";
