#!/bin/sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -O3 -march=native -flto xsetstatus.cpp -lfmt -lX11 -o xsetstatus
