all : mongrel_handler.o bstrlib.o bstraux.o
	gcc -lzmq -ljson mongrel_handler.o bstrlib.o bstraux.o -o mongrel_handler

mongrel_handler.o : mongrel_handler.h mongrel_handler.c
	gcc -c -std=c99 -g -Wall -Werror -o mongrel_handler.o mongrel_handler.c

bstrlib.o : bstr/bstrlib.c bstr/bstrlib.h
	gcc -c -std=c99 -g -Wall -Werror -o bstrlib.o bstr/bstrlib.c -Ibstr

bstraux.o : bstr/bstraux.c bstr/bstraux.h
	gcc -c -std=c99 -g -Wall -Werror -o bstraux.o bstr/bstraux.c -Ibstr

clean : 
	rm -rf *.o mongrel_handler
