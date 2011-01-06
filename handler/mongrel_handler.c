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

int main(int argc, char **args){
  int zmq_retval;
  /**
   * SETTING UP ZMQ
   * 1. Context
   * 2. Create socket
   * 3. Connect socket
   * 4. Bind socket
   */
  // Set to one to allow comm other than ipc
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

  uint8_t uuid[16];
  
  zmq_msg_t msg;
  zmq_msg_init(&msg);
  zmq_recv(pull_socket, &msg, 0);
  size_t msg_size = zmq_msg_size(&msg);
  fprintf(stdout, "Got a message of size %d", msg_size);
  memcpy(uuid,zmq_msg_data(&msg),sizeof(uuid));
  fprintf(stdout, "UUID: %x:%x:%x:%x:%x:%x:%x:%x--%x:%x:%x:%x:%x:%x:%x:%x", uuid[0],uuid[1],uuid[2],uuid[3],uuid[4],uuid[5],uuid[6],uuid[7],uuid[8],uuid[9],uuid[10],uuid[11],uuid[12],uuid[13],uuid[14],uuid[15]);

  char super_buffer[1024];
  uint8_t *msg_remaining = zmq_msg_data(&msg); // + sizeof(void*) * sizeof(uuid);
  snprintf(super_buffer, sizeof(super_buffer), "Remaining Message: %s",msg_remaining);
  fprintf(stdout,"%s",super_buffer);

/*
  int64_t more;
  size_t more_size = sizeof(more);
  zmq_retval = zmq_getsockopt(pull_socket, ZMQ_RCVMORE, &more, &more_size);
  if(zmq_retval != 0){
      switch(errno){
          case EINVAL : {
              fprintf(stderr, "option_name is unknown, or option_len or option_value is invalid, or the size of the buffer specified by option_len is insufficient");
              break;
          }
          case ETERM : {
              fprintf(stderr, "zmq context was terminated");
              break;
          }
          case EFAULT : {
              fprintf(stderr, "provided socket is not valid");
          }
      }
      exit(EXIT_FAILURE);
  }
  if(more != 0){
      fprintf(stdout, "More data awaits!");
  }
*/


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
