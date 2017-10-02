#include "../include/simulator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
#define BUFLEN 1000
#define TIMEOUT_INTERVAL 20.0
#define window_size getwinsize()

/*sender part*/
int base = 0;
int nextseqnum = 0; 
//int window_size = window_size; // how to set this parameter?

struct pkt sndpkt[100];

/*receiver part*/
int expectedseq = 0;

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
  if(nextseqnum < base + window_size){
    if(buffered == 0){
      A_pkt.seqnum = nextseqnum;
      A_pkt.acknum = 0;
      A_pkt.checksum = 0;
      memcpy(A_pkt.payload, message.data, 20); 
      A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

      struct pkt tmp = A_pkt;

      // potential bug..
      sndpkt[nextseqnum % window_size] = tmp;

      tolayer3(0, sndpkt[nextseqnum % window_size]);

      if(base == nextseqnum) starttimer(0, TIMEOUT_INTERVAL);

      nextseqnum++;
    }else{
      // send previous buffered and buffer this one
      // can be optimized by sending multiple buffered msg simultaneously
      A_pkt.seqnum = nextseqnum;
      A_pkt.acknum = 0;
      A_pkt.checksum = 0;
      memcpy(A_pkt.payload, buffer[buffer_head], 20);
      A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

      struct pkt tmp = A_pkt;

      buffer_head = (buffer_head+1)%BUFLEN;
      buffered--;
      sndpkt[nextseqnum % window_size] = tmp; 

      memcpy(buffer[buffer_tail], message.data, 20);
      buffer_tail = (buffer_tail+1)%BUFLEN; 
      buffered++;

      tolayer3(0, sndpkt[nextseqnum % window_size]);
      if(base == nextseqnum) starttimer(0, TIMEOUT_INTERVAL);
      nextseqnum++;

      // optimization
      while(nextseqnum < base + window_size && buffered != 0){
        A_pkt.seqnum = nextseqnum;
        A_pkt.acknum = 0;
        A_pkt.checksum = 0;
        memcpy(A_pkt.payload, buffer[buffer_head], 20);
        A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

        struct pkt tmp = A_pkt;

        buffer_head = (buffer_head+1)%BUFLEN;
        buffered--;
        sndpkt[nextseqnum % window_size] = tmp;

        tolayer3(0, sndpkt[nextseqnum % window_size]);
        if(base == nextseqnum) starttimer(0, TIMEOUT_INTERVAL);
        nextseqnum++;
      }
    }
  }else{
    //buffer
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
    base = packet.acknum + 1;
    if(base == nextseqnum) {
      stoptimer(0);
      /* is sending buffered necessary? doesn't hurt to check~ 
      while(nextseqnum < base + window_size && buffered != 0){
        A_pkt.seqnum = nextseqnum;
        A_pkt.acknum = 0;
        A_pkt.checksum = 0;
        memcpy(A_pkt.payload, buffer[buffer_head], 20);
        A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

        struct pkt tmp = A_pkt;

        buffer_head = (buffer_head+1)%BUFLEN;
        buffered--;
        sndpkt[nextseqnum % window_size] = tmp;

        tolayer3(0, sndpkt[nextseqnum % window_size]);
        if(base == nextseqnum) starttimer(0, TIMEOUT_INTERVAL);
        nextseqnum++;
      }
      */
    }
    else {
      stoptimer(0);
      starttimer(0, TIMEOUT_INTERVAL);
    } // should we first stoptimer(0) ??? 
  }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  starttimer(0, TIMEOUT_INTERVAL);
  for(int i = base; i< nextseqnum; i++){
    tolayer3(0, sndpkt[i % window_size]);
  }
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  base = 0;
  nextseqnum = 0;
  //window_size = window_size;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
  if(packet.seqnum == expectedseq && corrupt(packet) == 0){
    tolayer5(1, packet.payload);

    B_ack.seqnum = 0;
    B_ack.acknum = expectedseq;
    B_ack.checksum = 0;
    memset(B_ack.payload, 0, 20);
    B_ack.checksum = checksum((unsigned short *)&B_ack, sizeof(struct pkt));

    tolayer3(1, B_ack);
    expectedseq++;
  }
  else{  /*this is important for performance*/
    tolayer3(1, B_ack);
  }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  expectedseq = 0;
}
