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
bstring mongrel2_ws_upgrade_headers(mongrel2_request *req);
bstring mongrel2_ws_upgrade_body(mongrel2_request *req);

int mongrel2_ws_reply(mongrel2_socket *pub_socket, mongrel2_request *req, bstring data);

// For testing the calculations...
int mongrel2_ws_handshake_response(mongrel2_request *req, unsigned char response[16]);
#endif	/* M2WEBSOCKET_H */

