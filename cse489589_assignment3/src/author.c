/**
 * @author
 * @author  Swetank Kumar Saha <swetankk@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * AUTHOR [Control Code: 0x00]
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "../include/global.h"
#include "../include/control_header_lib.h"
#include "../include/network_util.h"
#include "../include/connection_manager.h"

#define AUTHOR_STATEMENT "I, jzhao44, have read and understood the course academic integrity policy."

void author_response(int sock_index)
{
    uint16_t payload_len, response_len;
    char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

    payload_len = sizeof(AUTHOR_STATEMENT)-1; // Discount the NULL chararcter
    cntrl_response_payload = (char *) malloc(payload_len);
    memcpy(cntrl_response_payload, AUTHOR_STATEMENT, payload_len);

    cntrl_response_header = create_response_header(sock_index, 0, 0, payload_len);

    response_len = CNTRL_RESP_HEADER_SIZE+payload_len;
    cntrl_response = (char *) malloc(response_len);
    /* Copy Header */
    memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);
    /* Copy Payload */
    memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
    free(cntrl_response_payload);

    sendALL(sock_index, cntrl_response, response_len);

    free(cntrl_response);
}

void init_response(int sock_index, int routerport, int dataport){
    /*init router(UDP)-router_socket and data(TCP) -data_socket*/
/*router socket setup*/
    int sock;
    struct sockaddr_in router_addr;
    socklen_t addrlen = sizeof(router_addr);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    //printf("%d\n", sock);
    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");


    bzero(&router_addr, sizeof(router_addr));

    router_addr.sin_family = AF_INET;
    router_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    router_addr.sin_port = htons(routerport);

    //printf("here??\n");
    if(bind(sock, (struct sockaddr *)&router_addr, sizeof(router_addr)) < 0)
        ERROR("bind() failed");
    router_socket = sock;
    //printf("not here\n");

/*data socket setup  */
    struct sockaddr_in data_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        ERROR("socket() failed");
    }
    /*
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0){
        ERROR("setsockopt() failed");
    }
    */
    bzero(&data_addr, sizeof(data_addr));

    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    data_addr.sin_port = htons(dataport);

    if(bind(sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0){
        ERROR("bind() failed");
    }

    if(listen(sock, 5) < 0)
        ERROR("listen() failed");
    data_socket = sock;

    printf("router_socket:%d, data_socket:%d\n", router_socket, data_socket);

    /*send init response to controller*/
    char *cntrl_response_header;

    cntrl_response_header = create_response_header(sock_index, 1, 0, 0);

    sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);
    inited = TRUE;
    //return;
    /*start broadcasting distance vectors, ??and establish connections??*/
    struct sockaddr_in destaddr;
    struct timeval temp;
    gettimeofday(&temp, NULL);
    for(int i=0; i<numofrouters; i++){
        if(isNeighbor[i] == 1 && thisID != i+1){
            bzero(&destaddr, sizeof(destaddr));
            destaddr.sin_family = AF_INET;
            destaddr.sin_addr.s_addr = topmap[i].ip_addr;
            destaddr.sin_port = htons(topmap[i].port1);

            char msg[100] = "";
            sprintf(msg, "%d---", thisID);
            for(int i=0; i< numofrouters; i++){
                char tmp[20];
                sprintf(tmp, "%d", topmap[i].cost);
                strcat(msg, tmp);
                if(i != numofrouters-1) strcat(msg, "---");
            }

			//printf("sendto port:%d, ", ntohs(destaddr.sin_port));
            //printf("#%d broadcast this: %s to %d\n",thisID ,msg, i+1);
            //ssize_t sent;
			//printf("sendto port:%d", destaddr.sin_port);
            //sent = sendto(router_socket, msg, sizeof(msg), 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
            //printf("sent:%ld, %lu\n", sent, sizeof(msg));
        }
    }
    timeoutSeq[thisID-1].nextexpecttime = temp.tv_sec+update_interval;
    printf("init this %d nextexpecttime:%ld(%ld)\n", thisID-1, temp.tv_sec+update_interval, temp.tv_sec);
}

void routing_table_response(int sock_index){
    uint16_t payload_len, response_len;
    char *cntrl_response_header, *cntrl_response_payload, *cntrl_response;

    payload_len = sizeof(char) * 8 * numofrouters;
    cntrl_response_payload = (char *)malloc(payload_len);
    bzero(cntrl_response_payload, sizeof(cntrl_response_payload));
    /*problem: should here do htons()????*/
    for(int i=0; i<numofrouters; i++){
        // void *src????
        uint16_t thisID_n = htons(topmap[i].routerId);
        uint16_t nexthop_n;
		if(distanceVector[thisID-1][i].nexthop >=1 && distanceVector[thisID-1][i].nexthop <=5)
			{nexthop_n= htons(topmap[distanceVector[thisID-1][i].nexthop-1].routerId);}
		else
			{nexthop_n = htons(INF);}
        uint16_t cost_n = htons(distanceVector[thisID-1][i].cost);
        memcpy(cntrl_response_payload+0x08*i, &thisID_n, sizeof(uint16_t));
        memcpy(cntrl_response_payload+0x04+0x08*i, &nexthop_n, sizeof(uint16_t));
        memcpy(cntrl_response_payload+0x06+0x08*i, &cost_n, sizeof(uint16_t));
    }

    cntrl_response_header = create_response_header(sock_index, 2, 0, payload_len);

    response_len = CNTRL_RESP_HEADER_SIZE+payload_len;
    cntrl_response = (char *)malloc(response_len);
    /* Copy Header */
    memcpy(cntrl_response, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);
    /* Copy Payload */
    memcpy(cntrl_response+CNTRL_RESP_HEADER_SIZE, cntrl_response_payload, payload_len);
    free(cntrl_response_payload);

    sendALL(sock_index, cntrl_response, response_len);

    free(cntrl_response);
}

void update_response(int sock_index, uint16_t targetid, uint16_t targetcost){
    /*send update response to controller*/
    char *cntrl_response_header;
    cntrl_response_header = create_response_header(sock_index, 3, 0, 0);
    sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);

    topmap[targetid-1].cost = targetcost;
    distanceVector[thisID-1][targetid-1].cost = targetcost;
/*
	for(int i=0; i<numofrouters; i++){
		if(distanceVector[thisID-1][i].nexthop == targetid){
			distanceVector[thisID-1][i].cost = distanceVector[thisID-1][targetid-1].cost + distanceVector[targetid-1][i].cost;	
		}
	}
*/

	for(int i=0; i<numofrouters; i++){
		for(int j=0; j<numofrouters; j++){
			if(distanceVector[i][j].nexthop == targetid){
				distanceVector[i][j].cost = distanceVector[i][targetid-1].cost + distanceVector[targetid-1][j].cost;	
			}	
		}	
	}

    updateDV();
}

void crash_response(int sock_index){
    /*send crash response to controller*/
    char *cntrl_response_header;
    cntrl_response_header = create_response_header(sock_index, 4, 0, 0);
    sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);

    exit(0);
}

void sendfile_response(int sock_index, uint32_t destination, uint8_t ttl, uint8_t transferid, uint16_t seqnumber, char *filename){
   unsigned long fsize = get_filesize(filename);
   unsigned long sent = 0;

   FILE *fp;
   if((fp = fopen(filename, "r")) == NULL){
       ERROR("file open error");
       return;
   }

    /*send sendfile response to controller
      after the fin bit set packet sent.  */
    char *cntrl_response_header;
    cntrl_response_header = create_response_header(sock_index, 5, 0, 0);
    sendALL(sock_index, cntrl_response_header, CNTRL_RESP_HEADER_SIZE);
    free(cntrl_response_header);
}
