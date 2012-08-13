/*******************************************
*
* Sends data via TCP, listens for UDP reply
*
********************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#define BUFFSIZE 64  /*A compromise length*/

void Die(char *mess) { perror(mess); exit(1); }

int main(int argc, char *argv[]) {
	/*variables for TCP*/
	int sock_tcp;
	struct sockaddr_in tcp_echoserver;
	char out_buffer[BUFFSIZE];
	unsigned int tcp_echolen;

	
	/*variables for the UDP listener*/
  int sock_udp;
  struct sockaddr_in udp_echoserver;
  struct sockaddr_in udp_echoclient;
  char reply_buffer[BUFFSIZE];
  unsigned int udp_echolen, clientlen, serverlen;
  int received = 0;
  int j;
  
	/*Check for correct usage*/
	if (argc != 4) {
  		fprintf(stderr, "USAGE: TCPecho <server_ip> <word> <port>\n");
  		exit(1);
	}

/* Create the TCP socket */
	if ((sock_tcp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
  		Die("Failed to create TCP socket");
  	}
/* Create the UDP socket */
	if ((sock_udp = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
  	Die("Failed to create UDP socket");
	}

	/* Construct the UDP server sockaddr_in structure */
	memset(&udp_echoserver, 0, sizeof(udp_echoserver));       /* Clear struct */
	udp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
	udp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Any IP address */
	/*udp_echoserver.sin_port = htons(atoi(argv[3])+1);        server port */
	udp_echoserver.sin_port = htons(13);
		
	serverlen = sizeof(udp_echoserver);
	if (bind(sock_udp, (struct sockaddr *) &udp_echoserver, serverlen) < 0) {
  	Die("Failed to bind UDP server socket");
	} 

	/* Construct the TCP server sockaddr_in structure */
	memset(&tcp_echoserver, 0, sizeof(tcp_echoserver));
	tcp_echoserver.sin_family = AF_INET;
	tcp_echoserver.sin_addr.s_addr = inet_addr(argv[1]);
	tcp_echoserver.sin_port = htons(atoi(argv[3]));
	
	/* Bind the UDP socket */


	/* Establish TCP connection */
	if (connect(sock_tcp, (struct sockaddr *) &tcp_echoserver, sizeof(tcp_echoserver)) < 0) {
  		Die("Failed to connect with TCP server");
	}
/* Send the word to the server */
	tcp_echolen = strlen(argv[2]);
	if (send(sock_tcp, argv[2], tcp_echolen, 0) != tcp_echolen) {
  		Die("Mismatch in number of sent bytes");
	}
	
/* Receive the word back from the server */
	fprintf(stdout, "Received: ");
	
	while (received < tcp_echolen) {

  	/*The following lines listen on the UDP socket for the reply*/
  	clientlen = sizeof(udp_echoclient);
    if ((received = recvfrom(sock_udp, reply_buffer, BUFFSIZE, 0, (struct sockaddr *) &udp_echoclient, &clientlen)) < 0) {
      Die("Failed to receive message");
    }
    fprintf(stderr, "Client connected: %s\n", inet_ntoa(udp_echoclient.sin_addr));
  		
  	reply_buffer[received] = '\0';        /* Assure null terminated string */
  	fprintf(stdout, reply_buffer);
	}
  	fprintf(stdout, "\n");
  	close(sock_udp);
  	close(sock_tcp);
  	exit(0);
}



