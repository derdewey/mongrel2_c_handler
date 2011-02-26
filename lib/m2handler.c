/**
 * Copyright Xavier Lange 2011
 * Simple handler for bridging C into mongrel2. Hopefully a useful
 * exploration of the simple protcol/pattern to setup plumbing.
 *
 * 
 * ZMQ documentation: http://api.zeromq.org/
 * Mongrel2 documentation: http://mongrel2.org/doc/tip/docs/manual/book.wiki
 *
 */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<zmq.h>
#include<jansson.h>
#include "m2handler.h"
#include "bstr/bstrlib.h"
#include "bstr/bstraux.h"
#include <strings.h>

#define DEBUG

static const struct tagbstring SPACE = bsStatic(" ");
static const struct tagbstring COLON = bsStatic(":");
static const struct tagbstring COMMA = bsStatic(",");
static const struct tagbstring SEPERATOR = bsStatic("\r\n\r\n");
static const struct tagbstring JSON = bsStatic("JSON");
static const struct tagbstring DISCONNECT = bsStatic("disconnect");
static const char *RESPONSE_HEADER = "%s %d:%d, ";

static void zmq_bstr_free(void *data, void *bstr){
    bdestroy(bstr);
}

mongrel2_ctx* mongrel2_init(int threads){
    mongrel2_ctx* ctx = calloc(1,sizeof(mongrel2_ctx));
    ctx->zmq_context = zmq_init(threads);
    if(ctx->zmq_context == NULL){
        fprintf(stderr, "Could not initialize zmq context");
        exit(EXIT_FAILURE);
    }
    return ctx;
}
int mongrel2_deinit(mongrel2_ctx *ctx){
    int zmq_retval = zmq_term(ctx->zmq_context);
    if(zmq_retval != 0){
        fprintf(stderr,"Could not terminate ZMQ context");
        exit(EXIT_FAILURE);
    }
    free(ctx);
    return 0;
}
/**
 * Made static because people don't necessarily need to use it. Thoughts? Should
 * it be publicly accessible?
 * @param ctx
 * @param type
 * @return
 */
static mongrel2_socket* mongrel2_alloc_socket(mongrel2_ctx *ctx, int type){
    mongrel2_socket *ptr = calloc(1,sizeof(mongrel2_socket));
    ptr->zmq_socket = zmq_socket(ctx->zmq_context, type);
    if(ptr == NULL || ptr->zmq_socket == NULL){
        fprintf(stderr, "Could not allocate socket");
        exit(EXIT_FAILURE);
    }
    return ptr;
}
/**
 * Identity is only needed for pull sockets (right?) so I'll only
 * mongrel2_pull_socket will call this.
 * @param ctx
 * @param socket
 * @param identity
 */
static void mongrel2_set_identity(mongrel2_ctx *ctx, mongrel2_socket *socket, const char* identity){
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
int mongrel2_connect(mongrel2_socket* socket, const char* dest){
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
    return 0;
}
/**
 * Honky-dory hand-made parser for mongrel2's request format
 *
 * Will only work for small requests, although structure is generous. Beware!
 *
 * @param netstring
 * @return
 */
mongrel2_request *mongrel2_parse_request(bstring raw_request_bstr){
  #ifdef DEBUG
  fprintf(stdout, "======NETSTRING======\n");
  fprintf(stdout, "%.*s\n",blength(raw_request_bstr),bdata(raw_request_bstr));
  fprintf(stdout, "=====================\n");
  #endif

  mongrel2_request* req = calloc(1, sizeof(mongrel2_request));
  if(req == NULL){
    fprintf(stderr,"Could not allocate mongrel2_request");
    goto error;
  }

  // bfromcstr(raw_mongrel_request);
  int suuid = 0, euuid;
  int sconnid, econnid;
  int spath, epath;
  int sheader, eheader;
  int sbody, ebody;

  // Extract the UUID
  euuid = binchr(raw_request_bstr, suuid, &SPACE);
  req->uuid = bmidstr(raw_request_bstr, suuid, euuid-suuid);
  if(req->uuid == NULL){
      fprintf(stderr,"Could not extract UUID!");
      goto error;
  }

  // Extract the Connection ID
  sconnid = euuid+1; // Skip over the space delimiter
  econnid = binchr(raw_request_bstr, sconnid, &SPACE);
  req->conn_id_bstr = bmidstr(raw_request_bstr,sconnid,econnid-sconnid);
  if(req->conn_id_bstr == NULL){
      fprintf(stderr, "Could not extract connection id");
      goto error;
  }
  sscanf(bdata(req->conn_id_bstr),"%d",&req->conn_id);

  // Extract the Path
  spath = econnid+1; // Skip over the space delimiter
  epath = binchr(raw_request_bstr, spath, &SPACE);
  req->path = bmidstr(raw_request_bstr,spath,epath-spath);
  if(req->path == NULL){
      fprintf(stderr, "Could not extract Path");
      goto error;
  }

  // Extract the headers
  // First we grab the length value as an int
  bstring tempheaderlenbstr;
  sheader = epath+1; // Skip over the space delimiter
  eheader = binchr(raw_request_bstr, sheader, &COLON);
  tempheaderlenbstr = bmidstr(raw_request_bstr,sheader,eheader-sheader);
  int headerlen;
  sscanf(bdata(tempheaderlenbstr),"%d",&headerlen);
  bdestroy(tempheaderlenbstr);

  // Now that we know the header length we can actually extract it
  sheader = eheader+1; // Skip over the number and the colon
  eheader = sheader+headerlen;
  req->raw_headers = bmidstr(raw_request_bstr,sheader,eheader-sheader);
  if(req->raw_headers == NULL){
      fprintf(stderr,"could not extract headers");
      goto error;
  } else if(blength(req->raw_headers) != headerlen){
      fprintf(stderr,"Expected headerlen to be %d, got %d",headerlen,blength(req->raw_headers));
      goto error;
  }

  // Extract the body
  // First we grab the length value as an int
  bstring tempbodylenbstr;
  sbody = eheader+1; // Skip over the comma
  ebody = binchr(raw_request_bstr,sbody,&COLON);
  tempbodylenbstr = bmidstr(raw_request_bstr,sbody,ebody-sbody);
  int bodylen;
  sscanf(bdata(tempbodylenbstr),"%d",&bodylen);
  bdestroy(tempbodylenbstr);

  // Nowe we have the body len we can extract the payload
  sbody = ebody+1; // Skip over the number and the colon
  ebody = sbody+bodylen;
  req->body = bmidstr(raw_request_bstr,sbody,ebody-sbody);
  if(req->body == NULL){
      fprintf(stderr,"could not extract body");
      goto error;
  } else if(blength(req->body) != bodylen){
      fprintf(stderr,"Expected body to be %d, got %d",bodylen,blength(req->body));
      goto error;
  }

  #ifdef DEBUG
  fprintf(stdout,"========PARSE_NETSTRING=========\n");
  fprintf(stdout,"SERVER_UUID: %s\n",bdata(req->uuid));
  fprintf(stdout,"CONNECTION_ID: %d\n",req->conn_id);
  fprintf(stdout,"PATH: %s\n",bdata(req->path));
  fprintf(stdout,"HEADERS: %s\n",bdata(req->raw_headers));
  fprintf(stdout,"BODY: %s\n",bdata(req->body));
  fprintf(stdout,"================================\n");
  #endif

  // TODO: error situations here?
  json_error_t header_err;
  req->headers = json_loads(bdata(req->raw_headers),0,&header_err);// json_string(bdata(req->raw_headers));
  if(req->headers == NULL){
      fprintf(stderr,"Problem parsing the inputs near position: %d, line: %d, col: %d",header_err.position,header_err.line,header_err.position);
      goto error;
  }
  if(json_typeof(req->headers) != JSON_OBJECT){
    fprintf(stderr, "Headers did not turn into an object... ruh roh!");
  }

  bdestroy(raw_request_bstr);
  return req;

  error:
  bdestroy(raw_request_bstr);
  mongrel2_request_finalize(req);
  return NULL;
}

/**
 * Valgrind thinks there's a memory leak happening from here...
 * A lot of 'syscall param socketcall.send(msg) point to unitialized byte(s)'
 * throughout my code. TBD!
 * @param pull_socket
 * @return 0 on success, -1 on failure. If failure, disconnect host and finalize.
 */
mongrel2_request *mongrel2_recv(mongrel2_socket *pull_socket){
    zmq_msg_t *msg = calloc(1,sizeof(zmq_msg_t));
    zmq_msg_init(msg);
    zmq_recv(pull_socket->zmq_socket,msg,0);
    
    bstring raw_request_bstr = blk2bstr(zmq_msg_data(msg),zmq_msg_size(msg));
    mongrel2_request *req = mongrel2_parse_request(raw_request_bstr);

    zmq_msg_close(msg);
    free(msg);

    return req;
}

/**
 * The most raw interface to the Mongrel2 webserver. Takes ownership of response.
 * @param pub_socket
 * @param response_buff
 */
int mongrel2_send(mongrel2_socket *pub_socket, bstring response){
    zmq_msg_t *msg = calloc(1,sizeof(zmq_msg_t));

    /**
     * zmq_bstr_free was calling free on bdata(response) which was wrong!
     * It needs to be called on the bstring itself. So that gets passed
     * in as the hint. Whew.
     */
    zmq_msg_init_data(msg,bdata(response),blength(response),zmq_bstr_free,response);

    zmq_send(pub_socket->zmq_socket,msg,0);
    zmq_msg_close(msg);
    free(msg);

    #ifdef DEBUG
    fprintf(stdout,"=======MONGREL2_SEND==========\n");
    fprintf(stdout,"''%.*s''\n",blength(response),bdata(response));
    fprintf(stdout,"==============================\n");
    #endif
    return 0;
}
/**
 * Convenience method for when you have headers and a body.
 * @param pub_socket
 * @param req
 * @param headers
 * @param body
 * @return
 */
int mongrel2_reply_http(mongrel2_socket *pub_socket, mongrel2_request *req, const_bstring headers, const_bstring body){
    bstring payload = bstrcpy(headers);
    bconcat(payload, &SEPERATOR);
    bconcat(payload,body);
    // mongrel2_send(pub_socket,response);
    int retval = mongrel2_reply(pub_socket, req, payload);
    bdestroy(payload);
    return retval;
}
/**
 * The big kahuna. Formats protocol message to mongrel2 and sends along your payload.
 * Does not take ownership of payload.
 * @param pub_socket
 * @param req
 * @param payload
 * @return
 */
int mongrel2_reply(mongrel2_socket *pub_socket, mongrel2_request *req, const_bstring payload){
    bstring response = bformat(RESPONSE_HEADER,bdata(req->uuid),blength(req->conn_id_bstr),req->conn_id);
    bconcat(response,payload);
    return mongrel2_send(pub_socket,response);
}

int mongrel2_request_for_disconnect(mongrel2_request *req){
    bstring header = NULL;
    json_t *json_body  = NULL;
    json_t *method_obj = NULL;
    const char *method_str  = NULL;

    const char* body_str = bdata(req->body);
    json_error_t jerr;
    json_body = json_loads(body_str,0,&jerr);
    if(json_body == NULL || !json_is_object(json_body)){
        json_decref(json_body);
        bdestroy(header);
        return 0;
    }

    method_obj = json_object_get(json_body,"type");
    if(method_obj == NULL){
        json_decref(json_body);
        bdestroy(header);
        return 0;
    }

    method_str = json_string_value(method_obj);
    bstring method_bstr = bfromcstr(method_str);
    json_decref(method_obj);
    json_decref(json_body);
    
    if(method_obj == NULL || bstrcmp(method_bstr,&DISCONNECT) != 0){
        bdestroy(method_bstr);
        json_decref(method_obj);
        json_decref(json_body);
        bdestroy(header);
        return 0;
    }
    bdestroy(method_bstr);

    json_decref(method_obj);
    json_decref(json_body);
    bdestroy(header);
    return 1;
}

/**
 * Convenience method for sending a close request. Essentially a mongrel2_reply with an empty payload.
 * @param pub_socket
 * @param req
 * @return
 */
int mongrel2_disconnect(mongrel2_socket *pub_socket, mongrel2_request *req){
    if(req == NULL){
        fprintf(stderr,"mongrel2_disconnect called with NULL pub_socket");
        return -1;
    } else if (pub_socket == NULL){
        fprintf(stderr,"mongrel2_disconnect called with NULL request");
        return -1;
    }
    bstring close = bfromcstr("");
    return mongrel2_reply(pub_socket,req,close);
}

/**
 * Returns a copy of the header value. You must free this yourself.
 * @param req
 * @param key
 * @return
 */
bstring mongrel2_request_get_header(mongrel2_request *req, char* key){
    json_t *header_val_obj = json_object_get(req->headers,key);
    const char* val_str = json_string_value(header_val_obj);
    bstring retval = bfromcstr((char*)val_str);
    json_decref(header_val_obj);

    return retval;
}

// CLEANUP OPERATIONS
int mongrel2_request_finalize(mongrel2_request *req){
    bdestroy(req->body);
    bdestroy(req->raw_headers);
    bdestroy(req->path);
    bdestroy(req->uuid);
    bdestroy(req->conn_id_bstr);
    json_decref(req->headers);
    free(req);
    return 0;
}
int mongrel2_close(mongrel2_socket *socket){
    if(socket == NULL || socket->zmq_socket == NULL){
        fprintf(stderr, "called mongrel2_close on something weird");
        exit(EXIT_FAILURE);
    }
    zmq_close(socket->zmq_socket);
    free(socket);
    return 0;
}
