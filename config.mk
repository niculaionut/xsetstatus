#flags
CPPSTD = -std=c++20
WFLAGS = -Wall -Wextra -Wpedantic
CPPFLAGS = ${CPPSTD} -O3 -march=native -flto -fno-exceptions -fno-rtti
DEBUGFLAGS = ${CPPSTD} -DNO_X11 -DIGNORE_ALREADY_RUNNING -g -Og -march=native -fno-rtti -fno-exceptions -fno-omit-frame-pointer

#libs
LIBS = -lX11 -lfmt

#compiler
CPPC = g++
