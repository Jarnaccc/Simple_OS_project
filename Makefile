# Makefile for par-shell, version 1
# Sistemas Operativos, DEI/IST/ULisboa 2015-16

GROUP_NUM=26
EXERCISE_NUM=5
ARCHIVE_NAME=G$(GROUP_NUM)_E$(EXERCISE_NUM).zip
CFLAGS=-g -Wall -pedantic -std=c99

all: par-shell fibonacci div0 par-shell-terminal

par-shell: par-shell.o commandlinereader.o list.o
	gcc -o par-shell par-shell.o commandlinereader.o list.o  -pthread

par-shell.o: par-shell.c commandlinereader.h
	gcc $(CFLAGS) -c par-shell.c

par-shell-terminal: par-shell-terminal.o commandlinereader.o
	gcc -o par-shell-terminal par-shell-terminal.o commandlinereader.o  -pthread

par-shell-terminal.o: par-shell-terminal.c commandlinereader.h
	gcc $(CFLAGS) -c par-shell-terminal.c

commandlinereader.o: commandlinereader.c commandlinereader.h
	gcc $(CFLAGS) -c commandlinereader.c

list.o: list.c list.h
	gcc $(CFLAGS) -c list.c

fibonacci: fibonacci.c
	gcc $(CFLAGS) -o fibonacci fibonacci.c

div0: div.c
	gcc $(CFLAGS) -o div0 div.c

clean:
	rm -f *.o par-shell fibonacci div0 par-shell-terminal core $(ARCHIVE_NAME)

dist:
	zip $(ARCHIVE_NAME) *.c *.h Makefile
