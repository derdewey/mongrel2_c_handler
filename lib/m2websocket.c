/*
 * File:   m2websocket.c
 * Author: xavierlange
 *
 * Created on February 10, 2011, 3:07 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include "m2handler.h"
#include "m2websocket.h"
#include "md5/md5.h"

#define START_CHAR (unsigned char)0x00
#define TERM_CHAR (unsigned char)0xFF

static uint32_t mongrel2_ws_concat_numbers(const char *incoming);
static uint32_t mongrel2_ws_count_spaces(const char *incoming);
static int mongrel2_ws_extract_seckey(const char* seckey_str, uint32_t *seckey);
static int mongrel2_ws_calculate_response(uint32_t seckey1, uint32_t seckey2, bstring body, unsigned char md5output[16]);

static const char* UPGRADE =
    "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Location: %s\r\n"
    "Sec-WebSocket-Origin: %s\r\n"
    "Sec-WebSocket-Protocol: Sample";

int mongrel2_ws_reply_upgrade(mongrel2_request *req, mongrel2_socket *socket){
    bstring headers = mongrel2_ws_upgrade_headers(req);
    bstring body = mongrel2_ws_upgrade_body(req);
    mongrel2_reply_http(socket,req,headers,body);
    bdestroy(headers);
    bdestroy(body);
    return 0;
}

bstring mongrel2_ws_upgrade_headers(mongrel2_request *req){
    bstring origin = mongrel2_request_get_header(req,"origin");
    bstring path   = mongrel2_request_get_header(req,"PATH");
    bstring host   = mongrel2_request_get_header(req,"host");

    //bstring location = bfromcstr("ws://localhost:6767/ws_handler");
    //bstring origin = bfromcstr("http://localhost:6767");
    bstring location = bformat("ws://%s%s",bdata(host),bdata(path));
    fprintf(stdout,"LOCATION: %s", bdata(location));
    bstring headers = bformat(UPGRADE,bdata(location),bdata(origin));

    fprintf(stdout,"Headers: %s\n",bdata(req->raw_headers));
    fprintf(stdout,"Body: %s\n",bdata(req->body));
    fprintf(stdout,"Challenge Response: %s\n",bdata(headers));

    bdestroy(location);
    bdestroy(host);
    bdestroy(path);
    bdestroy(origin);

    return headers;
}

bstring mongrel2_ws_upgrade_body(mongrel2_request *req){
    unsigned char raw_response[16];
    mongrel2_ws_handshake_response(req,raw_response);
    fprintf(stdout,"Response: %s\n",raw_response);
    // TODO: I assume this copies the content over!
    return blk2bstr(raw_response,16);
}

void mongrel2_ws_debug(bstring data){
    if(data == NULL){
        fprintf(stderr, "cannot debug null data");
        return;
    }
    bstring single_hex = NULL;
    bstring single_char = NULL;

    bstring hex_dump = bfromcstr("");
    bstring san_dump = bfromcstr("");
    char* buf = calloc(4,sizeof(char));
    if(buf == NULL || data == NULL){
        fprintf(stderr, "debug could not allocate a conversion buffer");
        goto exit;
    }

    unsigned char* raw_char;
    unsigned char* cstr = (unsigned char*)bdata(data);
    if(cstr == NULL){
        goto exit;
        return;
    }

    for(int i=0; i<blength(data); i++){
        snprintf(buf,4,"%02X ",cstr[i]);
        single_hex = bfromcstr(buf);
        bconcat(hex_dump,single_hex);
        bdestroy(single_hex);

        raw_char = &cstr[i];
        if(*raw_char == START_CHAR){
            buf[0] = '@';
        } else if(*raw_char == TERM_CHAR){
            buf[0] = '@';
        } else {
            buf[0] = *raw_char;
        }
        buf[1] = '\0';
        
        single_char = bfromcstr(buf);
        bconcat(san_dump,single_char);
        bdestroy(single_char);
    }
    fprintf(stdout, "########################\n");
    fprintf(stdout, "SANITIZED DATA\n%.*s\n",blength(san_dump), bdata(san_dump));
    fprintf(stdout, "DEBUGGER SEZ\n%.*s\n", blength(hex_dump), bdata(hex_dump));
    fprintf(stdout, "########################\n");

    exit:
    bdestroy(san_dump);
    bdestroy(hex_dump);
    free(buf);

    return;
}

int mongrel2_ws_reply(mongrel2_socket *pub_socket, mongrel2_request *req, bstring data){
    bstring payload = bstrcpy(data);

    // ! and # are the fill characters. You should not see these if the data is correct
    bInsertChrs(payload, blength(payload), 1, TERM_CHAR, '!');
    bInsertChrs(payload, 0, 1, START_CHAR, '#');
    mongrel2_reply(pub_socket,req,payload);
    // mongrel2_ws_debug(payload);
    bdestroy(payload);
    return 0;
}

int mongrel2_ws_handshake_response(mongrel2_request *req, unsigned char response[16]){
    // const char* headers = bdata(req->raw_headers);
    bstring bseckey1 = mongrel2_request_get_header(req,"sec-websocket-key1");
    bstring bseckey2 = mongrel2_request_get_header(req,"sec-websocket-key2");
    if(bseckey1 == NULL || bseckey2 == NULL){
        fprintf(stderr,"sec-websocket-key1 or 2 were not present");
        bdestroy(bseckey1);
        bdestroy(bseckey2);
        return -1;
    }

    // TODO : This guy will be throwing error in the near future.
    // Gotta validate the websocket protocol anyway...
    uint32_t seckey2;
    mongrel2_ws_extract_seckey(bdata(bseckey2),&seckey2);
    uint32_t seckey1;
    mongrel2_ws_extract_seckey(bdata(bseckey1),&seckey1);

    mongrel2_ws_calculate_response(seckey1,seckey2,req->body, response);
    bdestroy(bseckey1);
    bdestroy(bseckey2);
    return 0;
}

static int mongrel2_ws_extract_seckey(const char* seckey_str, uint32_t *seckey){
    uint32_t seckey_concat = mongrel2_ws_concat_numbers(seckey_str);
    uint32_t seckey_num_sp = mongrel2_ws_count_spaces(seckey_str);

    *seckey = seckey_concat/seckey_num_sp;
    return 0;
}

/**
 * Assumes little endian architecture!
 * Takes the inputs and returns the challenge response necessary
 * for completing a WS handshake.
 * @param seckey1   - Concated number, divided by number of spaces
 * @param seckey2   - Found same as seckey2
 * @param body      - Body from their request
 * @param md5output - Room enough to store the md5'd result
 * @return  0 on success
 */
static int mongrel2_ws_calculate_response(uint32_t seckey1, uint32_t seckey2, bstring body, unsigned char md5output[16]){
    // TODO test if body is 8 bytes.
    char* md5input = calloc(16,sizeof(char));
    char* seckey1_bytes = (char*)&seckey1;
    char* seckey2_bytes = (char*)&seckey2;
    char* body_str = bdata(body);
    if(body_str == NULL){
        fprintf(stderr, "mongrel2_ws_calculate_reponse got an empty body...");
        return -1;
    }

    // TODO this assumes little endian arch. Do a runtime check in the future to make it cross platform.
    md5input[0] = seckey1_bytes[3];
    md5input[1] = seckey1_bytes[2];
    md5input[2] = seckey1_bytes[1];
    md5input[3] = seckey1_bytes[0];

    md5input[4] = seckey2_bytes[3];
    md5input[5] = seckey2_bytes[2];
    md5input[6] = seckey2_bytes[1];
    md5input[7] = seckey2_bytes[0];

    memcpy((void*)&md5input[8], body_str, 8);

    md5((const unsigned char *)md5input, 16, md5output);
    free(md5input);
    return 0;
}

/**
 * Take a normal C string, concats all the numbers characters
 * and then turns that into a computer-native value.
 * @param incoming
 * @return
 */
static uint32_t mongrel2_ws_concat_numbers(const char *incoming){
    char* numbuff = calloc(16,sizeof(char));
    int ni = 0;
    for(int i=0; i<strlen(incoming); i++){
        if(incoming[i]-'0' >= 0 && incoming[i]-'9' <= 0){
            numbuff[ni] = incoming[i];
            ni = ni + 1;
        }
    }
    uint32_t extracted_number;
    sscanf(numbuff,"%u",&extracted_number);
    free(numbuff);
    return extracted_number;
}

static uint32_t mongrel2_ws_count_spaces(const char *incoming){
    int count = 0;
    for(int i=0; i<strlen(incoming); i++){
        if(incoming[i] == ' '){
            count = count + 1;
        }
    }
    return count;
}

// Swap this with main -- this is a protocol test
int test(int argc, char **args){
    /**
     * Example from page 8 of the web socket protocol RFC : http://www.whatwg.org/specs/web-socket-protocol/
     */
    char *headers = "{\"PATH\":\"/dds_stream\",\"host\":\"localhost:6767\","
                    "\"sec-websocket-key1\":\"18x 6]8vM;54 *(5:  {   U1]8  z [  8\","
                    "\"origin\":\"http://localhost:6767\",\"x-forwarded-for\":\"::1\","
                    "\"upgrade\":\"WebSocket\",\"connection\":\"Upgrade\","
                    "\"sec-websocket-key2\":\"1_ tx7X d  <  nw  334J702) 7]o}` 0\","
                    "\"METHOD\":\"GET\",\"VERSION\":\"HTTP/1.1\",\"URI\":\"/dds_stream\","
                    "\"PATTERN\":\"/dds_stream\"}";
    char *body = "Tm[K T2u";
    mongrel2_request *req = calloc(1,sizeof(mongrel2_request));
    req->raw_headers = bfromcstr(headers);
    req->body = bfromcstr(body);
    unsigned char response[16];
    mongrel2_ws_handshake_response(req, response);
    if(strncmp((const char*)response,"fQJ,fN/4F4!~K~MH",16) != 0){
        fprintf(stdout,"Response did not match expection\n");
        fprintf(stdout,"Exepcted: fQJ,fN/4F4!~K~MH\n");
        fprintf(stdout,"Got: %s",response);
    }
    mongrel2_request_finalize(req);
    return 0;
}
