CC=gcc

all: simsh

simsh: simsh.o helper.o history.o redirection.o color.o
	gcc simsh.o helper.o history.o redirection.o color.o -o simsh

simsh.o: simsh.c
	gcc -c simsh.c

helper.o: helper.c
	gcc -c helper.c

history.o: history.c
	gcc -c history.c

redirection.o: redirection.c
	gcc -c redirection.c

color.o: color.c
	gcc -c color.c

clean:
	rm -rf *o simsh


