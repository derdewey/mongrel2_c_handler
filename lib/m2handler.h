/**
 Mongrel2 C handler
 * A reimplementation of the code that is in Zed's Mongrel2 code.
 * His libm2 has functionality for management, etc. This code is for
 * understanding the Mongrel2 protocol, for being able to dig into the code
 * from a handler's perspective only. No worrying about tasks, ragel, or whatever.
 *
 * The protocol is simple... so this code should simple, too!
 *
 * Lots of inspiration from the mongrel2 rack handler and the mongrel2 lua handler.
 */

#ifndef MONGREL_HANDLER_H
#define MONGREL_HANDLER_H

#include<zmq.h>
#include<jansson.h>
#include "bstr/bstrlib.h"
#include "bstr/bstraux.h"

struct mongrel2_ctx_t{
    void* zmq_context;
};
typedef struct mongrel2_ctx_t mongrel2_ctx;

struct mongrel2_socket_t{
    void* zmq_socket;
};
typedef struct mongrel2_socket_t mongrel2_socket;

struct mongrel2_request_t{
    bstring uuid;
    int conn_id;
    bstring conn_id_bstr;
    bstring path;
    bstring raw_headers;
    json_t *headers;
    bstring body;
};
typedef struct mongrel2_request_t mongrel2_request;

mongrel2_ctx* mongrel2_init(int threads);
int mongrel2_deinit(mongrel2_ctx *ctx);

int mongrel2_connect(mongrel2_socket* socket, const char* dest);
mongrel2_socket* mongrel2_pull_socket(mongrel2_ctx *ctx, char* identity);
mongrel2_socket* mongrel2_pub_socket(mongrel2_ctx *ctx);
int mongrel2_close(mongrel2_socket *socket);

mongrel2_request *mongrel2_recv(mongrel2_socket *pull_socket);
mongrel2_request *mongrel2_parse_request(bstring raw_mongrel_request);

int mongrel2_send(mongrel2_socket *pub_socket, bstring response);
int mongrel2_reply(mongrel2_socket *pub_socket, mongrel2_request *req, const_bstring payload);
int mongrel2_reply_http(mongrel2_socket *pub_socket, mongrel2_request *req, const_bstring headers, const_bstring body);

int mongrel2_request_for_disconnect(mongrel2_request *req);
int mongrel2_disconnect(mongrel2_socket *pub_socket, mongrel2_request *req);

bstring mongrel2_request_get_header(mongrel2_request *req, char* key);
int mongrel2_request_finalize(mongrel2_request *req);
#endif
