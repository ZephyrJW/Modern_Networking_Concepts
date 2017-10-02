/**  * @connection_manager  * @author  Swetank Kumar Saha
<swetankk@buffalo.edu>  * @version 1.0  *  * @section LICENSE  *  * This
program is free software; you can redistribute it and/or  * modify it under
the terms of the GNU General Public License as  * published by the Free
Software Foundation; either version 2 of  * the License, or (at your option)
any later version.  *  * This program is distributed in the hope that it will
be useful, but  * WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  * General
Public License for more details at  * http://www.gnu.org/copyleft/gpl.html  *
* @section DESCRIPTION  *  * Connection Manager listens for incoming
connections/messages from the  * controller and other routers and calls the
desginated handlers.  */

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>

#include "../include/connection_manager.h"
#include "../include/global.h"
#include "../include/control_handler.h"

fd_set master_list, watch_list;
int head_fd;

/*this router times out, so rebroadcast*/
void broadcast(){
    struct sockaddr_in destaddr;
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
                //sprintf(tmp, "%d", topmap[i].cost);
                sprintf(tmp, "%d", distanceVector[thisID-1][i].cost);
                strcat(msg, tmp);
                if(i != numofrouters-1) strcat(msg, "---");
            }

/*
			char ip[100];
			socklen_t socklen = sizeof(ip);
			inet_ntop(AF_INET, &(destaddr.sin_addr.s_addr), ip, socklen);
            printf("#%d broadcast this: %s\n",thisID ,msg);
*/
			//printf("to %s port:%d\n", ip,ntohs(destaddr.sin_port));
            sendto(router_socket, msg, sizeof(msg), 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
        }
    }
}

/*3 consective timeout remove this neighbor,
  index is the index of down router.        */
void downneighbor_handler(int index){
    printf("handle down routers\n");
    //assert(topmap[index].routerId == index+1);
    topmap[index].cost = INF;
    distanceVector[thisID-1][index].cost = INF;
    //assert(isNeighbor[index] == 1);
    isNeighbor[index] = 0;
    //should the timer also be updated?

    updateDV();
}

/*update this routers DV from distanceVector algorithm*/
void updateDV_ori(){
    printf("here we update DV\n");
    while(1){
        bool updated = FALSE;
        /*distanceVector[a][b].cost -- link cost from a to b*/
        for(int i=0; i<numofrouters; i++){
            if(i == thisID-1 || distanceVector[thisID-1][i].cost == INF || isNeighbor[i] == 0) continue;

            for(int j=0; j<numofrouters; j++){
                if(distanceVector[i][j].cost + distanceVector[thisID-1][i].cost < distanceVector[thisID-1][j].cost){
                    distanceVector[thisID-1][j].cost = distanceVector[i][j].cost + distanceVector[thisID-1][i].cost;
                    //distanceVector[thisID-1][j].nexthop = (isNeighbor[i] == 1)? i+1: distanceVector[thisID-1][i].nexthop;
					distanceVector[thisID-1][j].nexthop = distanceVector[thisID-1][i].nexthop;
                    updated = TRUE;
                }
            }
        }
        if(updated == FALSE) break;
    }
}

void updateDV(){
/*	
	for(int i=0; i<numofrouters; i++){
		if(distanceVector[thisID-1][i].cost == INF || i == thisID-1) continue;
		if(distanceVector[thisID-1][i].nexthop >= 1 && distanceVector[thisID-1][i].nexthop <=5){
			int next = distanceVector[thisID-1][i].nexthop;
			distanceVector[thisID-1][i].cost = distanceVector[thisID-1][next-1].cost + distanceVector[next-1][i].cost;
		}
	}
*/
	for(int i=0; i< numofrouters; i++){
		for(int j=0; j<numofrouters; j++){
			if(distanceVector[thisID-1][i].cost > distanceVector[j][i].cost){
				if(distanceVector[thisID-1][j].cost + distanceVector[j][i].cost < distanceVector[thisID-1][i].cost){
					distanceVector[thisID-1][i].cost = distanceVector[thisID-1][j].cost + distanceVector[j][i].cost;
					distanceVector[thisID-1][i].nexthop = distanceVector[thisID-1][j].nexthop;
				}
			}
		}
	}
}
/*
void updateDV1(){
	while(1){
		bool updated = FALSE;
	
		for(int i=0; i< numofrouters; i++){
			if(distanceVector[thisID-1][i].cost == 0 || distanceVector[thisID-1][i].cost == INF) continue;

			for(int j=0; j< numofrouters; j++){
				if(distanceVector[thisID-1][j].cost > distanceVector[thisID-1][i].cost + distanceVector[i][j].cost){
					distanceVector[thisID-1][j].cost = distanceVector[thisID-1][i].cost + distanceVector[i][j].cost;
					distanceVector[thisID-1][j].nexthop = i+1;
					updated = TRUE;
				}
			}
		}
		if(updated == FALSE) break;
	}	
	
}
*/
/*update DV from msg received*/
void process_routermsg(char *content){
    char *segments[20];
    int count = 0;
    char *p;
    p = strtok(content, "---");
    while(p != NULL){
        segments[count++] = p;
        p = strtok(NULL, "---");
    }

    int index = atoi(segments[0]);
    bool hasChanged = FALSE;
    for(int i=0; i<numofrouters; i++){
        int tmp = atoi(segments[i+1]);
        if(distanceVector[index-1][i].cost != tmp) hasChanged = TRUE;
        distanceVector[index-1][i].cost = tmp;
    }

	if(hasChanged == TRUE){
		for(int i=0; i<numofrouters; i++){
			if(distanceVector[thisID-1][i].nexthop == index){
				distanceVector[thisID-1][i].cost = distanceVector[index-1][i].cost + distanceVector[thisID-1][index-1].cost;	
			}
		}
	}
	

    printf("display distanceVectors\n");
    for(int i=0; i<numofrouters; i++){
        for(int j=0; j<numofrouters; j++){
            printf("%d ", distanceVector[i][j].cost);
        }
        printf("\n");
    }

    if(hasChanged == TRUE) updateDV();
}

void main_loop()
{
    int selret, sock_index, fdaccept;

    while(TRUE){
        /* can't printf anything out when uninited, why?
        for(int i=0;i<numofrouters;i++){
            printf("%d, %ld,%d  ",i, timeoutSeq[i].nextexpecttime, timeoutSeq[i].chances);
        }
        printf("\n");
        */
        watch_list = master_list;
        if(inited == FALSE) {
            selret = select(head_fd+1, &watch_list, NULL, NULL, NULL);
        }else{
            struct timeval timer, current;
            gettimeofday(&current, NULL);

            //int i=0;
			//printf("%d\n", numofrouters);
			//
			///*
			printf("current time:%d,  ", current.tv_sec);
			for(int i=0;i<numofrouters; i++){
				printf("%d ", timeoutSeq[i].nextexpecttime);
			}
			printf("\n");
			//*/
            time_t nexttimer = 0;
			bool set = FALSE;

            for(int i=0;i<numofrouters; i++){
                if(timeoutSeq[i].nextexpecttime == 0) continue;
                //printf("expect time:%ld\n", timeoutSeq[i].nextexpecttime);
                time_t tmp = timeoutSeq[i].nextexpecttime - current.tv_sec;
                if(set == FALSE){
					nexttimer = tmp;
					set = TRUE;
				}else nexttimer = nexttimer > tmp? tmp: nexttimer;

				//printf("nexttimer: %d", nexttimer);
                //break;
            }
            //timer.tv_sec = nexttimer < 0? 0: nexttimer;
			timer.tv_sec = nexttimer;
            timer.tv_usec = 0;

            //printf("timer.sec: %ld\n", timer.tv_sec);
            selret = select(head_fd+1, &watch_list, NULL, NULL, &timer);
            //printf("%ld-%ld = %ld\n",timeoutSeq[i].nextexpecttime, current.tv_sec,nexttimer);
        }

        // select returns the total number of ready fd in the sets
        if(selret < 0)
            ERROR("select failed.");

        // timeout
        if(selret == 0){
            //timeout
            struct timeval now;
            gettimeofday(&now, NULL);
/*
			for(int i=0;i<numofrouters; i++){
				printf("%d ", timeoutSeq[i].nextexpecttime);
			}
			printf("\n");
*/
            for(int i=0; i<numofrouters; i++){
                if(timeoutSeq[i].nextexpecttime != now.tv_sec) continue;
                if(i == thisID-1) {
                    // implement this, send distance vector to neighbor again..
                    printf("this router fires timeout rebroadcast\t");
                    broadcast();
                    timeoutSeq[i].nextexpecttime = now.tv_sec + update_interval;
                    //break;
                }else{
                    timeoutSeq[i].nextexpecttime = now.tv_sec + update_interval;
                    timeoutSeq[i].chances--;
                    if(timeoutSeq[i].chances == 0){
                        // this neighbor is hopeless
                        printf("should handle the down routers\n");
                        downneighbor_handler(i);
                    }
                    //break;
                }
            }
        }else{

            /* Loop through file descriptors to check which ones are ready */
            for(sock_index=0; sock_index<=head_fd; sock_index+=1){

                if(FD_ISSET(sock_index, &watch_list)){

                    /* control_socket */
                    if(sock_index == control_socket){
                        fdaccept = new_control_conn(sock_index);

                        /* Add to watched socket list */
                        FD_SET(fdaccept, &master_list);
                        if(fdaccept > head_fd) head_fd = fdaccept;
                    }

                    /* router_socket */
                    else if(sock_index == router_socket){
                        //printf("ever enters here\n");
                        //call handler that will call recvfrom() .....
                        struct sockaddr sender;
                        socklen_t len;
                        char recvmsg[100];
                        recvfrom(router_socket, recvmsg, sizeof(recvmsg), 0, &sender, &len);

                        struct timeval temp;
                        gettimeofday(&temp, NULL);
                        //struct sockaddr_in *sender_in = (struct sockaddr_in *)&sender;
                        /*
                        for(int i=0;i<numofrouters;i++){
                            if(topmap[i].ip_addr == (((struct sockaddr_in *)&sender)->sin_addr.s_addr)){
                            //if(topmap[i].ip_addr == sender_in->sin_addr.s_addr){
                                //printf("%d :sender ID:%d--%d\n", i, i+1, topmap[i].routerId);
                                gettimeofday(&temp, NULL);
                                timeoutSeq[i].nextexpecttime = temp.tv_sec+update_interval;
                                timeoutSeq[i].chances = 3;
                            }
                        }*/

                        char victim[100];
                        strcpy(victim, recvmsg);
                        char *p;
                        p = strtok(victim, "---");
                        //printf("%s| %s\n", p, recvmsg);
                        int targ = atoi(p);
                        timeoutSeq[targ-1].nextexpecttime = temp.tv_sec+update_interval;
                        timeoutSeq[targ-1].chances = 3;

                        printf("msg received:%s\n", recvmsg);
                        process_routermsg(recvmsg);
                    }

                    /* data_socket */
                    else if(sock_index == data_socket){
                        //new_data_conn(sock_index);
                        int sockfd;
                        socklen_t daddr_len;
                        struct sockaddr_in remote_data_addr;

                        sockfd = accept(sock_index, (struct sockaddr *)&remote_data_addr, &daddr_len);
                        if(sockfd < 0){
                            ERROR("accpet failed");
                        }

                        FD_SET(sockfd, &master_list);
                        if(sockfd > head_fd) head_fd = sockfd;
                        /*save this socket to corresponding dataconnections index*/
                        // is following strategy feasible?? potential bug
                        int index;
                        for(int i=0; i<numofrouters; i++){
                            if(topmap[i].ip_addr == remote_data_addr.sin_addr.s_addr){
                                index = topmap[i].routerId;
                                break;
                            }
                        }
                        assert(dataconnections[index-1].sockfd == -1);
                        dataconnections[index-1].sockfd = sockfd;
                        dataconnections[index-1].counterpart = remote_data_addr.sin_addr.s_addr;
                    }

                    /* Existing connection */
                    else{
                        if(isControl(sock_index)){
                            if(!control_recv_hook(sock_index)) FD_CLR(sock_index, &master_list);
                            // hook is init(0x01)
                            if(inited == TRUE) {
                                if(!FD_ISSET(router_socket, &master_list)){
                                    FD_SET(router_socket, &master_list);
                                    if(router_socket > head_fd) head_fd = router_socket;
                                }
                                if(!FD_ISSET(router_socket, &master_list)){
                                    FD_SET(data_socket, &master_list);
                                    if(data_socket > head_fd) head_fd = data_socket;
                                }
                            }

                            if(justEstablished == TRUE){
                                for(int i=0; i<5; i++){
                                    if(isNeighbor[i] == 1){
                                        if(!FD_ISSET(dataconnections[i].sockfd, &master_list)){
                                            FD_SET(dataconnections[i].sockfd, &master_list);
                                            if(dataconnections[i].sockfd > head_fd){
                                                head_fd = dataconnections[i].sockfd;
                                            }
                                        }
                                    }
                                }
                                justEstablished = FALSE;
                            }

                        }else if(isData(sock_index)){
                            /*recv data and establish_connections*/
                            establish_connections();
                        }
                        //else if isData(sock_index);
                        else ERROR("Unknown socket index");
                    }
                }
            } //end of for loop
        }
    }
}

void init()
{
    control_socket = create_control_sock();

    inited = FALSE;
    justEstablished = FALSE;
    update_interval = -1;
    virtualIDseed = 1;
    //router_socket and data_socket will be initialized after INIT from controller

    /*init timeoutSeq*/
    for(int i=0;i<5;i++){
        timeoutSeq[i].nextexpecttime = 0;
        timeoutSeq[i].chances = 3;
    }

    /*init dataconnections*/
    for(int i=0; i<5; i++){
        dataconnections[i].sockfd = -1;
    }

    FD_ZERO(&master_list);
    FD_ZERO(&watch_list);

    /* Register the control socket */
    FD_SET(control_socket, &master_list);
    head_fd = control_socket;

    main_loop();
}
