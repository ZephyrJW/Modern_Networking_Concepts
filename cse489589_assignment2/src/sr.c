#include "../include/simulator.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/
/*
 * note about get_sim_time(): 
 * this function returns the absolute time of this simulator rather 
 * than giving us the relative time between every starttimer()
 */

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
#define BUFLEN 1000
#define TIMEOUT_INTERVAL 30.0
#define window_size getwinsize()

struct srpkt{
  struct pkt content;
  float timer;
  int acked; // 0- not yet, 1- acked
};

/*sender part*/
int sndbase = 0;
int nextseqnum = 0; 
//int window_size = window_size; // how to set this parameter?

struct srpkt sndpkt[2000];

/*receiver part*/
int recvbase = 0;
struct pkt recvbuffer[2000];

/*general part*/
char buffer[BUFLEN][20];
int buffered = 0;
int buffer_head = 0;
int buffer_tail = 0;

struct pkt A_pkt;
struct pkt B_ack;


/*Checksum, citation: http://www.csee.usf.edu/~kchriste/tools/checksum.c*/
unsigned short checksum(unsigned short *target, int len){
  unsigned int sum = 0;

  while(len > 1){
    sum += *target++;
    len -= 2;
  }

  if(len){
    sum += *(unsigned char *)target;
  }

  while(sum >> 16){
    sum = (sum >> 16) + (sum & 0xffff);
  }

  return (unsigned short)(~sum);
}

/*Using checksum for corruptions detection*/
/*If no flip-bit, the check_sum would be zero*/
int corrupt(struct pkt packet){
  unsigned short check_sum;
  check_sum = checksum((unsigned short *)&packet, sizeof(struct pkt));
  if(check_sum) return 1; // corrupted
  return 0; // correct
}

/* called from layer 5, passed the data to be sent to other side */
void A_output(message)
  struct msg message;
{
  if(nextseqnum < sndbase + window_size){
    if(buffered == 0){
      A_pkt.seqnum = nextseqnum;
      A_pkt.acknum = 0;
      A_pkt.checksum = 0;
      memcpy(A_pkt.payload, message.data, 20);
      A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

      struct srpkt tmp;
      tmp.content.seqnum = A_pkt.seqnum;
      tmp.content.acknum = A_pkt.acknum;
      tmp.content.checksum = A_pkt.checksum;
      memcpy(tmp.content.payload, A_pkt.payload, 20);
      tmp.acked = 0;
      tmp.timer = get_sim_time() + TIMEOUT_INTERVAL;

      sndpkt[nextseqnum] = tmp;

      tolayer3(0, tmp.content);
      nextseqnum++;

    }else{
      int flag = 0;
      while(nextseqnum < sndbase+ window_size && buffered > 0){
        A_pkt.seqnum = nextseqnum;
        A_pkt.acknum = 0;
        A_pkt.checksum = 0;
        memcpy(A_pkt.payload, buffer[buffer_head], 20);
        A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

        buffer_head = (buffer_head + 1) % BUFLEN;
        buffered--;

        struct srpkt tmp;
        tmp.content.seqnum = A_pkt.seqnum;
        tmp.content.acknum = A_pkt.acknum;
        tmp.content.checksum = A_pkt.checksum;
        memcpy(tmp.content.payload, A_pkt.payload, 20); 
        tmp.acked = 0;
        tmp.timer = get_sim_time() + TIMEOUT_INTERVAL;

        sndpkt[nextseqnum] = tmp;
        if(flag == 0){
          memcpy(buffer[buffer_tail], message.data, 20);
          buffer_tail = (buffer_tail+1)%BUFLEN;
          buffered++;
          flag = 1;
        }
        tolayer3(0, tmp.content);     
        nextseqnum++;

      }
    }
  }else{
    if(buffered < BUFLEN){
      memcpy(buffer[buffer_tail], message.data, 20);
      buffer_tail = (buffer_tail+1)%BUFLEN;
      buffered++;
    }
  }
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  if(corrupt(packet) == 0){
    sndpkt[packet.acknum].acked = 1;

    if(packet.acknum == sndbase){
      while(sndpkt[sndbase].acked == 1){
        sndbase++;
      }
    }

    while(buffered > 0 && nextseqnum < sndbase + window_size){
      A_pkt.seqnum = nextseqnum;
      A_pkt.acknum = 0;
      A_pkt.checksum = 0;
      memcpy(A_pkt.payload, buffer[buffer_head], 20);
      A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

      buffer_head = (buffer_head + 1) % BUFLEN;
      buffered--;

      struct srpkt tmp;
      //tmp.content = A_pkt;
      tmp.content.seqnum = A_pkt.seqnum;
      tmp.content.acknum = A_pkt.acknum;
      tmp.content.checksum = A_pkt.checksum;
      memcpy(tmp.content.payload, A_pkt.payload, 20);
      tmp.acked = 0;
      tmp.timer = get_sim_time() + TIMEOUT_INTERVAL;

      sndpkt[nextseqnum] = tmp;

      tolayer3(0, tmp.content);
      nextseqnum++;
    }
  } 

}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  for(int i= sndbase; i< nextseqnum; i++){
    if(sndpkt[i].acked == 0 && sndpkt[i].timer <= get_sim_time()){
        sndpkt[i].timer = get_sim_time() + TIMEOUT_INTERVAL;
        tolayer3(0, sndpkt[i].content);
    }
  } 
  starttimer(0, 5.0);
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  sndbase = 1;
  nextseqnum = 1; 
  starttimer(0, 5.0);
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
  if(corrupt(packet) == 0){
    if(packet.seqnum <= recvbase - 1 && packet.seqnum >= recvbase - window_size){
    B_ack.seqnum = 0;
    B_ack.acknum = packet.seqnum;
    B_ack.checksum = 0;
    memset(B_ack.payload, 0, 20);
    B_ack.checksum = checksum((unsigned short *)&B_ack, sizeof(struct pkt));

    tolayer3(1, B_ack);
    }else{
      B_ack.seqnum = 0;
      B_ack.acknum = packet.seqnum;
      B_ack.checksum = 0;
      memset(B_ack.payload, 0, 20);
      B_ack.checksum = checksum((unsigned short *)&B_ack, sizeof(struct pkt));

      tolayer3(1, B_ack);
      
      recvbuffer[packet.seqnum] = packet;
      if(packet.seqnum == recvbase){
        while(recvbuffer[recvbase].seqnum != 0){
          tolayer5(1, recvbuffer[recvbase].payload);
          recvbase++;
        }
      }
    } 
  }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  recvbase = 1;
}
