#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <math.h>
#define MAXPENDING 5    /* Max connection requests */
#define BUFFSIZE 64

void Die(char *mess) { perror(mess); exit(1); }

void HandleClient(int rcvsock) {
  char buffer[BUFFSIZE];
  char sum, tmp, tmp2;
	int udp_clientsock;
	struct sockaddr_in udp_echoserver;
  int received = -1;
  /* Receive message */
  if ((received = recv(rcvsock, buffer, BUFFSIZE, 0)) < 0) {
    Die("Failed to receive initial bytes from client");
  }
  fprintf(stderr, "Received %s \n", buffer);
  
  /* Create the UDP socket */
	if ((udp_clientsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
  	Die("Failed to create socket");
	}
/* Construct the UDP server sockaddr_in structure */
	memset(&udp_echoserver, 0, sizeof(udp_echoserver));       /* Clear struct */
	udp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
	udp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Any IP address */
	udp_echoserver.sin_port = htons(13);  								/*hard wire the UDP to port 13*/
  
  while (received > 0) {
    /* Send back received data */
    if (sendto(udp_clientsock, buffer, received, 0, (struct sockaddr *) &udp_echoserver, sizeof(udp_echoserver)) != received) {
  	Die("Failed to send UDP reply");
	}
    /* Check for more data */
    if ((received = recv(rcvsock, buffer, BUFFSIZE, 0)) < 0) {
      Die("Failed to receive additional bytes from client");
    }
  }
  close(rcvsock);
  close(udp_clientsock);
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

/* Bind the socket 
	if (bind(udp_serversock, (struct sockaddr *) &udp_echoserver, sizeof(udp_echoserver)) < 0) {
  	Die("Failed to bind UDP server socket");
	}*/
	
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


