.PHONY: clean


all: example.c ucfg.c ucfg.h
	gcc example.c ucfg.c -ggdb -Wall -Wextra -pedantic -ansi -o example

clean:
	rm -f example example.conf
