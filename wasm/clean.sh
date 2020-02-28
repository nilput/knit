#!/bin/bash
shopt -s extglob
rm -r !(emi.c|shell_minimal.html|xterm.js|xterm.css|build.sh|clean.sh|dist.sh)
