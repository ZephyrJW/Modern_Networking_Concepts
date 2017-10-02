/**
 * @jzhao44@buffalo.edu_assignment1
 * @author  Jiawei Zhao <jzhao44@buffalo.edu@buffalo.edu>
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
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "../include/global.h"
#include "../include/logger.h"


 #define CMD_SIZE 100
 #define BUFLEN 1024
 #define MSGLEN 256
 #define BACKLOG 4
 #define STD_IN 0
 #define HOSTNAMESTRLEN 50
 #define PORTSTRLEN 10
 #define loged_in 1
 #define loged_out 0


 struct connection {
     char hostname[HOSTNAMESTRLEN];
     char remote_addr[INET_ADDRSTRLEN];
     int portNum;
     int msg_received;
     int msg_sent;
     int status;
     int blockindex;

     struct connection *blockedIPs[3];

     int connsockfd;
 };

 char bufferedmsg[BUFLEN] = "";

 int role = 0; // denote server - 1, or client - 0
 int localsockfd;
 int clientsockfd;
 //int flag = 0;
 char listenerPort[PORTSTRLEN];
 struct connection connections[4];
 int connIndex = 0;
 int logedin = 0; // a flag to indicate log in, avoid multible log in

 fd_set master, read_fds;
 int maxfd;

 /*
  *Function to get local IP address,
  *returns 0 on success and -1 on fail.
  *Pass in a char * to store the result.
  */
  int get_localIP(char *res){
     int sockfd = 0;
     struct addrinfo hints, *servinfo, *p;
     struct sockaddr myip;
     socklen_t len = sizeof(myip);

     memset(&hints, 0, sizeof(hints));
     hints.ai_family = AF_UNSPEC;
     hints.ai_socktype = SOCK_DGRAM;

     if(getaddrinfo("8.8.8.8", "53", &hints, &servinfo) != 0){
         return -1;
     }

     for(p=servinfo; p!= NULL; p=p->ai_next){
         if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
             continue;
         }

         if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
             close(sockfd);
             continue;
         }
         break;
     }

     if(p == NULL){
         return -1;
     }
     freeaddrinfo(servinfo);

     getsockname(sockfd, &myip, &len);
     inet_ntop(AF_INET, &(((struct sockaddr_in *)&myip)->sin_addr), res, INET6_ADDRSTRLEN);
     close(sockfd);

     return 0;
 }

 /*
  * Preparation, create socket and listen for connection.
  * Success return 0, otherwise -1
  */
 int prep(const char *port){
     struct addrinfo hints, *servinfo, *p;

     memset(&hints, 0, sizeof hints);
     hints.ai_family = AF_INET;
     hints.ai_socktype = SOCK_STREAM;
     hints.ai_flags = AI_PASSIVE;

     if(getaddrinfo(NULL, port, &hints, &servinfo) == -1){
         return -1;
     }

     for(p = servinfo; p != NULL; p=p->ai_next){
         if((localsockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
             continue;
         }

         if(bind(localsockfd, p->ai_addr, p->ai_addrlen) == -1){
             close(localsockfd);
             continue;
         }
         break;
     }

     if(p == NULL) return -1;
     freeaddrinfo(servinfo);

     if(listen(localsockfd, BACKLOG) == -1){
         return -1;
     }

     return 0;
 }


 int strToNum(const char* s){
     int ret = 0;
     for (int i=0;i<strlen(s);i++){
         int t = s[i]-'0';
         if  (t<0 || t >9) return -1;
         ret = ret*10+t;
     }
     return ret;
 }


 /*
  * Check if given addr and #port is valid
  */
 int isValidAddr(char *addr, char *port){
	 //debug
	 //addr[strlen(addr)] = '\0';
	 //port[strlen(port)] = '\0';
     int ret = 1;
     for(int i=0; i<CMD_SIZE;i++){
         if(*(addr+i) == '\0') break;
         if(*(addr+i) == '.') continue;
         int t = *(addr+i) - '0';
         if(t<0 || t>9) {
             ret = 0;
             break;
         }
     }

     for(int i=0; i<CMD_SIZE;i++){
         if(*(port+i) == '\0') break;
         int t = *(port+i) - '0';
         if(t<0 || t>9) {
             ret = 0;
             break;
         }
     }
     return ret;
 }

 /*
  * Pack & unpack the list info to send,
  * argm list shoulb be empty string "",
  * res store in an struct conns array
  */
 void packList(char *list){
     for (int i=0; i<connIndex; i++) {
         if(connections[i].status == loged_in){
             char tmp[PORTSTRLEN];
             char status[5];
             // int to string
             sprintf(tmp, "%d", connections[i].portNum);
             sprintf(status, "%d", connections[i].status);

             strcat(list, connections[i].hostname);
             strcat(list, "---");
             strcat(list, connections[i].remote_addr);
             strcat(list, "---");
             strcat(list, tmp);
             strcat(list, "---");
             strcat(list, status);
             strcat(list, "---");
         }
     }
 }

 void unpackList(char *list){
     char *segments[20];
     int count = 0;
     char *p;
     p = strtok(list, "---");
     while (p != NULL) {
         segments[count++] = p;
         p = strtok(NULL, "---");
     }

     if (connIndex != 0) connIndex = 0;
     for(int i=0;i<count;){
         strcpy(connections[connIndex].hostname, segments[i++]);
         strcpy(connections[connIndex].remote_addr, segments[i++]);
         int tmp = strToNum(segments[i++]);
         connections[connIndex].portNum = tmp;
         tmp = strToNum(segments[i++]);
         connections[connIndex++].status = tmp;
     }

 }

 /*
  * Check if certain sender is blocked by receiver
  * return 0 if not blocked
  */

 int isBlocked(int sender, char *receiver){
     int ret = 0;

     char senderaddr[INET_ADDRSTRLEN] = "";
     for(int i=0;i<connIndex;i++){
         if(connections[i].connsockfd == sender){
             strcpy(senderaddr, connections[i].remote_addr);
             break;
         }
     }
     for(int i =0; i<connIndex; i++){
         if(strcmp(connections[i].remote_addr, receiver) == 0){
             for(int j=0; j <connections[i].blockindex;j++){
                 if(strcmp(connections[i].blockedIPs[j]->remote_addr, senderaddr) == 0){
                     ret = 1;
                     break;
                 }
             }
             break;
         }
     }

     return ret;
 }

 /*
  * Process input command
  */
 void processCmd(char **cmd, int count){
     if (strcmp(cmd[0], "AUTHOR") == 0) {
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n", "jzhao44");
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);
     }else if(strcmp(cmd[0], "IP") == 0){
         char localip[INET6_ADDRSTRLEN];
         get_localIP(localip);
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("IP:%s\n", localip);
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);
     }else if(strcmp(cmd[0], "PORT") == 0){
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("PORT:%d\n", strToNum(listenerPort));
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);
     }else if(strcmp(cmd[0], "LIST") == 0){
         int count = 1;
	  	 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
         for(int i=0;i<connIndex;i++){
             if(connections[i].status == loged_in){
				 cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", count++, connections[i].hostname, connections[i].remote_addr, connections[i].portNum);
				 fflush(stdout);
             }
         }
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
         fflush(stdout);
     }else if(strcmp(cmd[0], "LOGIN") == 0){
         if(role != 0 || count != 3 || !isValidAddr(cmd[1], cmd[2]) || logedin){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         struct addrinfo hints, *servinfo, *p;
         memset(&hints, 0, sizeof hints);
         hints.ai_family = AF_INET;
         hints.ai_socktype = SOCK_STREAM;

         if(getaddrinfo(cmd[1], cmd[2], &hints, &servinfo) != 0){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }
         for(p=servinfo; p!= NULL; p=p->ai_next){
             if((clientsockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
                 continue;
             }
             if(connect(clientsockfd, p->ai_addr, p->ai_addrlen) == -1){
                 close(clientsockfd);
                 continue;
             }
             break;
         }
         if(p == NULL){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }
         freeaddrinfo(servinfo);
 		 send(clientsockfd, listenerPort, sizeof listenerPort, 0);

         char buf[BUFLEN];
         recv(clientsockfd, buf, BUFLEN, 0);
         unpackList(buf);

         char unread[BUFLEN];
         recv(clientsockfd, unread, BUFLEN, 0);
         char *tmp[BUFLEN];
         int count = 0;
         char *q = strtok(unread, "---");
         while (q!=NULL) {
             tmp[count++] = q;
             q = strtok(NULL, "---");
         }

         for(int i=1;i<count; ){
			 cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
			 fflush(stdout);
			 cse4589_print_and_log("msg from:%s\n[msg]:%s\n", tmp[i], tmp[i+1]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", "RECEIVED");
             fflush(stdout);
             i+=2;
         }

        logedin = 1;
        FD_SET(clientsockfd, &master);
 		maxfd = clientsockfd>maxfd? clientsockfd:maxfd;


		cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		fflush(stdout);
		cse4589_print_and_log("[%s:END]\n", cmd[0]);
		fflush(stdout);
		// mark

     }else if(strcmp(cmd[0], "REFRESH") == 0){
         if(role != 0 || !logedin){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         send(clientsockfd, "REFRESH", 7, 0);
         char update[BUFLEN];
         recv(clientsockfd, update, BUFLEN, 0);
         unpackList(update);
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);
         //process msg to update list
     }else if(strcmp(cmd[0], "SEND") == 0){
         if(role != 0 || !logedin || !isValidAddr(cmd[1], "8888")){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         // check if recvier is in local list
         int flag = 0;
         for (int i=0; i<connIndex; i++) {
             if (strcmp(cmd[1], connections[i].remote_addr) == 0) {
                 flag = 1;
                 break;
             }
         }
         if(flag == 0) {
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         char msg[MSGLEN] = "";
         for (int i=2;i<count;i++){
             strcat(msg, cmd[i]);
             if(i != count-1) strcat(msg, " ");
         }

         char buf[BUFLEN] = "";
 		 strcat(buf, cmd[0]);
         strcat(buf, " ");
         strcat(buf, cmd[1]);
         strcat(buf, " ");
         strcat(buf, msg);
 	     send(clientsockfd, buf, sizeof(buf), 0);// sizeof buf  also works?

         char res[10];
         recv(clientsockfd, res, 10, 0);
		 //debug
		//  printf("--%s--\n", res);
         if (strcmp(res, "FAIL") == 0) {
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
         }else{
             //success
			 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
         }

     }else if (strcmp(cmd[0], "BROADCAST") == 0){
         if (role != 0 || !logedin) {
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }
 //        if (count != 2) {
 //            //fail
 //            return;
 //        }

         char msg[MSGLEN] = "";
         for (int i=1;i<count;i++){
             strcat(msg, cmd[i]);
             if(i != count-1) strcat(msg, " ");
         }

         char buf[BUFLEN];
         strcpy(buf, cmd[0]);
         strcat(buf, " ");
         strcat(buf, msg);
         send(clientsockfd, buf, BUFLEN, 0);
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);
     }else if(strcmp(cmd[0], "BLOCK") == 0){
         if (role != 0 || !logedin || !isValidAddr(cmd[1], "8888") || count != 2) {
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         // check if it's in local list
         int flag = 0;
         for(int i=0;i<connIndex;i++){
             if(strcmp(connections[i].remote_addr, cmd[1]) == 0){
                 flag = 1;
                 break;
             }
         }
         if(flag == 0){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }
         // bug
         char buf[BUFLEN];
         strcpy(buf, cmd[0]);
         strcat(buf, " ");
         strcat(buf, cmd[1]);
         send(clientsockfd, buf, BUFLEN, 0);

         char res[10];
         recv(clientsockfd, res, 10, 0);
         if(strcmp(res, "FAIL") == 0){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
         }else{
             //success
			 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
         }

     }else if(strcmp(cmd[0], "UNBLOCK") == 0){
         if(role != 0 || !logedin || !isValidAddr(cmd[1], "8888") || count != 2){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         //check local list
         int flag = 0;
         for(int i=0;i<connIndex;i++){
             if(strcmp(connections[i].remote_addr, cmd[1]) == 0){
                 flag = 1;
                 break;
             }
         }
         if(flag == 0){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
             return;
         }

         char buf[BUFLEN];
         strcpy(buf, cmd[0]);
         strcat(buf, " ");
         strcat(buf, cmd[1]);
         send(clientsockfd, buf, BUFLEN, 0);

         char res[10];
         recv(clientsockfd, res, 10, 0);
         if(strcmp(res, "FAIL") == 0){
             //fail
			 cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
         }else{
             //success
			 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
			 fflush(stdout);
			 cse4589_print_and_log("[%s:END]\n", cmd[0]);
			 fflush(stdout);
         }

     }else if(strcmp(cmd[0], "EXIT") == 0){
         char buf[BUFLEN] = "EXIT";
         send(clientsockfd, buf, BUFLEN, 0);
         logedin = 0;
         close(clientsockfd);
         FD_CLR(clientsockfd, &master);
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);
         exit(0);
     }else if(strcmp(cmd[0], "LOGOUT") == 0){
         if(!logedin){
             //fail
			cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			fflush(stdout);
		 	cse4589_print_and_log("[%s:END]\n", cmd[0]);
			fflush(stdout);
            return;
         }
         char buf[BUFLEN] = "LOGOUT";
         send(clientsockfd, buf, BUFLEN, 0); //16?
         logedin = 0;
         close(clientsockfd);
         FD_CLR(clientsockfd, &master);
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);

     }else if(strcmp(cmd[0], "STATISTICS") == 0){
         if(role != 1){
             //fail
			cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			fflush(stdout);
		  	cse4589_print_and_log("[%s:END]\n", cmd[0]);
			fflush(stdout);
            return;
         }
		 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
		 fflush(stdout);
         for(int i=0;i<connIndex;i++){
             char tmp[20];
             if(connections[i].status == loged_in){
                 strcpy(tmp, "logged-in");
             }else{
                 strcpy(tmp, "logged-out");
             }

			 cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", i+1, connections[i].hostname, connections[i].msg_sent, connections[i].msg_received, tmp);
             fflush(stdout);
             //cse4589
         }
		 cse4589_print_and_log("[%s:END]\n", cmd[0]);
		 fflush(stdout);

     }else if(strcmp(cmd[0], "BLOCKED") == 0){
         if(role != 1 || count != 2 || !isValidAddr(cmd[1], "8888")){
             //fail
			cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			fflush(stdout);
		  	cse4589_print_and_log("[%s:END]\n", cmd[0]);
			fflush(stdout);
            return;
         }
         int flag = 0;
         for(int i=0;i<connIndex;i++){
             if(strcmp(connections[i].remote_addr, cmd[1]) == 0){
                 flag = 1;
				 cse4589_print_and_log("[%s:SUCCESS]\n", cmd[0]);
				 fflush(stdout);
                 for (int j=0; j<connections[i].blockindex; j++) {
                     //cse4589
                     cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", j+1, connections[i].blockedIPs[j]->hostname, connections[i].blockedIPs[j]->remote_addr, connections[i].blockedIPs[j]->portNum);
					 fflush(stdout);
                 }
                 break;
             }
         }

         if(flag == 0){
             //fail
			cse4589_print_and_log("[%s:ERROR]\n", cmd[0]);
			fflush(stdout);
		  	cse4589_print_and_log("[%s:END]\n", cmd[0]);
			fflush(stdout);
            return;
         }else{
		  	cse4589_print_and_log("[%s:END]\n", cmd[0]);
			fflush(stdout);
		 }
     }

 }

 /*
  * Response to client incoming msges
  */
 void response(char **arguments, int count, int caller){
     if(strcmp(arguments[0], "SEND") == 0){

         char msg[BUFLEN] = "";
         char senderaddr[INET_ADDRSTRLEN];
         int sender;
         for(sender=0;sender<connIndex;sender++){
             if(connections[sender].connsockfd == caller){
                 strcat(msg, connections[sender].remote_addr);
                 strcat(msg, " ");
                 strcat(msg, arguments[2]);
                 strcat(msg, " ");

                 strcpy(senderaddr, connections[sender].remote_addr);
                 break;
             }
         }

         int flag = 0;// check if the target already exited
         for(int i=0;i<connIndex;i++){
             if(strcmp(arguments[1], connections[i].remote_addr) == 0){
                 flag = 1;
                 if(isBlocked(caller, arguments[1])){
					 send(caller, "BLOCKED", 7, 0);
                     return;
                 }

                 if(connections[i].status == loged_in){
                     send(connections[i].connsockfd, msg, BUFLEN, 0);
                     connections[i].msg_received++;
                     //triger event
					 cse4589_print_and_log("[%s:SUCCESS]\n" , "RELAYED");
					 fflush(stdout);
					 cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", connections[sender].remote_addr, connections[i].remote_addr, arguments[2]);
					 fflush(stdout);
					 cse4589_print_and_log("[%s:END]\n", "RELAYED");
					 fflush(stdout);

                 }else{
                     strcat(bufferedmsg, connections[i].remote_addr);
                     strcat(bufferedmsg, "---");
                     strcat(bufferedmsg, senderaddr);
                     strcat(bufferedmsg, "---");
                     strcat(bufferedmsg, arguments[2]);
                     strcat(bufferedmsg, "---");
                     connections[i].msg_received++;
                 }

                 break;
             }
         }
         if(flag==0){
             send(caller, "FAIL", 4, 0);
         }else{
             send(caller, "SUCCESS", 7, 0);
             connections[sender].msg_sent++;
         }

     }else if(strcmp(arguments[0], "REFRESH") == 0){
         char list[BUFLEN] = "";
         packList(list);
         send(caller, list, BUFLEN, 0);
     }else if(strcmp(arguments[0], "BROADCAST") == 0){

         char sender[INET_ADDRSTRLEN];
         char msg[BUFLEN];
         for(int i=0;i<connIndex;i++){
             if(connections[i].connsockfd == caller){
                 connections[i].msg_sent++;
                 strcpy(sender, connections[i].remote_addr);

                 strcpy(msg, connections[i].remote_addr);
                 strcat(msg, " ");
                 strcat(msg, arguments[1]);
                 strcat(msg, " ");
             }
         }

         for (int i=0; i<connIndex; i++) {
             if(!isBlocked(caller, connections[i].remote_addr) && connections[i].connsockfd != caller){
                 if(connections[i].status == loged_in){
                     send(connections[i].connsockfd, msg, BUFLEN, 0);
                     connections[i].msg_received++;
                     //triger event
					 cse4589_print_and_log("[%s:SUCCESS]\n" , "RELAYED");
					 fflush(stdout);
					 cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender, "255.255.255.255", msg);
					 fflush(stdout);
					 cse4589_print_and_log("[%s:END]\n", "RELAYED");
					 fflush(stdout);
                 }else{
 //                    strcat(bufferedmsg, "255.255.255.255"); mark
                     strcat(bufferedmsg, connections[i].remote_addr);
                     strcat(bufferedmsg, "---");
                     strcat(bufferedmsg, sender);
                     strcat(bufferedmsg, "---");
                     strcat(bufferedmsg, arguments[1]);
                     strcat(bufferedmsg, "---");
                     connections[i].msg_received++;
                 }
             }
         }
     }else if(strcmp(arguments[0], "BLOCK") == 0){

         int target;
         for(target=0;target<connIndex;target++){
             if(connections[target].connsockfd == caller) break;
         }

         int found = 0;
         int blocking;
         for (blocking = 0; blocking< connIndex; blocking++) {
             if(strcmp(connections[blocking].remote_addr, arguments[1])== 0) {
                 found = 1;
                 break;
             }
         }

         if(found == 0){
             send(caller, "FAIL", 4, 0);
             return;
         }

         if (connections[target].blockindex == 0) {
             //potential bug, if the blocking client already exited
             connections[target].blockedIPs[connections[target].blockindex++] = &connections[blocking];
         }else{
             for(int i=0;i<connections[target].blockindex; i++){
                 if(strcmp(connections[target].blockedIPs[i]->remote_addr, arguments[1]) == 0){
                     send(caller, "FAIL", 4, 0);
                     return;
                 }
             }
             connections[target].blockedIPs[connections[target].blockindex++] = &connections[blocking];

             //sort the block list
             if(connections[target].blockindex > 1){
                 for(int i=0;i<connections[target].blockindex-1;i++){
                     for (int j=i; j<connections[target].blockindex; j++) {
                         if(connections[target].blockedIPs[i]->portNum > connections[target].blockedIPs[j]->portNum){
                             struct connection *tmp = connections[target].blockedIPs[i];
                             connections[target].blockedIPs[i] = connections[target].blockedIPs[j];
                             connections[target].blockedIPs[j] = tmp;
                         }
                     }
                 }
             }
         }
         send(caller, "SUCCESS", 7, 0);

     }else if(strcmp(arguments[0], "UNBLOCK") == 0){
         int target = 0;
         for(target = 0; target<connIndex;target++){
             if(connections[target].connsockfd == caller) break;
         }

         if(connections[target].blockindex == 0){
             send(caller, "FAIL", 4, 0);
             return;
         }

         int flag = 0;
         for(int i=0;i<connections[target].blockindex;i++){
             if(strcmp(connections[target].blockedIPs[i]->remote_addr, arguments[1]) == 0){
                 if(i == 2){
                     connections[target].blockindex--;
                     flag = 1;
                     break;
                 }
                 for(int j=i+1;j<connections[target].blockindex;j++){
                     connections[target].blockedIPs[j-1] = connections[target].blockedIPs[j];
                 }
                 flag = 1;
                 connections[target].blockindex--;
                 break;
             }
         }
         if(flag == 0){
             send(caller, "FAIL", 4, 0);
             return;
         }
         send(caller, "SUCCESS", 7, 0);

     }else if(strcmp(arguments[0], "EXIT") == 0){
         int target;
         for(target=0;target<connIndex;target++){
             if(connections[target].connsockfd == caller){
                 close(connections[target].connsockfd);
                 FD_CLR(connections[target].connsockfd, &master);
                 if(target == 3){
                     connIndex--;
                     break;
                 }
                 for(int j=target+1;j<connIndex;j++){
                     connections[j-1] = connections[j];
                 }
                 connIndex--;

                 break;
             }
         }
     }else if(strcmp(arguments[0], "LOGOUT") == 0){
         for(int i=0;i<connIndex;i++){
             if(connections[i].connsockfd == caller){
                 close(connections[i].connsockfd);
                 FD_CLR(connections[i].connsockfd, &master);
                 connections[i].status = loged_out;
                 break;
             }
         }
     }


 }

 /*
  * Start select for cmd and connecting.
  */
 void start(void){
     char *argm[5];

     FD_ZERO(&master);
     FD_ZERO(&read_fds);
     FD_SET(STD_IN, &master);
     FD_SET(localsockfd, &master);
     maxfd = localsockfd;

     while (1) {
         read_fds = master;
         if(select(maxfd+1, &read_fds, NULL, NULL, NULL) == -1){
             return;
         }

         for(int i=0 ;i < maxfd+1; i++){
             if(FD_ISSET(i, &read_fds)){
                 // collect input
                 if(i == STD_IN){
                     char *cmd = (char *)malloc(sizeof(char)*CMD_SIZE);
                     memset(cmd, '\0', CMD_SIZE);
                     fgets(cmd, CMD_SIZE-1, stdin);
                     for(int j =0; j<CMD_SIZE; j++){
                         if(cmd[j] == '\n'){
                             cmd[j] = '\0';
                             break;
                         }
                     }

                     int count = 0;
                     char *tmp = strtok(cmd, " ");
                     while(tmp != NULL){
                         argm[count++] = tmp;
                         tmp = strtok(NULL, " ");
                     }

                     processCmd(argm, count);
                 }else if(i == localsockfd && role == 1){
                     // process new connections, use a data structure to store info
                     struct sockaddr_storage remoteaddr;
                     socklen_t len = sizeof(remoteaddr);
                     int newfd = accept(localsockfd, (struct sockaddr *)&remoteaddr, &len);
                     if(newfd == -1){
 //                        flag = -1;
                         continue;
                     }
                     FD_SET(newfd, &master);
                     maxfd = maxfd > newfd? maxfd: newfd;

					 char clientPort[PORTSTRLEN];
					 // bug: different length between client and server
					 recv(newfd, clientPort, PORTSTRLEN, 0);
                     char tmp[INET_ADDRSTRLEN];
                     inet_ntop(AF_INET, &(((struct sockaddr_in *)&remoteaddr)->sin_addr), tmp, INET_ADDRSTRLEN);
                     struct hostent *he;
                     struct in_addr ipv4addr;
                     inet_pton(AF_INET, tmp, &ipv4addr);
                     he = gethostbyaddr(&ipv4addr, sizeof(struct in_addr), AF_INET);
                     int exist = 0;
                     for(int i=0;i<connIndex;i++){
                         if(strcmp(connections[i].remote_addr, tmp) == 0){
                             exist = 1;
                             connections[i].status = loged_in;
                             break;
                         }
                     }
                     if(!exist){
                         struct connection newConnection;
                         newConnection.connsockfd = newfd;
                         strcpy(newConnection.remote_addr, tmp);
                         //newConnection.portNum = ((struct sockaddr_in *)&remoteaddr)->sin_port;
						 newConnection.portNum = strToNum(clientPort);
                         strcpy(newConnection.hostname, he->h_name);
                         newConnection.msg_sent = 0;
                         newConnection.msg_received = 0;
                         newConnection.status = loged_in;
                         newConnection.blockindex = 0;
                         connections[connIndex++] = newConnection;
                     }
                     // sort the array
                     if(connIndex > 1){
                         for(int cur = 0; cur< connIndex-1; cur++){
                             for(int fast = cur+1; fast<connIndex; fast++){
                                 if(connections[cur].portNum > connections[fast].portNum){
                                     struct connection tmp = connections[cur];
                                     connections[cur] = connections[fast];
                                     connections[fast] = tmp;
                                 }
                             }
                         }
                     }
                     // afterwards redirect the array to newly connected client
                     char list[BUFLEN] = "";
                     packList(list);
                     send(newfd, list, BUFLEN, 0);


                     //prepare buffered msg for that client
                     int count = 0;
                     char *bufmsg[BUFLEN];
                     char *p;
                     p = strtok(bufferedmsg, "---");
                     while (p!=NULL) {
                         bufmsg[count++] = p;
                         p = strtok(NULL, "---");
                     }

                     char newBufferedmsg[BUFLEN]="";
                     char sendingmsg[BUFLEN]="";

					 int flag = 0;
                     for (int i=0; i<count; ) {
                         if (strcmp(tmp, bufmsg[i]) == 0) {
							 if(flag == 0){
								 strcat(sendingmsg, "BROADCAST,");
								 strcat(sendingmsg, "---");
								 flag = 1;
							 }
                             strcat(sendingmsg, bufmsg[i+1]);
                             strcat(sendingmsg, "---");
                             strcat(sendingmsg, bufmsg[i+2]);
                             strcat(sendingmsg, "---");
							 cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
							 fflush(stdout);
							 cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", bufmsg[i+1], bufmsg[i], bufmsg[i+2]);
							 fflush(stdout);
							 cse4589_print_and_log("[%s:END]\n", "RELAYED");
							 fflush(stdout);
							 i+=3;
                         } else if(strcmp(bufmsg[i], "255.255.255.255") == 0){
							 if(flag == 0){
								 strcat(sendingmsg, "BROADCAST,");
								 strcat(sendingmsg, "---");
								 flag = 1;
							 }
                             strcat(newBufferedmsg, "255.255.255.255");
                             strcat(newBufferedmsg, "---");
                             strcat(newBufferedmsg, bufmsg[i+1]);
                             strcat(newBufferedmsg, "---");
                             strcat(newBufferedmsg, bufmsg[i+2]);
                             strcat(newBufferedmsg, "---");

                             strcat(sendingmsg, bufmsg[i+1]);
                             strcat(sendingmsg, "---");
                             strcat(sendingmsg, bufmsg[i+2]);
                             strcat(sendingmsg, "---");
							 cse4589_print_and_log("[%s:SUCCESS]\n", "RELAYED");
							 fflush(stdout);
							 cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", bufmsg[i+1], bufmsg[i], bufmsg[i+2]);
							 fflush(stdout);
							 cse4589_print_and_log("[%s:END]\n", "RELAYED");
							 fflush(stdout);
							 i+=3;
                         } else{
                             strcat(newBufferedmsg, bufmsg[i++]);
                             strcat(newBufferedmsg, "---");
                             strcat(newBufferedmsg, bufmsg[i++]);
                             strcat(newBufferedmsg, "---");
                             strcat(newBufferedmsg, bufmsg[i++]);
                             strcat(newBufferedmsg, "---");
                         }
                     }
                     strcpy(bufferedmsg, newBufferedmsg);
				
                     send(newfd, sendingmsg, BUFLEN, 0);

                 }else if(role == 0 && i == clientsockfd){
 			         char buf[BUFLEN];
                     int nbytes = recv(i, buf, sizeof buf, 0);
					 
					 char flag[10]="";
					if(nbytes > 1){
						strncpy(flag, buf, 10);
					 }

					 if(strcmp(flag, "BROADCAST,") == 0){
						char *tmp;
	 					int count = 0;
	 					char *msgset[BUFLEN];
	 					tmp = strtok(buf, "---");
	 					while (tmp != NULL) {
	 						msgset[count++] = tmp;
	 						tmp = strtok(NULL, "---");
	 					}
					

	 					for(int i=1;i<count;){
	 						cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
	 						fflush(stdout);
	 						cse4589_print_and_log("msg from:%s\n[msg]:%s\n", msgset[i], msgset[i+1]);
	 						fflush(stdout);
	 						cse4589_print_and_log("[%s:END]\n", "RECEIVED");
	 						fflush(stdout);
	 						i+=2;
	 					}


					}else{
	                     char *tmp;
	                     int count = 0;
	                     char *msgset[BUFLEN];
	                     tmp = strtok(buf, " ");
	                     while (tmp != NULL) {
	                         msgset[count++] = tmp;
	                         tmp = strtok(NULL, " ");
	                     }

						 char recvmsg[MSGLEN] = "";
						 for(int n = 1; n< count; n++){
							strcat(recvmsg, msgset[n]);
							if(n<count-1) strcat(recvmsg, " ");
						 }

						 cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
						 fflush(stdout);
						 cse4589_print_and_log("msg from:%s\n[msg]:%s\n", msgset[0], recvmsg);
						 fflush(stdout);
						 cse4589_print_and_log("[%s:END]\n", "RECEIVED");
						 fflush(stdout);
						 }
                     // triger event cse4589
                     // recv from server to receive msg from others
                 }else{
                     char buf[BUFLEN];
                     ssize_t nbytes = recv(i, buf, sizeof buf, 0);
                     buf[nbytes] = '\0';

                     int count = 0;
                     char *p;
                     char *arguments[BUFLEN];
                     p = strtok(buf, " ");
                     while (p != NULL) {
                         arguments[count++] = p;
                         p = strtok(NULL, " ");
                     }

					 char msg[MSGLEN] = "";
					 if(strcmp(arguments[0], "SEND") == 0){
						 for(int i=2; i<count; i++){
							strcat(msg, arguments[i]);
							if(i < count-1) strcat(msg, " ");
						 }
						 count = 3;
						 arguments[count-1] = msg;
					 }

					 if(strcmp(arguments[0], "BROADCAST") == 0){
						for(int i=1; i<count; i++){
							strcat(msg, arguments[i]);
							if(i < count-1) strcat(msg, " ");
						 }
						 count = 2;
						 arguments[count-1] = msg;
					 }
					 //debug
					 //printf("%s\n%s\n", msg, arguments[count]);

					// for(int l = 0; l < count; l++){
					//	printf("%s--- ", arguments[l]);
					 //}
					 //printf("\n");

 		            response(arguments, count, i);
                     // recv from connected socket, also need to consider argument,send, refresh or block.

                 }
             }
         }
     }
 }
 /**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	if (argc != 3) {
		printf("usage: ./chat_app s/c #port\n");
		return 1;
	}

	if(strcmp(argv[1], "s")==0) role = 1;
	else if(strcmp(argv[1], "c")==0) role = 0;
	else return -1; //invalid argument
	strcpy(listenerPort, argv[2]);

	prep(argv[2]);
	start();

	return 0;
}
