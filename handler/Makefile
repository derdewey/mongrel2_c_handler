all : mongrel_handler.o dds_init.o
	gcc -lzmq -ljson mongrel_handler.o -o mongrel_handler

mongrel_handler.o : mongrel_handler.h mongrel_handler.c
	gcc -c -std=c99 -g -Wall -Werror -o mongrel_handler.o mongrel_handler.c

clean : 
	rm -rf *.o
