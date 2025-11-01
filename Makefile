all: build run

build: kubsh.c
	gcc kubsh.c -l readline -o kubsh

run: kubsh
	sudo ./kubsh

clean: rm kubsh
