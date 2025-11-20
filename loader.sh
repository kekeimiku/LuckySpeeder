#!/bin/bash

set -euo pipefail

clang -arch arm64 -c shellcode.s -o shellcode.o

ld -arch arm64 -static -o shellcode.bin shellcode.o -e _start

clang -o bin2shellcode bin2shellcode.c

./bin2shellcode shellcode.bin

rm -f shellcode.o shellcode.bin bin2shellcode

clang -O3 -flto loader.c -o loader
