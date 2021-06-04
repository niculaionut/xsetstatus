#!/bin/sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Os -march=native -fno-exceptions -flto xsetstatus.cpp -lfmt -lX11
