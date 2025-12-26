FUSE_CFLAGS := $(shell pkg-config --cflags fuse3)
FUSE_LDFLAGS := $(shell pkg-config --libs fuse3)

READLINE_CFLAGS := $(shell pkg-config --cflags readline)
READLINE_LDFLAGS := $(shell pkg-config --libs readline)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE -std=c11 $(FUSE_CFLAGS) $(READLINE_CFLAGS) -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -Wno-format-truncation
LDFLAGS = $(FUSE_LDFLAGS) $(READLINE_LDFLAGS) -lpthread

TARGET = kubsh

.PHONY: all clean test

all: $(TARGET)

$(TARGET): kubsh.o vfs.o
	$(CC) $(CFLAGS) kubsh.o vfs.o -o $(TARGET) $(LDFLAGS)

kubsh.o: kubsh.c vfs.h
	$(CC) $(CFLAGS) -c kubsh.c -o kubsh.o

vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -c vfs.c -o vfs.o

clean:
	rm -f *.o $(TARGET)

test: all
	sudo docker run -v $(PWD):/mnt tyvik/kubsh_test:master
