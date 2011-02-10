Mongrel2 C Handler: libm2handler
--------------------------------

A C library for handling requests from Mongrel2. Includes a suite of sample handlers to get you up and running.

### Prequisites ###

#### mongrel2 ####
http://www.mongrel2.org/
Installed and configured to your liking. If you are trying out the sample handlers use ./config.sh in the deployment folder.

#### libjson0 / libjson0-dev ####
http://oss.metaparadigm.com/json-c/

### Parsing Messages From Mongrel2 ###
Mongrel talks to handlers use zmq_msg_t's. You will be sending and receiving these over the course of your task. A zmq_mst_t has a opaque data field where your webserver-related info comes in -- you must parse the data field according the to the mongrel2 format (aka, a protocol on top of zmq).

My C handler will parse for you and return a mongrel2_request_t. This has all the info you'll need to get going.

Fields that may need explanation:
UUID: The name of the mongrel2 instance that will be helping you out today. 128 bit number in hex with '-' thrown in. Always 36 characters.
CONN_ID: Requests get an ID, a token to keep you and mongrel on the same page. Monotonic, always increasing int starts from 0.
PATH: URL you know and love. Hopefully that shit is escaped!
HEADERS: JSON dictionary
BODY: An opaque buffer

Spaces and commas are literal elements of the protocol. ZMQ message format for incoming requests:

UUID CONN_ID PATH HEADER_NETSTRING,BODY_NETSTRING,

Netstrings are defined as:
STRINGIFEID_LENGTH_OF_DATA:DATA
I sscanf the length into a uint32_t, allocate a buffer of that size, and put opaque_data into it.

As my friend David says, they're like ASN.1 but easier.

== Sending response to Mongrel2 ==
Simpler than parsing the message.
STR_CONN_LIST: 1-128 CONN_ID(s) seperated by a space. Ex: "0 2 3"
STR_CONN_LIST_LENGTH: For STR_CONN_LIST example you put 5
CONN_NETSTRING: Expands as "STR_CONN_LIST_LENGTH:STR_CONN_LIST", where ':' is literal.
OPAQUE_RESPONSE: Whatever you want, mongrel won't touch it

UUID CONN_NETSTRING, OPAQUE_RESPONSE

Reasoning for a CONN_NETSTRING: Allow a single handler message to go to multiple clients. Web multicast made easy? Dunno, could be handy with websocket-y stuff.

Disconnects are done by sending all the same info except you have a zero length OPAQUE_RESPONSE. But don't forget that trailing space!

== Large messages ==
I think you'll have to use ZMQ's getsockopt to see if there are more messages related to what you're processing. Large requests will be split up into multiple messages. No support for that just yet.

== Valgrind ==

This software was developed with valgrind in hand.

  valgrind --leak-check=full --gen-suppressions=yes ./mongrel_handler

It throws a real shit fit about "Syscall param socketcall.sendto(msg) points to uninitialised byte(s)" and I have no idea how to work around it. It sounds harmless.

AND I QUOTE (http://markmail.org/message/oqebrtdanawsiz62)
This (common) problem is caused by writing to a socket (or a file) bytes derived from a structure which has alignment padding, which is uninitialised. That is harmless, but the problem is I don't know of a general way to suppress these errors which doesn't also potentially hide real bugs when you mistakenly write uninitialised data to a file/ socket. The best I can suggest is to do a case-by-case suppression yourself (--gen-suppressions=yes is your friend).
END QUOTE

== Testing sample program ==

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

== bstring ==

I wasn't sure how to put bstrings into the library and still keep the header files seperate, etc. So for now, copy the header files from in here into your own project and make sure they can be found. The object files will be satisfied by libm2hander.a.

== Contact ==
Feel free to send me through github. Patches are welcome (and how!)!

Author: Xavier Lange
License: Whatever mongrel2 uses. MIT right now.
