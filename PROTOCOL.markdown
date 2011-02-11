How Messages From Mongrel2 Are Parsed
-------------------------------------

Mongrel talks to handlers use zmq_msg_t's. You will be sending and receiving these over the course of your task. A zmq_mst_t has a opaque data field where your webserver-related info comes in -- you must parse the data field according the to the mongrel2 format (aka, a protocol on top of zmq).

My C handler will parse for you and return a mongrel2_request_t. This has all the info you'll need to get going.

Fields that may need explanation:
UUID: The name of the mongrel2 instance that will be helping you out today. 128 bit number in hex with '-' thrown in. Always 36 characters.
CONN_ID: Requests get an ID, a token to keep you and mongrel on the same page. Monotonic, always increasing int starts from 0.
PATH: URL you know and love.
HEADERS: JSON object (aka, a dictionary)
BODY: An opaque buffer

How data looks coming in from Mongrel2
--------------------------------------

Spaces and commas are literal elements of the protocol. ZMQ message format for incoming requests:

UUID CONN_ID PATH HEADER_NETSTRING,BODY_NETSTRING,

Netstrings are defined as:

  STRINGIFEID_LENGTH_OF_DATA:DATA

I sscanf the length into a uint32_t, allocate a buffer of that size, and put opaque_data into it.

As my friend David says, they're like ASN.1 but easier.

How data looks going back to Mongrel2
-------------------------------------

Simpler than parsing the message.

STR_CONN_LIST: 1-128 CONN_ID(s) seperated by a space.
 
  Example: "0 2 3"

STR_CONN_LIST_LENGTH: The number of bytes, not entries, in the list.

  For STR_CONN_LIST example: 5

CONN_NETSTRING: Expands as "STR_CONN_LIST_LENGTH:STR_CONN_LIST", where ':' is literal.
OPAQUE_RESPONSE: Whatever you want, mongrel won't touch it

High level description of the byte stream

  UUID CONN_NETSTRING, OPAQUE_RESPONSE

Reasoning for a CONN_NETSTRING: Allow a single handler message to go to multiple clients. Web multicast made easy? Dunno, could be handy with websocket-y stuff.

How disconnection works
-----------------------

Disconnects are remote commands to mongrel2 saying, Hey, close that socket would ya? It is a special case of sending data to mongrel2 it's just that you send no data. Mongrel2 will interpret the empty payload as a CLOSE request. Don't forget the trailing space!

  UUID CONN_NETSTRING, 


