#!/bin/sh
clang++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -O3 -march=native -flto xsetstatus.cpp -lfmt -lX11 -o xsetstatus
