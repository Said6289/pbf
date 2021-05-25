GAME_EXECUTABLE = fluid

COMPILER_FLAGS = $(shell sdl2-config --cflags) -Iinclude -Wall -Wextra -pedantic -std=c++20 -Werror -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-value -Wno-unused-variable -Wno-writable-strings -Wno-null-dereference
LINKER_FLAGS = $(shell sdl2-config --libs) -lm -ldl -lGL

.PHONY: all
all:
	clang -O2 -g $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(GAME_EXECUTABLE) main.cpp
