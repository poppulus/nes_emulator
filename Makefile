#Check for OS, might be scuffed
ifeq ($(OS), Windows_NT)
	uname_S := Windows
else 
	uname_S := $(shell uname -s)
endif

ifeq ($(uname_S), Windows)
	target = main.exe
endif
ifeq ($(uname_S), Linux)
	target = main
endif

#OBJS specifies which files to compile as part of the project
OBJS = main.c emu.c

#CC specifies which compiler we're using
CC = gcc

#COMPILER_FLAGS specifies the additional compilation options we're using
#
COMPILER_FLAGS = -Wall -g `pkg-config --cflags gtk4`

#LINKER_FLAGS specifies the libraries we're linking against
# -lSDL2_mixer gtk+-3.0
LINKER_FLAGS = `pkg-config --libs gtk4` -lSDL2

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = main

#This is the target that compiles our executable
all : $(OBJS)
	$(CC) $(COMPILER_FLAGS) -o $(OBJ_NAME) $(OBJS) $(LINKER_FLAGS) 
