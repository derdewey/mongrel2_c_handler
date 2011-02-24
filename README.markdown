# Mongrel2 C Handler: libm2handler


A C library for handling requests from Mongrel2. Includes a suite of sample handlers to get you up and running.

### Prequisites ###

#### mongrel2 ####
http://www.mongrel2.org/
Installed and configured to your liking. If you are trying out the sample handlers use ./config.sh in the deployment folder.

#### libjansson ####
http://www.digip.org/jansson/
https://github.com/akheron/jansson

### Valgrind ###

This software was developed with valgrind in hand.

  valgrind --leak-check=full --gen-suppressions=yes ./mongrel_handler

It throws a real shit fit about "Syscall param socketcall.sendto(msg) points to uninitialised byte(s)" and I have no idea how to work around it. It sounds harmless.

AND I QUOTE (http://markmail.org/message/oqebrtdanawsiz62)
This (common) problem is caused by writing to a socket (or a file) bytes derived from a structure which has alignment padding, which is uninitialised. That is harmless, but the problem is I don't know of a general way to suppress these errors which doesn't also potentially hide real bugs when you mistakenly write uninitialised data to a file/ socket. The best I can suggest is to do a case-by-case suppression yourself (--gen-suppressions=yes is your friend).
END QUOTE

### Sample Handlers ###

From inside of lib run 'make test' to build all the handlers.

Testing body_toupper_handler
I use three terminal sessions:
(1) cd deployment && ./config.sh && ./start.sh # Mongrel2 is up
(2) cd handler && make test && ./body_toupper_handler
(3) curl -x "some post data" http://localhost:6767/
# The curl session should spit back the data but capitalized.

Testing fifo_reader_handler
Now we'll use four sessions
(1) As before
(2) cd handler && make test && ./fifo_reader_handler
(3) curl http://localhost:6767/
(4) cd handler && cat Makefile > handler_pipe
You will see the Makefile in the curl session.

### Contact ###
Feel free to send me through github. Patches are welcome (and how!)! I'm also on mongrel2@librelist.com.

Author: Xavier Lange
License: My code is MIT. My websocket code uses the md5 function from PolarSSL -- that one is GPL.
