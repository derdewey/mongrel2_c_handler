/**
 * Start up your mongrel2 server
 * Run this program
 * Then test it out using Chrome 10 or higher
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "../m2handler.h"
#include "../m2websocket.h"

// Static function definitions
static const struct tagbstring SENDER = bsStatic("82209006-86FF-4982-B5EA-D1E29E55D483");
static void *conn_handler(void *nothing);

// Shared variables
static mongrel2_socket *pub_socket;

int main(int argc, char **args){
    bstring pull_addr = bfromcstr("tcp://127.0.0.1:7999");
    bstring pub_addr  = bfromcstr("tcp://127.0.0.1:7998");

    mongrel2_ctx *ctx = mongrel2_init(1); // Yes for threads?

    mongrel2_socket *pull_socket = mongrel2_pull_socket(ctx,bdata(&SENDER));
    mongrel2_connect(pull_socket, bdata(pull_addr));

    pub_socket = mongrel2_pub_socket(ctx);
    mongrel2_connect(pub_socket, bdata(pub_addr));

    mongrel2_request *request;

    pthread_t conn_thread;
    int pthread_retval;
    while((request = mongrel2_recv(pull_socket))!=NULL){
        pthread_retval = pthread_create(&conn_thread, NULL, conn_handler, (void*)request);
    }

    bdestroy(pull_addr);
    bdestroy(pub_addr);

    mongrel2_close(pull_socket);
    mongrel2_close(pub_socket);
    mongrel2_deinit(ctx);
    return 0;
}

static void *conn_handler(void *arg){
    mongrel2_request *req = (mongrel2_request*)arg;
    fprintf(stdout,"\nTHREAD SPAWNED TO HANDLE %d\nSending handshake\n",req->conn_id);
    mongrel2_ws_reply_upgrade(req,pub_socket);
    bstring msg = bfromcstr("{\"msg\" : \"hi\"}");
    mongrel2_ws_reply(pub_socket,req,msg);
    bdestroy(msg);

    // mongrel2_disconnect(pub_socket,req);
    pthread_exit(NULL);
}