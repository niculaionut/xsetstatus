#flags
CPPSTD = -std=c++20
WFLAGS = -Wall -Wextra -Wpedantic -Werror
CPPFLAGS = ${CPPSTD} -O3 -march=native -flto -fno-exceptions -fno-rtti
DEBUGFLAGS = ${CPPSTD} -DNO_X11 -DMULTIPLE_INSTANCES -g -Og -march=native -fno-rtti -fno-exceptions -fno-omit-frame-pointer

#libs
LIBS = -lX11 -lfmt

#compiler
CPPC = g++
