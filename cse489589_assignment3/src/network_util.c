/**
 * @network_util
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
 * Network I/O utility functions. send/recvALL are simple wrappers for
 * the underlying send() and recv() system calls to ensure nbytes are always
 * sent/received.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

ssize_t recvALL(int sock_index, char *buffer, ssize_t nbytes)
{
    ssize_t bytes = 0;
    bytes = recv(sock_index, buffer, nbytes, 0);

    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += recv(sock_index, buffer+bytes, nbytes-bytes, 0);

    return bytes;
}

ssize_t sendALL(int sock_index, char *buffer, ssize_t nbytes)
{
    ssize_t bytes = 0;
    bytes = send(sock_index, buffer, nbytes, 0);

    if(bytes == 0) return -1;
    while(bytes != nbytes)
        bytes += send(sock_index, buffer+bytes, nbytes-bytes, 0);

    return bytes;
}


int getlocalIP(uint32_t *ret){
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr myip;
    socklen_t len = sizeof myip;
    char res[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    //hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo("8.8.8.8", "53", &hints, &servinfo) != 0){
        printf("error code 1: getservinfo\n");
        return 1;
    }
    int conn_id;

    for (p = servinfo; p!=NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            printf("error: socket init\n");
            continue;
        }

        if ((conn_id = connect(sockfd, p->ai_addr, p->ai_addrlen)) == -1){
            close(sockfd);
            printf("error: connect\n");
            continue;
        }
        break;
    }

    if(p==NULL){
        printf("error code 2:connect error\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    getsockname(sockfd, &myip, &len);
    inet_ntop(AF_INET, &(((struct sockaddr_in *)&myip)->sin_addr), res, sizeof res);
    *ret = ((struct sockaddr_in *)&myip)->sin_addr.s_addr;
//    printf("ip addr: %s\n", res);

    struct hostent *he;
    struct in_addr ipv4addr;
    inet_pton(AF_INET, res, &ipv4addr);
    he = gethostbyaddr(&ipv4addr, sizeof ipv4addr, AF_INET);
 //   printf("HOST NAME: %s\n", he->h_name);

    close(sockfd);

    return 0;
}

/*http://blog.csdn.net/gdujian0119/article/details/6363574*/
unsigned long get_filesize(const char *path){
    unsigned long fsize = -1;
    struct stat statbuffer;
    if(stat(path, &statbuffer) < 0){
        return fsize;
    }else{
        fsize = statbuffer.st_size;
    }
    return fsize;
}
