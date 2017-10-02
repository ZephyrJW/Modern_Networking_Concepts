/**
 * @control_handler
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
 * Handler for the control plane.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/queue.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/network_util.h"
#include "../include/control_header_lib.h"
#include "../include/author.h"

#ifndef PACKET_USING_STRUCT
    #define CNTRL_CONTROL_CODE_OFFSET 0x04
    #define CNTRL_PAYLOAD_LEN_OFFSET 0x07
#endif

//int topmap_index = 0;
/*loacl ports*/
int routerport;
int dataport;

/* Linked List for active control connections */
struct ControlConn
{
    int sockfd;
    LIST_ENTRY(ControlConn) next;
}*connection, *conn_temp;
LIST_HEAD(ControlConnsHead, ControlConn) control_conn_list;

int create_control_sock()
{
    int sock;
    struct sockaddr_in control_addr;
    socklen_t addrlen = sizeof(control_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        ERROR("socket() failed");

    /* Make socket re-usable */
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof(int)) < 0)
        ERROR("setsockopt() failed");

    bzero(&control_addr, sizeof(control_addr));

    control_addr.sin_family = AF_INET;
    control_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    control_addr.sin_port = htons(CONTROL_PORT);

    if(bind(sock, (struct sockaddr *)&control_addr, sizeof(control_addr)) < 0)
        ERROR("bind() failed");

    if(listen(sock, 10) < 0)
        ERROR("listen() failed");

    LIST_INIT(&control_conn_list);

    return sock;
}

int new_control_conn(int sock_index)
{
    int fdaccept, caddr_len;
    struct sockaddr_in remote_controller_addr;

    caddr_len = sizeof(remote_controller_addr);
    fdaccept = accept(sock_index, (struct sockaddr *)&remote_controller_addr, &caddr_len);
    if(fdaccept < 0)
        ERROR("accept() failed");

    /* Insert into list of active control connections */
    connection = malloc(sizeof(struct ControlConn));
    connection->sockfd = fdaccept;
    LIST_INSERT_HEAD(&control_conn_list, connection, next);

    return fdaccept;
}

void remove_control_conn(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next) {
        if(connection->sockfd == sock_index) LIST_REMOVE(connection, next); // this may be unsafe?
        free(connection);
    }

    //if(sock_index == control_socket) ERROR("Why are you closing listening socket");
    close(sock_index);
}

bool isControl(int sock_index)
{
    LIST_FOREACH(connection, &control_conn_list, next)
        if(connection->sockfd == sock_index) return TRUE;

    return FALSE;
}

bool isData(int sock_index)
{
    for(int i=0; i<5; i++){
        if(dataconnections[i].sockfd == sock_index) return TRUE;
    }
    return FALSE;
}

void establish_connections(){
    for(int i=0; i<5; i++){
        if(isNeighbor[i] == 1){
            if(dataconnections[i].sockfd != -1) break;
            else{
                struct sockaddr_in toConnect;
                socklen_t len = sizeof(toConnect);
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                if(sock < 0){
                    ERROR("socket create failed");
                }

                bzero(&toConnect, sizeof(toConnect));
                toConnect.sin_family = AF_INET;
                toConnect.sin_addr.s_addr = topmap[i].ip_addr;
                toConnect.sin_port = htons(topmap[i].port2);

                //struct sockaddr *tmp = (struct sockaddr *)toConnect;
                if(connect(sock, (struct sockaddr *)&toConnect, len) == -1){
                    close(sock);
                    ERROR("connect failed");
                }

                dataconnections[i].sockfd = sock;
                dataconnections[i].counterpart = topmap[i].ip_addr;
                // should fdset
            }
        }
    }
}

bool control_recv_hook(int sock_index)
{
    char *cntrl_header, *cntrl_payload;
    uint8_t control_code;
    uint16_t payload_len;

    /* Get control header */
    cntrl_header = (char *) malloc(sizeof(char)*CNTRL_HEADER_SIZE);
    bzero(cntrl_header, CNTRL_HEADER_SIZE);

    if(recvALL(sock_index, cntrl_header, CNTRL_HEADER_SIZE) < 0){
        //debug
        printf("should have entered here(remove_control_conn)\n");
        remove_control_conn(sock_index);
        free(cntrl_header);
        return FALSE;
    }

    /* Get control code and payload length from the header */
    #ifdef PACKET_USING_STRUCT
        /** ASSERT(sizeof(struct CONTROL_HEADER) == 8)
          * This is not really necessary with the __packed__ directive supplied during declaration (see control_header_lib.h).
          * If this fails, comment #define PACKET_USING_STRUCT in control_header_lib.h
          */
        BUILD_BUG_ON(sizeof(struct CONTROL_HEADER) != CNTRL_HEADER_SIZE); // This will FAIL during compilation itself; See comment above.

        struct CONTROL_HEADER *header = (struct CONTROL_HEADER *) cntrl_header;
        control_code = header->control_code;
        payload_len = ntohs(header->payload_len);
    #endif
    #ifndef PACKET_USING_STRUCT
        memcpy(&control_code, cntrl_header+CNTRL_CONTROL_CODE_OFFSET, sizeof(control_code));
        memcpy(&payload_len, cntrl_header+CNTRL_PAYLOAD_LEN_OFFSET, sizeof(payload_len));
        payload_len = ntohs(payload_len);
    #endif

    free(cntrl_header);

    /* Get control payload */
    ssize_t got = 0;
    if(payload_len != 0){
        cntrl_payload = (char *) malloc(sizeof(char)*payload_len);
        bzero(cntrl_payload, payload_len);

        if((got = recvALL(sock_index, cntrl_payload, payload_len)) < 0){
            remove_control_conn(sock_index);
            free(cntrl_payload);
            return FALSE;
        }
    }
    //printf("got: %ld\n", got);

    /* Triage on control_code */
    switch(control_code){
        case 0:
			author_response(sock_index);
            break;

        case 1:
			//printf("%lu\n", sizeof(cntrl_payload));
            printf("");
            //uint16_t numofrouters = 0;
            //uint16_t update_interval = 0;
            memcpy(&numofrouters, cntrl_payload+0x00, sizeof(numofrouters));
            memcpy(&update_interval, cntrl_payload+0x02, sizeof(update_interval));
            numofrouters = ntohs(numofrouters);
            update_interval = ntohs(update_interval);
            //printf("#of router: %d, intervaltime: %d\n", numofrouters, update_interval);

            for(int i=0; i<numofrouters; i++){
                memcpy(&topmap[i].routerId, cntrl_payload+0x04+12*i, sizeof(topmap[i].routerId));
                memcpy(&topmap[i].port1, cntrl_payload+0x06+12*i, sizeof(topmap[i].port1));
                memcpy(&topmap[i].port2, cntrl_payload+0x08+12*i, sizeof(topmap[i].port2));
                memcpy(&topmap[i].cost, cntrl_payload+0x0a+12*i, sizeof(topmap[i].cost));
                memcpy(&topmap[i].ip_addr, cntrl_payload+0x0c+12*i, sizeof(topmap[i].ip_addr));
                topmap[i].virtualID = virtualIDseed++;

                topmap[i].routerId = ntohs(topmap[i].routerId);
                topmap[i].port1 = ntohs(topmap[i].port1);
                topmap[i].port2 = ntohs(topmap[i].port2);
                topmap[i].cost = ntohs(topmap[i].cost);
                // topmap[i].ip_addr = ntohs(topmap[i].ip_addr);
                printf("#%d router info: %d, %d, %d, %d, %d, ", i, topmap[i].routerId, topmap[i].port1, topmap[i].port2, topmap[i].cost, topmap[i].ip_addr);
                //printf("%x\n", topmap[i].ip_addr);
                char address[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &topmap[i].ip_addr, address, INET_ADDRSTRLEN);
                printf("%s\n", address);
                //topmap_index = i;
            }
            //printf("%d\n", topmap_index);
            uint32_t localip;
            getlocalIP(&localip);
            for(int i=0; i< numofrouters; i++){
                if(localip == topmap[i].ip_addr) {
                    routerport = topmap[i].port1;
                    dataport = topmap[i].port2;
                    //thisID = topmap[i].routerId;
                    thisID = topmap[i].virtualID;
                }
                isNeighbor[i] = topmap[i].cost == INF? 0:1;
            }

            /*init distanceVector matrix*/
            for(int i=0; i<numofrouters; i++){
                for(int j =0; j<numofrouters; j++){
                    if(thisID == i+1){
                        distanceVector[i][j].cost = topmap[j].cost == INF? INF:topmap[j].cost;
                        distanceVector[i][j].nexthop = topmap[j].cost == INF?-1:j+1;
                    }else{
                        distanceVector[i][j].cost = INF;
                        distanceVector[i][j].nexthop = -1;
                    }
                }
            }

#if 0
/* vector*/
            for(int i=0;i<numofrouters; i++){
                for(int j=0;j<numofrouters; j++){
                    printf("DV[%d][%d]:%d %d    ",i,j, distanceVector[i][j].cost, distanceVector[i][j].nexthop);
                }
                printf("\n");
            }
/*neighbor list(including thr router itself without 'i+1 != thisID')*/
            for(int i=0;i<numofrouters;i++){
                if(isNeighbor[i] == 1 && i+1 != thisID) printf("%d   ", i+1);
            }
            printf("\n");
#endif
            printf("thisId: %d\n", thisID);
            init_response(sock_index, routerport, dataport);
            break;

        case 2:
            /*routing_table*/
            routing_table_response(sock_index);
            break;
        case 3:
            /*update one router's link cost*/
            printf("");
            uint16_t targetid, targetcost;
            memcpy(&targetid, cntrl_payload, sizeof(uint16_t));
            memcpy(&targetcost, cntrl_payload+0x02, sizeof(uint16_t));
            targetid = ntohs(targetid);
			for(int i=0; i< numofrouters; i++){
				if(targetid == topmap[i].routerId){
					targetid = topmap[i].virtualID;
					break;
				}
			}
            targetcost = ntohs(targetcost);
            update_response(sock_index, targetid, targetcost);
            break;
        case 4:
            /*this router crashes*/
            crash_response(sock_index);
            break;
        case 5:
            /*send file command*/
            printf("");
            uint32_t destination;
            uint8_t ttl, transferid;
            uint16_t seqnumber, filename_len;
            filename_len = payload_len-8;
            char *filename = (char *)malloc(sizeof(char) * filename_len);

            memcpy(&destination, cntrl_payload, sizeof(destination));
            memcpy(&ttl, cntrl_payload+0x04, sizeof(ttl));
            memcpy(&transferid, cntrl_payload+0x05, sizeof(transferid));
            memcpy(&seqnumber, cntrl_payload+0x06, sizeof(seqnumber));
            memcpy(filename, cntrl_payload+0x08, sizeof(char) * filename_len);

            destination = ntohl(destination);
            // 16 bits required ??
            ttl = ntohs(ttl);
            transferid = ntohs(transferid);
            seqnumber = ntohs(seqnumber);

            establish_connections();
            justEstablished = TRUE;
            sendfile_response(sock_index, destination, ttl, transferid, seqnumber, filename);
            break;
        /* .......
        case 1: init_response(sock_index, cntrl_payload);
                break;

            .........
           .......
         ......*/
    }

    if(payload_len != 0) free(cntrl_payload);
    return TRUE;
}
