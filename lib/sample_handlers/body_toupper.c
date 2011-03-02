/**
 * Start up your mongrel2 server
 * Run this program
 * Then test it out using curl
 *
 * Send a file from your current directory
 * curl localhost:6767/body_to_upper_handler -d @m2handler.c -v
 *
 * Send a string from the command line
 * curl localhost:6767/body_to_upper_handler -d "hello handler" -v
 * 
 *
 */
#include "../m2handler.h"
static const struct tagbstring SENDER = bsStatic("82209006-86FF-4982-B5EA-D1E29E55D481");

int main(int argc, char **args){
    bstring pull_addr = bfromcstr("tcp://127.0.0.1:9999");
    bstring pub_addr  = bfromcstr("tcp://127.0.0.1:9998");

    mongrel2_ctx *ctx = mongrel2_init(1); // Yes for threads?

    mongrel2_socket *pull_socket = mongrel2_pull_socket(ctx,bdata(&SENDER));
    mongrel2_connect(pull_socket, bdata(pull_addr));

    mongrel2_socket *pub_socket = mongrel2_pub_socket(ctx);
    mongrel2_connect(pub_socket, bdata(pub_addr));

    const bstring headers = bfromcstr("HTTP/1.1 200 OK\r\nDate: Fri, 07 Jan 2011 01:15:42 GMT\r\nStatus: 200 OK\r\nConnection: close");
    mongrel2_request *request;

    while(1){
        request = mongrel2_recv(pull_socket);
        btoupper(request->body);
        mongrel2_reply_http(pub_socket, request, headers, request->body);
        mongrel2_disconnect(pub_socket, request);
        mongrel2_request_finalize(request);
    }

    bdestroy(headers);

    bdestroy(pull_addr);
    bdestroy(pub_addr);

    mongrel2_close(pull_socket);
    mongrel2_close(pub_socket);
    mongrel2_deinit(ctx);
    return 0;
}
