#flags
WFLAGS = -Wall -Wextra -Wpedantic -Werror
CPPFLAGS = -std=c++20 -O3 -march=native -flto -fno-exceptions

#libs
LIBS = -lX11 -lfmt

#compiler
CPPC = g++
