#include "../include/simulator.h"
#include <stdio.h>
#include <string.h>
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
*********************************************************************/

/********* STUDENTS WRITE THE NEXT SIX ROUTINES *********/
#define BUFLEN 1000
#define TIMEOUT_INTERVAL 10.0

int A_status = 0; // 0,1,2,3
// 0: wait for call 0 , 1: wait for ack 0, 2: wait for call 1, 3: wait for ack 1
int B_status = 0; 
// 0: wait for pkt0, 1: wait for pkt1
// bool inTransit = false;
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
  if(A_status == 0 || A_status == 2){
    if(buffered == 0){
      A_pkt.seqnum = (A_status == 0)? 0:1;
      A_pkt.acknum = 0;
      A_pkt.checksum = 0;
      //strcpy(A_pkt.payload, message.data);
      memcpy(A_pkt.payload, message.data, 20);
      A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt)); // !!! 

      tolayer3(0, A_pkt);
      //debug
      //printf("A send %s\n", A_pkt.payload);
      // inTransit = true;
      A_status = (A_status + 1) % 4;
      starttimer(0, TIMEOUT_INTERVAL);
    }else{
      // send previous buffered msg if there's buffered msg
      A_pkt.seqnum = (A_status == 0)? 0:1;
      A_pkt.acknum = 0;
      A_pkt.checksum = 0;
      //strcpy(A_pkt.payload, buffer[buffer_head]);
      memcpy(A_pkt.payload, buffer[buffer_head], 20);
      A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt)); // !!! 

      buffer_head = (buffer_head+1)%BUFLEN;
      buffered--;
      // buffer this message
      //strcpy(buffer[buffer_tail], message.data);
      memcpy(buffer[buffer_tail], message.data, 20);
      buffer_tail = (buffer_tail+1)%BUFLEN;
      buffered++;

      tolayer3(0, A_pkt);
      //debug
      //printf("A send: %s\n", A_pkt.payload);
      // inTransit = true;
      A_status = (A_status + 1) % 4;
      starttimer(0, TIMEOUT_INTERVAL); 
    }

  }else{
    //buffer
    if(buffered < BUFLEN){
      //strcpy(buffer[buffer_tail], message.data);
      memcpy(buffer[buffer_tail], message.data, 20);
      buffer_tail = (buffer_tail+1) % BUFLEN;
      buffered++;
    }
  }

}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  if(A_status == 1){
    if(packet.acknum == 0 && corrupt(packet) == 0){
      if(buffered == 0){
       stoptimer(0);
       A_status = (A_status + 1) % 4;
      // /*
      }else{
        stoptimer(0);
        A_pkt.seqnum = ((A_status+1)%4 == 0)? 0:1;
        A_pkt.acknum = 0;
        A_pkt.checksum = 0;
        //strcpy(A_pkt.payload, buffer[buffer_head]);
        memcpy(A_pkt.payload, buffer[buffer_head], 20);
        A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

        buffer_head = (buffer_head + 1) % BUFLEN;
        buffered--;

        tolayer3(0, A_pkt);
        starttimer(0, TIMEOUT_INTERVAL);
        A_status = (A_status+2)%4;
      }
     // */
    }else{
     // wait for timeout 
    } 
  }else if(A_status == 3){
    if(packet.acknum == 1 && corrupt(packet) == 0){
      if(buffered == 0){
        stoptimer(0);
        A_status = (A_status + 1) % 4;
      ///*
      }else{
        stoptimer(0);
        A_pkt.seqnum = ((A_status+1)%4 == 0)? 0:1;
        A_pkt.acknum = 0;
        A_pkt.checksum = 0;
        //strcpy(A_pkt.payload, buffer[buffer_head]);
        memcpy(A_pkt.payload, buffer[buffer_head], 20);
        A_pkt.checksum = checksum((unsigned short *)&A_pkt, sizeof(struct pkt));

        buffer_head = (buffer_head+1) % BUFLEN;
        buffered--;

        tolayer3(0, A_pkt);
        starttimer(0, TIMEOUT_INTERVAL);
        A_status = (A_status+2)%4;
      } 
      //*/

    }
  }

}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  tolayer3(0, A_pkt);
  starttimer(0, TIMEOUT_INTERVAL);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  A_status = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
  if(packet.seqnum == B_status && corrupt(packet) == 0){
    tolayer5(1, packet.payload);
    //debug
    //printf("B recvs: %s\n", packet.payload);

    B_ack.seqnum = 0;
    B_ack.acknum = B_status;
    B_ack.checksum = 0;
    //strcpy(B_ack.payload, "");
    memset(B_ack.payload, 0, 20);
    B_ack.checksum = checksum((unsigned short *)&B_ack, sizeof(struct pkt));

    tolayer3(1, B_ack);
    B_status = (B_status + 1) % 2;
  } else{
    B_ack.seqnum = 0;
    B_ack.acknum = (B_status+1)%2;
    B_ack.checksum = 0;
    //strcpy(B_ack.payload, "");
    memset(B_ack.payload, 0, 20);
    B_ack.checksum = checksum((unsigned short *)&B_ack, sizeof(struct pkt));

    tolayer3(1, B_ack);
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  B_status = 0;
}
