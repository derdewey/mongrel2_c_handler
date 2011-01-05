all : mongrel_handler.o dds_init.o
	gcc -lzmq mongrel_handler.o dds_init.o -o mongrel_handler

mongrel_handler.o : mongrel_handler.h mongrel_handler.c
	gcc -c -Wall -Werror -o mongrel_handler.o mongrel_handler.c

dds_init.o : dds_init.h dds_init.c
	gcc -c -Wall -Werror -o dds_init.o dds_init.c
