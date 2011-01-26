OPTCFLAGS=-I/opt/local/include
OPTLFLAGS=-L/opt/local/lib
LOCAL_MONGREL2_LIB=-L. -lm2handler

CFLAGS=-g -std=c99 -Wall -Werror
LFLAGS=-L/opt/local/lib
LIBS=-lzmq $(OPTLIBS)
PREFIX?=/usr/local

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
	ranlib libm2handler.a

test: body_toupper_handler fifo_reader_handler

body_toupper.o : sample_handlers/body_toupper.c
	$(CC) $(CFLAGS) -I. -c sample_handlers/body_toupper.c -o body_toupper.o

body_toupper_handler: lib body_toupper.o bstrlib.o bstraux.o
	$(CC) $(CFLAGS) $(LIBS) -I. bstrlib.o bstraux.o body_toupper.o -o body_toupper_handler $(LOCAL_MONGREL2_LIB)

fifo_reader.o : sample_handlers/fifo_reader.c
	$(CC) $(CFLAGS) -I. -c sample_handlers/fifo_reader.c -o fifo_reader.o

fifo_reader_handler: lib fifo_reader.o bstrlib.o bstraux.o
	$(CC) $(CFLAGS) $(LIBS) -I. bstrlib.o bstraux.o fifo_reader.o -o fifo_reader_handler $(LOCAL_MONGREL2_LIB)

install: lib
	install --owner=root libm2handler.a $(PREFIX)/lib
	install --owner=root mongrel_handler.h $(PREFIX)/include

	# --mode=rw-r--r

clean : 
	rm -rf *.o mongrel_handler libm2handler.a body_toupper_handler
