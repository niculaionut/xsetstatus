#flags
CPPSTD = -std=c++20
WFLAGS = -Wall -Wextra -Wpedantic
CPPFLAGS = ${CPPSTD} -O3 -march=native -flto -fno-exceptions -fno-rtti
NOX11FLAGS = ${CPPFLAGS} -DNO_X11 -DIGNORE_ALREADY_RUNNING

#libs
LIBS = -lX11 -lfmt

#compiler
CPPC = g++
