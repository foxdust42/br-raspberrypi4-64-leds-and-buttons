CC = gcc

.PHONY: clean

prog: main.c
	$(CC) -lgpiod -g -o gpio-progs main.c

clean:
	rm gpio-progs