/* 
 * File:   m2websocket.h
 * Author: xavierlange
 *
 * Created on February 10, 2011, 3:07 PM
 */

#include "stdint.h"

#ifndef M2WEBSOCKET_H
#define	M2WEBSOCKET_H

int mongrel2_ws_reply_upgrade(mongrel2_request *req, mongrel2_socket *socket);
int mongrel2_ws_handshake_response(mongrel2_request *req, unsigned char response[16]);
#endif	/* M2WEBSOCKET_H */
