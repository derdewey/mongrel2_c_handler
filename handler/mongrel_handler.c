/**
 * Copyright Xavier Lange 2011
 * Simple handler for bridging C into mongrel2. Hopefully a useful
 * exploration of the simple protcol/pattern to setup plumbing.
 *
 * 
 * ZMQ documentation: http://api.zeromq.org/
 * Mongrel2 documentation: http://mongrel2.org/doc/tip/docs/manual/book.wiki
 */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<zmq.h>
#include "bstr/bstrlib.h"
#include "bstr/bstraux.h"

#define DEBUG
#define SENDER_ID "82209006-86FF-4982-B5EA-D1E29E55D481"

const struct tagbstring SPACE = bsStatic(" ");
const struct tagbstring COLON = bsStatic(":");
const struct tagbstring COMMA = bsStatic(",");
const struct tagbstring SEPERATOR = bsStatic("\r\n\r\n");

const char *RESPONSE_HEADER = "%s %d:%d, ";

void zmq_bstr_free(void *data, void *hint){
    fprintf(stdout,"zmq_bstr_free got called with %d\n",(int)data);
    bdestroy(data);
}

struct mongrel2_ctx_t{
    void* zmq_context;
};
typedef struct mongrel2_ctx_t mongrel2_ctx;

struct mongrel2_socket_t{
    void* zmq_socket;
};
typedef struct mongrel2_socket_t mongrel2_socket;

// sscanf(data,"%s %d %s %d",uuid, &conn_id, path, &header_len);
struct mongrel2_request_t{
    bstring uuid;
    int conn_id;
    bstring conn_id_bstr;
    bstring path;
    int body_len;
    bstring headers;
    bstring body;
};
typedef struct mongrel2_request_t mongrel2_request;

void mongrel2_init(mongrel2_ctx *ctx){
    ctx->zmq_context = zmq_init(1);
    if(ctx->zmq_context == NULL){
        fprintf(stderr, "Could not initialize zmq context");
        exit(EXIT_FAILURE);
    }
}

// SETUP FUNCTIONS
mongrel2_socket* mongrel2_alloc_socket(mongrel2_ctx *ctx, int type){
    mongrel2_socket *ptr = calloc(1,sizeof(mongrel2_socket));
    ptr->zmq_socket = zmq_socket(ctx->zmq_context, type);
    if(ptr == NULL || ptr->zmq_socket == NULL){
        fprintf(stderr, "Could not allocate socket");
        exit(EXIT_FAILURE);
    }
    return ptr;
}
void mongrel2_set_identity(mongrel2_ctx *ctx, mongrel2_socket *socket, const char* identity){
    int zmq_retval = zmq_setsockopt(socket->zmq_socket,ZMQ_IDENTITY,identity,strlen(identity));
    if(zmq_retval != 0){
      switch(errno){
          case EINVAL : {
              fprintf(stderr, "Unknown setsockopt property");
              break;
          }
          case ETERM : {
              fprintf(stderr, "ZMQ context already terminated");
              break;
          }
          case EFAULT : {
              fprintf(stderr, "Socket provided was not valid");
              break;
          }
      }
      exit(EXIT_FAILURE);
    }
}
mongrel2_socket* mongrel2_pull_socket(mongrel2_ctx *ctx, char* identity){
    mongrel2_socket *socket;
    socket = mongrel2_alloc_socket(ctx,ZMQ_PULL);

    mongrel2_set_identity(ctx,socket,identity);

    return socket;
}
mongrel2_socket* mongrel2_pub_socket(mongrel2_ctx *ctx){
    mongrel2_socket *socket;
    socket = mongrel2_alloc_socket(ctx,ZMQ_PUB);
    return socket;
}
void mongrel2_connect(mongrel2_socket* socket, const char* dest){
    int zmq_retval;
    zmq_retval = zmq_connect(socket->zmq_socket, dest);
    if(zmq_retval != 0){
      switch(errno){
          case EPROTONOSUPPORT : {
              fprintf(stderr, "Protocol not supported");
              break;
          }
          case ENOCOMPATPROTO : {
              fprintf(stderr, "Protocol not compatible with socket type");
              break;
          }
          case ETERM : {
              fprintf(stderr, "ZMQ context has already been terminated");
              break;
          }
          case EFAULT : {
              fprintf(stderr, "A NULL socket was provided");
              break;
          }
      }
      exit(EXIT_FAILURE);
    }
    return;
}

// RECEIVE OPERATIONS
/**
 * Honky-dory hand-made parser for mongrel2's request format
 *
 * Will only work for small requests, although structure is generous. Beware!
 * @param netstring
 * @return
 */
mongrel2_request *mongrel2_parse_request(const char* raw_mongrel_request){
  #ifdef DEBUG
  fprintf(stdout, "======NETSTRING======\n");
  fprintf(stdout, "%s\n",raw_mongrel_request);
  fprintf(stdout, "=====================\n");
  #endif

  mongrel2_request* req = calloc(1, sizeof(mongrel2_request));
  if(req == NULL){
    fprintf(stderr,"Could not allocate mongrel2_request");
    exit(EXIT_FAILURE);
  }

  bstring bnetstring = bfromcstr(raw_mongrel_request);
  int suuid = 0, euuid;
  int sconnid, econnid;
  int spath, epath;
  int sheader, eheader;
  int sbody, ebody;

  // Extract the UUID
  euuid = binchr(bnetstring, suuid, &SPACE);
  req->uuid = bmidstr(bnetstring, suuid, euuid-suuid);
  if(req->uuid == NULL){
      fprintf(stderr,"Could not extract UUID!");
      exit(EXIT_FAILURE);
  }

  // Extract the Connection ID
  sconnid = euuid+1; // Skip over the space delimiter
  econnid = binchr(bnetstring, sconnid, &SPACE);
  req->conn_id_bstr = bmidstr(bnetstring,sconnid,econnid-sconnid);
  if(req->conn_id_bstr == NULL){
      fprintf(stderr, "Could not extract connection id");
  }
  sscanf(bdata(req->conn_id_bstr),"%d",&req->conn_id);

  // Extract the Path
  spath = econnid+1; // Skip over the space delimiter
  epath = binchr(bnetstring, spath, &SPACE);
  req->path = bmidstr(bnetstring,spath,epath-spath);
  if(req->path == NULL){
      fprintf(stderr, "Could not extract Path");
      exit(EXIT_FAILURE);
  }

  // Extract the headers
  // First we grab the length value as an int
  bstring tempheaderlenbstr;
  sheader = epath+1; // Skip over the space delimiter
  eheader = binchr(bnetstring, sheader, &COLON);
  tempheaderlenbstr = bmidstr(bnetstring,sheader,eheader-sheader);
  int headerlen;
  sscanf(bdata(tempheaderlenbstr),"%d",&headerlen);
  bdestroy(tempheaderlenbstr);

  // Now that we know the header length we can actually extract it
  sheader = eheader+1; // Skip over the number and the colon
  eheader = sheader+headerlen;
  req->headers = bmidstr(bnetstring,sheader,eheader-sheader);
  if(req->headers == NULL){
      fprintf(stderr,"could not extract headers");
      exit(EXIT_FAILURE);
  } else if(blength(req->headers) != headerlen){
      fprintf(stderr,"Expected headerlen to be %d, got %d",headerlen,blength(req->headers));
      exit(EXIT_FAILURE);
  }

  // Extract the body
  // First we grab the length value as an int
  bstring tempbodylenbstr;
  sbody = eheader+1; // Skip over the comma
  ebody = binchr(bnetstring,sbody,&COLON);
  tempbodylenbstr = bmidstr(bnetstring,sbody,ebody-sbody);
  int bodylen;
  sscanf(bdata(tempbodylenbstr),"%d",&bodylen);
  bdestroy(tempbodylenbstr);

  // Nowe we have the body len we can extract the payload
  sbody = ebody+1; // Skip over the number and the colon
  ebody = sbody+bodylen;
  req->body = bmidstr(bnetstring,sbody,ebody-sbody);
  if(req->body == NULL){
      fprintf(stderr,"could not extract body");
      exit(EXIT_FAILURE);
  } else if(blength(req->body) != bodylen){
      fprintf(stderr,"Expected body to be %d, got %d",bodylen,blength(req->body));
      exit(EXIT_FAILURE);
  }

  #ifdef DEBUG
  fprintf(stdout,"========PARSE_NETSTRING=========\n");
  fprintf(stdout,"SERVER_UUID: %s\n",bdata(req->uuid));
  fprintf(stdout,"CONNECTION_ID: %d\n",req->conn_id);
  fprintf(stdout,"PATH: %s\n",bdata(req->path));
  fprintf(stdout,"HEADERS: %s\n",bdata(req->headers));
  fprintf(stdout,"================================\n");
  #endif

  bdestroy(bnetstring);
  return req;
}

/**
 * Valgrind thinks there's a memory leak happening from here...
 * A lot of 'syscall param socketcall.send(msg) point to unitialized byte(s)'
 * throughout my code. TBD!
 * @param pull_socket
 * @return
 */
mongrel2_request *mongrel2_recv(mongrel2_socket *pull_socket){
    zmq_msg_t *msg = calloc(1,sizeof(zmq_msg_t));
    zmq_msg_init(msg);
    zmq_recv(pull_socket->zmq_socket,msg,0);
    char* raw_request = (char*) zmq_msg_data(msg);

    mongrel2_request* req = mongrel2_parse_request(raw_request);

    zmq_msg_close(msg);
    free(msg);

    return req;
}

/**
 * Removed the mongrel2_request as a param from here. That info is encoded in
 * the response buff. Bad idea?
 * @param pub_socket
 * @param response_buff
 */
void mongrel2_send(mongrel2_socket *pub_socket, bstring response){
    zmq_msg_t zmq_response;
    zmq_msg_init_data(&zmq_response,bdata(response),blength(response),zmq_bstr_free,NULL);

    zmq_send(pub_socket->zmq_socket,&zmq_response,0);

    #ifdef DEBUG
    fprintf(stdout,"=======MONGREL2_SEND==========\n");
    fprintf(stdout,"Sending: ''%s''\n",bdata(response));
    fprintf(stdout,"==============================\n");
    #endif

    zmq_msg_close(&zmq_response);
}
void mongrel2_reply_http(mongrel2_socket *pub_socket, mongrel2_request *req, const_bstring headers, const_bstring body){
    // All the info except headers and body
    bstring response = bformat(RESPONSE_HEADER,bdata(req->uuid),blength(req->conn_id_bstr),req->conn_id);

    // Now tack on headers and body
    bconcat(response,headers);
    bconcat(response,&SEPERATOR);
    bconcat(response,body);

    mongrel2_send(pub_socket,response);
    //bdestroy(response);
}
void mongrel2_disconnect(mongrel2_socket *pub_socket, mongrel2_request *req){
    bstring response = bformat(RESPONSE_HEADER,bdata(req->uuid),blength(req->conn_id_bstr),req->conn_id);
    mongrel2_send(pub_socket,response);
    bdestroy(response);
}

// CLEANUP OPERATIONS
void mongrel2_request_finalize(mongrel2_request *req){
    bdestroy(req->body);
    bdestroy(req->headers);
    bdestroy(req->path);
    bdestroy(req->uuid);
    bdestroy(req->conn_id_bstr);
    free(req);
    return;
}
void mongrel2_close(mongrel2_socket *socket){
    zmq_close(socket->zmq_socket);
    free(socket);
}
void mongrel2_deinit(mongrel2_ctx *ctx){
    int zmq_retval = zmq_term(ctx->zmq_context);
    if(zmq_retval != 0){
        fprintf(stderr,"Could not terminate ZMQ context");
        exit(EXIT_FAILURE);
    }
    return;
}

int main(int argc, char **args){
    mongrel2_ctx ctx;
    mongrel2_init(&ctx);

    mongrel2_socket *pull_socket = mongrel2_pull_socket(&ctx,SENDER_ID);
    mongrel2_connect(pull_socket,"tcp://127.0.0.1:9999");
    
    mongrel2_socket *pub_socket = mongrel2_pub_socket(&ctx);
    mongrel2_connect(pub_socket,"tcp://127.0.0.1:9998");

    mongrel2_request *request;
    request = mongrel2_recv(pull_socket);

    const bstring headers = bfromcstr("HTTP/1.1 200 OK\r\nDate: Fri, 07 Jan 2011 01:15:42 GMT\r\nStatus: 200 OK\r\nConnection: close");
    // const bstring body = bfromcstr("HI!");

    mongrel2_reply_http(pub_socket, request, headers, request->body);
    bdestroy(headers);
    //bdestroy(body);

    mongrel2_disconnect(pub_socket, request);
    mongrel2_request_finalize(request);
    
    mongrel2_close(pull_socket);
    mongrel2_close(pub_socket);
    mongrel2_deinit(&ctx);
    return 0;
}