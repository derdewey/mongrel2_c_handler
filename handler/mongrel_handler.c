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
#include<stdint.h>
#include<zmq.h>
#include<assert.h>

#define DEBUG

#define SENDER_ID "82209006-86FF-4982-B5EA-D1E29E55D481"
const char *RESPONSE_FORMAT = "%s %d:%d, %s";
// const char *CLOSE_FORMAT = "%s %d:%d, ";
const char *CLOSE_FORMAT = "\0";

void zmq_dummy_free(void *data, void *hint){
    free(data);
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
    char uuid[37];
    int conn_id;
    char path[2048];
    int headers_len;
    int body_len;
    char headers[2048];
    char body[2048];
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
 * Really awful hand-made parser for netstrings. Must modify to copy string contents!
 *
 * Will only work for small requests, although structure is generous. Beware!
 * @param netstring
 * @return
 */
mongrel2_request *mongrel2_parse_netstring(const char* netstring){
  #ifdef DEBUG
  fprintf(stdout, "======NETSTRING======\n");
  fprintf(stdout, "%s\n",netstring);
  fprintf(stdout, "=====================\n");
  #endif

  mongrel2_request* req = calloc(1, sizeof(mongrel2_request));
  if(req == NULL){
    fprintf(stderr,"Could not allocate mongrel2_request");
    exit(EXIT_FAILURE);
  }

  sscanf(netstring,"%s %d %s %d",req->uuid, &req->conn_id, req->path, &req->headers_len);

  // Am I allowe to do this? Will the C police write me up?
  char* cursor = (char*)netstring;
  cursor = strchr(cursor, ' '); // over UUID
  ++cursor; // consume the space
  cursor = strchr(cursor, ' '); // over SeqID
  ++cursor; // consume the space
  cursor = strchr(cursor, ' '); // over Path
  ++cursor; // consume the space
  cursor = strchr(cursor, ':'); // over header_len
  ++cursor; // consume the semi-colon

  memcpy(req->headers, cursor, req->headers_len);
  req->headers[req->headers_len] = '\0';
  for(int i=0; i<req->headers_len; i++){   // cursor = cursor + header_len; instead of loop?
      ++cursor;
  }
  cursor = strchr(cursor, ',');
  ++cursor; // consume ,
  sscanf(cursor, "%d", &req->body_len);
  // fprintf(stdout,"Body Length: %d\n",req->body_len);
  cursor = strchr(cursor, ':');
  ++cursor; // consume :
  // req->body = calloc(body_len+1,sizeof(char*));
  memcpy(req->body, cursor, req->body_len);
  // fprintf(stdout,"Body: %s\n",req->body);
  // should figure out assertion for ',' at end of message

  #ifdef DEBUG
  fprintf(stdout,"========PARSE_NETSTRING=========\n");
  fprintf(stdout,"SERVER_UUID: %s\n",req->uuid);
  fprintf(stdout,"CONNECTION_ID: %d\n",req->conn_id);
  fprintf(stdout,"PATH: %s\n",req->path);
  fprintf(stdout,"HEADERS: %s\n",req->headers);
  fprintf(stdout,"================================\n");
  #endif

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
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    zmq_recv(pull_socket->zmq_socket,&msg,0);
    char* raw_request = (char*) zmq_msg_data(&msg);

    mongrel2_request* req = mongrel2_parse_netstring(raw_request);

    zmq_msg_close(&msg);
    return req;
}

void mongrel2_send(mongrel2_socket *pub_socket, mongrel2_request *req, char* headers, char* body){
    zmq_msg_t response;
    char* seperator = "\r\n\r\n";
    // zmq_msg_init(&response);
    int buffer_len = strlen(headers) + strlen(body) + strlen(seperator);
    void* buffer = calloc(buffer_len,sizeof(char));

    zmq_msg_init_data(&response,buffer,buffer_len,zmq_dummy_free,NULL);

    zmq_send(pub_socket->zmq_socket,&response,0);

    zmq_msg_close(&response);
}

// CLEANUP OPERATIONS
void mongrel2_finalize_request(mongrel2_request *req){
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

    char *resp_headers = "HTTP/1.1 200 OK\r\nDate: Fri, 07 Jan 2011 01:15:42 GMT\r\nStatus: 200 OK\r\nLength: 0\r\nConnection: close";
    char *resp_body = "HI!";
    mongrel2_send(pub_socket, request, resp_headers, resp_body);

    
    mongrel2_finalize_request(request);


    
    mongrel2_close(pull_socket);
    mongrel2_close(pub_socket);
    mongrel2_deinit(&ctx);
    return 0;
}

int begone_foul_demon(int argc, char **args){
  int zmq_retval;
  /**
   * SETTING UP ZMQ
   * 1. Context
   * 2. Create socket
   * 3. Connect socket
   * 4. Bind socket
   */
  // Set to 1 to allow comm other than ipc
  void* zmq_context = zmq_init(1);
  if(zmq_context == NULL){
    fprintf(stderr, "Could not initialize zmq context");
    exit(EXIT_FAILURE);
  }
  
  void* pull_socket = zmq_socket(zmq_context,ZMQ_PULL);
  if(pull_socket == NULL){
      fprintf(stderr, "Could not create a ZMQ_PULL socket");
      exit(EXIT_FAILURE);
  }

  zmq_retval = zmq_setsockopt(pull_socket,ZMQ_IDENTITY,SENDER_ID,sizeof(SENDER_ID)-1);
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

  zmq_retval = zmq_connect(pull_socket, "tcp://127.0.0.1:9999");
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

  void* pub_socket = zmq_socket(zmq_context,ZMQ_PUB);
  if(pub_socket == NULL){
      fprintf(stderr, "Could not create pub socket");
      exit(EXIT_FAILURE);
  }
  // Abstract connection stuff into another fuction and refactor!
  zmq_retval = zmq_connect(pub_socket, "tcp://127.0.0.1:9998");
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

  zmq_msg_t msg;
  zmq_msg_init(&msg);
  zmq_recv(pull_socket, &msg, 0);
  size_t msg_size = zmq_msg_size(&msg);
  char* data = (char*)zmq_msg_data(&msg);
  fprintf(stdout, "Got a message of size %d\n", msg_size);
  fprintf(stdout, "Received: %s\n",data);
  
  char uuid[36+1], path[256];
  uint32_t conn_id, header_len, body_len;   // How big can the msg id get? 32-bit or 64-bit?
  
  sscanf(data,"%s %d %s %d",uuid, &conn_id, path, &header_len);
  fprintf(stdout,"%s\n",uuid);
  fprintf(stdout,"%d\n",conn_id);
  fprintf(stdout,"%s\n",path);
  fprintf(stdout,"%d\n",header_len);
  
  char *headers = NULL, *body = NULL;
  headers = calloc(header_len+1,sizeof(char*));

  /**
   * This code will barf if the path has a space in it!
   * Modify to handle escaping. Lameness.
   * Terrible code... whee!
   */
  char* cursor = data;
  cursor = strchr(cursor, ' '); // over UUID
  ++cursor; // consume the space
  cursor = strchr(cursor, ' '); // over SeqID
  ++cursor; // consume the space
  cursor = strchr(cursor, ' '); // over Path
  ++cursor; // consume the space
  cursor = strchr(cursor, ':'); // over header_len
  ++cursor; // consume the semi-colon
  memcpy(headers, cursor, header_len);
  headers[header_len] = '\0';
  fprintf(stdout,"%s\n",headers);
  for(int i=0; i<header_len; i++){   // cursor = cursor + header_len; instead of loop?
      ++cursor;
  }
  cursor = strchr(cursor, ',');
  ++cursor; // consume ,
  sscanf(cursor, "%d", &body_len);
  fprintf(stdout,"Body Length: %d\n",body_len);
  cursor = strchr(cursor, ':');
  ++cursor; // consume :
  body = calloc(body_len+1,sizeof(char*));
  memcpy(body, cursor, body_len);
  fprintf(stdout,"Body: %s\n",body);
  // should figure out assertion for ',' at end of message

  zmq_msg_t response_msg;
  zmq_msg_init(&response_msg);
  char *hello = "HTTP/1.1 200 OK\r\nDate: Fri, 07 Jan 2011 01:15:42 GMT\r\nStatus: 200 OK\r\nLength: 0\r\nConnection: close\r\n\r\nBye";
  //size_t hello_size = strlen(hello);
  char response_str[256];
  char conn_id_str[256];
  snprintf(conn_id_str, 256, "%d",conn_id);
  size_t conn_id_str_len = strlen(conn_id_str);
  snprintf(response_str, 256, RESPONSE_FORMAT, uuid, conn_id_str_len, conn_id, hello);
  size_t buffer_size = strlen(response_str);
  char *response_buffer = calloc(buffer_size,sizeof(char));
  memcpy(response_buffer,response_str,buffer_size);
  fprintf(stdout, "====RESPONSE====\n%s\n",response_buffer);
  zmq_msg_init_data(&response_msg,response_buffer,buffer_size,zmq_dummy_free,NULL);
  zmq_retval = zmq_send(pub_socket,&response_msg,0);
  if(zmq_retval != 0){
      fprintf(stderr, "aww shit. couldn't write");
      exit(EXIT_FAILURE);
  }
  zmq_msg_close(&msg);


  /**
   * Why doesn't this disconnect statement work?
   */
  zmq_msg_t close_msg;
  zmq_msg_init(&close_msg);
  // snprintf(response_str, 256, CLOSE_FORMAT, uuid, conn_id_str_len, conn_id);
  // snprintf(response_str, 256, CLOSE_FORMAT);
  // buffer_size = strlen(response_str);
  // memcpy(response_buffer,response_str,buffer_size);
  // zmq_msg_init_data(&close_msg,(void*)"",0,zmq_dummy_free, NULL);
  // fprintf(stdout,"Sending close statement: '%s'\n", response_str);
  zmq_retval = zmq_send(pub_socket,&close_msg,0);
  if(zmq_retval != 0){
      fprintf(stderr,"could not send close message");
      exit(EXIT_FAILURE);
  }


  // free(headers);
  // free(body);
  // zmq_msg_close(&close_msg); // big error here. Track down later.
  /**
   * TEARING DOWN ZMQ
   */
  if(zmq_close(pub_socket) != 0){
      fprintf(stderr, "Could not close ZMQ_PUB socket");
      exit(EXIT_FAILURE);
  }
  if(zmq_close(pull_socket) != 0){
      fprintf(stderr, "Could not close ZMQ_PULL socket");
      exit(EXIT_FAILURE);
  }

  zmq_retval = zmq_term(zmq_context);
  if(zmq_retval != 0){
    fprintf(stderr, "Could not terminate zmq context. Lookat errno.");
    exit(EXIT_FAILURE);
  }
  return 0;
}
