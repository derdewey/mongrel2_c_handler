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

char *SENDER_ID = "82209006-86FF-4982-B5EA-D1E29E55D481";

void zmq_dummy_free(void *data, void *hint){
    free(data);
}

int main(int argc, char **args){
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
  char *hello = "HTTP/1.1 OK 200\r\nBlah\r\n\r\nHey cURL";
  //size_t hello_size = strlen(hello);
  char response_str[256];
  char conn_id_str[256];
  snprintf(conn_id_str, 256, "%d",conn_id);
  size_t conn_id_str_len = strlen(conn_id_str);
  snprintf(response_str, 256, "%s %d:%d, %s", uuid, conn_id_str_len, conn_id, hello);
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

  free(headers);
  free(body);
  zmq_msg_close(&msg);
  /**
   * TEARING DOWN ZMQ
   */
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
