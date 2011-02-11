OPTCFLAGS=-I/opt/local/include
OPTLFLAGS=-L/opt/local/lib
LOCAL_MONGREL2_LIB=-L. -lm2handler

CFLAGS=-g -std=c99 -Wall -Werror
LFLAGS=-L/opt/local/lib
LIBS=-lzmq -ljson $(OPTLIBS)
PREFIX?=/usr/local

SOURCES=m2handler.c bstr/bstrlib.c bstrlib.c

all : lib
	
m2handler.o : m2handler.h m2handler.c
	$(CC) $(CFLAGS) -c -o m2handler.o m2handler.c -I/opt/local/include

m2websocket.o : m2websocket.h m2websocket.c
	$(CC) $(CFLAGS) -c -o m2websocket.o m2websocket.c -I/opt/local/include

md5.o : md5/md5.h md5/md5.c md5/config.h
	$(CC) $(CFLAGS) -c -o md5.o md5/md5.c

bstrlib.o : bstr/bstrlib.c bstr/bstrlib.h
	$(CC) $(CFLAGS) -c -o bstrlib.o bstr/bstrlib.c

bstraux.o : bstr/bstraux.c bstr/bstraux.h
	$(CC) $(CFLAGS) -c -o bstraux.o bstr/bstraux.c -Ibstr

lib: m2handler.o m2websocket.o md5.o bstrlib.o bstraux.o
	ar rcs libm2handler.a m2handler.o m2websocket.o md5.o bstrlib.o bstraux.o
	ranlib libm2handler.a

test: body_toupper_handler fifo_reader_handler ws_handshake_handler

body_toupper.o : sample_handlers/body_toupper.c
	$(CC) $(CFLAGS) -I. -c sample_handlers/body_toupper.c -o body_toupper.o

body_toupper_handler: lib body_toupper.o bstrlib.o bstraux.o
	$(CC) $(CFLAGS) $(LIBS) -I. bstrlib.o bstraux.o body_toupper.o -o body_toupper_handler $(LOCAL_MONGREL2_LIB)

fifo_reader.o : sample_handlers/fifo_reader.c
	$(CC) $(CFLAGS) -I. -c sample_handlers/fifo_reader.c -o fifo_reader.o

fifo_reader_handler: lib fifo_reader.o bstrlib.o bstraux.o
	$(CC) $(CFLAGS) $(LIBS) -I. bstrlib.o bstraux.o fifo_reader.o -o fifo_reader_handler $(LOCAL_MONGREL2_LIB)

ws_handshake.o : sample_handlers/ws_handshake.c
	$(CC) $(CFLAGS) -I. -c sample_handlers/ws_handshake.c -o ws_handshake.o

ws_handshake_handler : lib ws_handshake.o bstrlib.o bstraux.o m2handler.o m2websocket.o
	$(CC) $(CFLAGS) $(LIBS) -I. bstrlib.o bstraux.o ws_handshake.o -o ws_handshake_hander $(LOCAL_MONGREL2_LIB)

install: lib
	install --owner=root libm2handler.a $(PREFIX)/lib
	install --owner=root m2handler.h $(PREFIX)/include
	install -d --owner=root $(PREFIX)/include/bstr/
	install --owner=root bstr/bstrlib.h $(PREFIX)/include/bstr/
	install --owner=root bstr/bstraux.h $(PREFIX)/include/bstr/


	# --mode=rw-r--r

clean : 
	rm -rf *.o m2handler libm2handler.a body_toupper_handler fifo_reader_handler handler_pipe
