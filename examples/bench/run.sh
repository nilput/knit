#!/bin/bash
rm results.txt
echo 'Make sure you compiled knit with "make opt -B"!'
perf stat -r 100 php7.2     gcd.php 2>> results.txt
perf stat -r 100 python3    gcd.py  2>> results.txt
perf stat -r 100 lua5.3     gcd.lua 2>> results.txt
perf stat -r 100 perl       gcd.pl  2>> results.txt
perf stat -r 100 ../../knit gcd.kn  2>> results.txt

