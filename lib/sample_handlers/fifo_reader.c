/**
 * Start up your mongrel2 server
 * Run this program
 * Then test it out using curl.
 *
 * Loops on read, does not close initial request, just ctrl-c it to stop.
 *
 *
 * curl localhost:6767/fifo_reader_handler -d "hello handler" -v
 *
 *
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

#include "../m2handler.h"
static const struct tagbstring SENDER = bsStatic("82209006-86FF-4982-B5EA-D1E29E55D482");

int main(int argc, char **args){
    bstring pull_addr = bfromcstr("tcp://127.0.0.1:8999");
    bstring pub_addr  = bfromcstr("tcp://127.0.0.1:8998");

    mongrel2_ctx *ctx = mongrel2_init(1); // Yes for threads?

    mongrel2_socket *pull_socket = mongrel2_pull_socket(ctx,bdata(&SENDER));
    mongrel2_connect(pull_socket, bdata(pull_addr));

    mongrel2_socket *pub_socket = mongrel2_pub_socket(ctx);
    mongrel2_connect(pub_socket, bdata(pub_addr));

    const bstring headers = bfromcstr("HTTP/1.1 200 OK\r\nDate: Fri, 07 Jan 2011 01:15:42 GMT\r\nStatus: 200 OK\r\nConnection: close");
    mongrel2_request *request;

    // Need to set umask to allow file creation... then restore? Not sure!
    mode_t process_mask = umask(0);
    int retval;
    retval = mkfifo("handler_pipe", S_IRUSR);
    umask(process_mask);
    if(retval != 0){
        switch(errno){
            case EACCES :
                fprintf(stderr,"Insufficient permissions to create fifo handler_pipe");
            case EEXIST :
                //fprintf(stderr,"handler_pipe already exists. That's cool tho.");
                goto no_mkfifo_error;
            case ENAMETOOLONG :
                fprintf(stderr,"handler_pipe is too long of a name. Unlikely!");
                break;
            case ENOENT :
                fprintf(stderr,"path for handler_pipe does not exist");
                break;
            case ENOSPC :
                fprintf(stderr,"no more room for handler_pipe");
                break;
            case ENOTDIR :
                fprintf(stderr,"path for handler_pipe is not a directory");
                break;
            case EROFS :
                fprintf(stderr,"handler_pipe cannot be created on read only fs");
                break;
            default:
                fprintf(stderr,"Error creating fifo. Not sure what though. Sorry! %d",errno);
                break;
        }
        exit(EXIT_FAILURE);
    }
    no_mkfifo_error:
    fprintf(stdout,"Created handler_pipe AOK\n");
    
    FILE *fifofd = fopen("handler_pipe","r");
    if(fifofd == NULL){
        fprintf(stderr,"Could not open handler_pipe");
        exit(EXIT_FAILURE);
    }

    size_t read_size;
    request = mongrel2_recv(pull_socket);
    // A 1k buffer
    void* fifo_buffer = calloc(1024,1);
    //while(1){
        read_size = fread(fifo_buffer, 1024, 1, fifofd);
        fprintf(stdout,"read_size from fifo: %zd", read_size); // z is for size_t's... weird!
        bdestroy(request->body);
        request->body = bfromcstralloc(1024,(const char*)fifo_buffer);
        mongrel2_reply_http(pub_socket, request, headers, request->body);
        mongrel2_disconnect(pub_socket, request);
    //}
    mongrel2_request_finalize(request);

    bdestroy(headers);

    bdestroy(pull_addr);
    bdestroy(pub_addr);

    mongrel2_close(pull_socket);
    mongrel2_close(pub_socket);
    mongrel2_deinit(ctx);
    return 0;
}
