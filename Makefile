# define some Makefile variables for the compiler and compiler flags to use Makefile variables later 
# in Makefile: $()
# -g 	adds debugging information to the executable file
# -Wall turns on most, but not all, compiler warnings

CC = gcc
CFLAGS = -g -Wall -pthread

# typing "make" will invoke the first target entry in the file
# (in this case the defauult target entry)
# you can nmae this target entry anything, but "default" or "all"
# are the most commonly used names by convention 

default: server.out client.out

client.out: client.c
	gcc client.c -o client.out

server.out: server.c
	gcc server.c -o server.out