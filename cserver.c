
/*******************************************
*
* receives data via TCP, sends UDP reply

Modify to use TCP as control channel
Send data chunk. ACK with count and cksum

If ACK is wrong we re-send the chunk

This prog needs to
passive open TCP session on port xyz
then open UDP socket
send details of UDP socket to client via TCP
Listen for connection on UDP
NB later on we can add security here like MPTCP

Once data flows on UDP it sends back ACKs
question: coping with re-ordering?

Other possibility would be to use FEC, etc
Once all chunks sent sender sends as much 
correction data as reported loss...

*
********************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#define MAXPENDING 5    /* Max connection requests */
#define BUFFSIZE 64

void Die(char *mess) { perror(mess); exit(1); }

void HandleClient(int rcvsock) {
  char buffer[BUFFSIZE];
  char command[7];
  char sum, tmp, tmp2;
	int udp_clientsock;
	struct sockaddr_in udp_echoserver;
  int received = -1;
  /* Receive message */
  if ((received = recv(rcvsock, buffer, BUFFSIZE, 0)) < 0) {
    Die("Failed to receive initial bytes from client");
  }
  fprintf(stderr, "Received %s \n", buffer);
  
  /* Create the UDP socket 
	if ((udp_clientsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
  	Die("Failed to create socket");*/
/*	}
/* Construct the UDP server sockaddr_in structure */
/*	memset(&udp_echoserver, 0, sizeof(udp_echoserver));        Clear struct */
/*	udp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
/*	udp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Any IP address */
/*	udp_echoserver.sin_port = htons(1313);  								/*hard wire the UDP to port 1313*/
  
  while (received > 0) {
    /* Send back received data */
    strcpy(command, "U 1313");
    command[6]  = '\0';
    if (received = send(rcvsock, command, 7, 0) != 7) {
  		Die("Failed to send TCP command in reply");
		}
		fprintf(stderr, "sent back %s \n", command);
    received = 0;
  }
  close(rcvsock);
  //close(udp_clientsock);
}

int main(int argc, char *argv[]) {
  int tcp_serversock, tcp_clientsock;
  struct sockaddr_in tcp_echoserver, tcp_echoclient;
  if (argc != 2) {
    fprintf(stderr, "USAGE: echoserver <port>\n");
    exit(1);
  }
  /* Create the TCP socket */
  if ((tcp_serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    Die("Failed to create socket");
  }
  /* Construct the TCP server sockaddr_in structure */
  memset(&tcp_echoserver, 0, sizeof(tcp_echoserver));       /* Clear struct */
  tcp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
  tcp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Incoming addr */
  tcp_echoserver.sin_port = htons(atoi(argv[1]));       /* server port */

/* Bind the server socket */
	if (bind(tcp_serversock, (struct sockaddr *) &tcp_echoserver, sizeof(tcp_echoserver)) < 0) {
 		Die("Failed to bind the TCP server socket");
	}

/* Listen on the server socket */
	if (listen(tcp_serversock, MAXPENDING) < 0) {
  	Die("Failed to listen on server socket");
	}
  /* Run until cancelled */
  while (1) {
    unsigned int clientlen = sizeof(tcp_echoclient);
    /* Wait for client connection */
    if ((tcp_clientsock = accept(tcp_serversock, (struct sockaddr *) &tcp_echoclient, &clientlen)) < 0) {
      Die("Failed to accept client connection");
    }
    fprintf(stdout, "Client connected: %s\n", inet_ntoa(tcp_echoclient.sin_addr));
    
    HandleClient(tcp_clientsock);
  }
}

