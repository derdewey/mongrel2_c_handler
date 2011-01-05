#include<stdio.h>
#include<zmq.h>

int main(int argc, char **args){
  // Initializing the ZMQ stuff

  // Set to one to allow comm other than ipc
  void* zmq_context = zmq_init(1);
  if(zmq_context == NULL){
    fprintf(stderr, "Could not initialize zmq context");
  }
  
  // Smarts go here
  

  // Tearing down the ZMQ stuff  
  int zmq_term_retval = zmq_term(zmq_context);
  if(zmq_term_retval != 0){
    fprintf(stderr, "Could not terminate zmq context. Lookat errno.");
  }
  return 0;
}
