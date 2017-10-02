#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

typedef enum {FALSE, TRUE} bool;

#define ERROR(err_msg) {perror(err_msg); exit(EXIT_FAILURE);}

/* https://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/ */
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)])) // Interesting stuff to read if you are interested to know how this works

#define INF 65535
#define DATA_PAYLOAD 1024

uint16_t CONTROL_PORT;

uint16_t numofrouters;
uint16_t update_interval;
uint16_t thisID;
uint16_t virtualIDseed;

bool justEstablished;

struct topology_entry
{
	uint16_t routerId;
    uint16_t virtualID;// serve as an index for storing
	// router port
	uint16_t port1;
	// data port
	uint16_t port2;
	uint16_t cost;
	uint32_t ip_addr;
	//char ip_addr[20];
};

struct topology_entry topmap[5];

int isNeighbor[5];
struct timeoutseq{
	time_t nextexpecttime;
	// 3 chances for 3 consective miss
	int chances;
};
struct timeoutseq timeoutSeq[5];


struct distanceVectorEntry {
	uint16_t cost;
	//uint16_t nexthop;
	int nexthop;
};

struct distanceVectorEntry distanceVector[5][5];

struct dataconn{
    int sockfd;
    uint32_t counterpart;
};
struct dataconn dataconnections[5];

//int topmap_index = 0;


#endif
