#!/bin/bash
if ! [ -f xterm.js ]; then
    echo "You should get xterm.js/xterm.css from https://xtermjs.org"
fi
emcc -O3 -s WASM=1 -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["ccall", "cwrap"]' -s ASSERTIONS=1 -Wall -Wextra  -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter -I ../jadwal/src/ -I ../jadwal/third_party/ -I ../src emi.c --shell-file shell_minimal.html -o knit.html

if [ -d dist ]; then
    rm -r dist
fi
mkdir dist
cp knit.* xterm* dist
