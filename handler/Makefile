all : lib
	
mongrel_handler.o : mongrel_handler.h mongrel_handler.c
	gcc -c -std=c99 -g -Wall -Werror -o mongrel_handler.o mongrel_handler.c -I/opt/local/include

bstrlib.o : bstr/bstrlib.c bstr/bstrlib.h
	gcc -c -std=c99 -g -Wall -Werror -o bstrlib.o bstr/bstrlib.c -Ibstr

bstraux.o : bstr/bstraux.c bstr/bstraux.h
	gcc -c -std=c99 -g -Wall -Werror -o bstraux.o bstr/bstraux.c -Ibstr

lib: mongrel_handler.o bstrlib.o bstraux.o
	ar rcs libm2handler.a mongrel_handler.o bstrlib.o bstraux.o
#	gcc -shared -Wl,-soname,libm2handler.so.1 mongrel_handler.o bstrlib.o bstraux.o -lzmq -ljson -o libm2handler.so.1.0.1

body_toupper.o : sample_handlers/body_toupper.c
  gcc -c -std=c99 -g -Wall -Werror sample_handlers/body_toupper.c -o body_toupper.o

test: lib body_toupper.o mongrel_handler.o bstrlib.o bstraux.o
  gcc -std=c99 -g -Wall -Werror -lm2handler -L. body_toupper.o -o body_toupper_handler

clean : 
	rm -rf *.o mongrel_handler
