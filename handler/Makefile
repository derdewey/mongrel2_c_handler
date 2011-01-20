CFLAGS=-g -std=c99 -Wall -Werror $(OPTFLAGS) -I/opt/local/include
LFLAGS=-L/opt/local/lib
LIBS=-lzmq $(OPTLIBS)

SOURCES=mongrel_handler.c bstr/bstrlib.c bstrlib.c

all : lib
	
mongrel_handler.o : mongrel_handler.h mongrel_handler.c
	$(CC) $(CFLAGS) -c -o mongrel_handler.o mongrel_handler.c -I/opt/local/include

bstrlib.o : bstr/bstrlib.c bstr/bstrlib.h
	$(CC) $(CFLAGS) -c -o bstrlib.o bstr/bstrlib.c

bstraux.o : bstr/bstraux.c bstr/bstraux.h
	$(CC) $(CFLAGS) -c -o bstraux.o bstr/bstraux.c -Ibstr

lib: mongrel_handler.o bstrlib.o bstraux.o
	ar rcs libm2handler.a mongrel_handler.o bstrlib.o bstraux.o
#	gcc -shared -Wl,-soname,libm2handler.so.1 mongrel_handler.o bstrlib.o bstraux.o -lzmq -ljson -o libm2handler.so.1.0.1

body_toupper.o : sample_handlers/body_toupper.c
	$(CC) $(CFLAGS) -c sample_handlers/body_toupper.c -o body_toupper.o

test: lib body_toupper.o mongrel_handler.o bstrlib.o bstraux.o
	gcc -std=c99 -g -Wall -Werror -lm2handler -lzmq -I/opt/local/include -L. -L/opt/local/lib body_toupper.o -o body_toupper_handler

clean : 
	rm -rf *.o mongrel_handler libm2handler.a body_toupper_handler
