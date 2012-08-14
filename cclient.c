/*******************************************
*
* receives data via TCP, sends UDP reply


Modify to use TCP as control channel
Send data chunk. ACK with count and cksum

If ACK is wrong we re-send the chunk

This prog needs to
open TCP session on port xyz
Then listen for reply with details of UDP
then open UDP socket
NB later on we can add security here like MPTCP

Start sending data, listen for ACKs on TCP
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
#define TCPBUFFSIZE 64  /*A compromise length*/
#define UDPBUFFSIZE 1400 /*fixed for now*/

#define OPEN "open"
#define CLOSE "close"
#define RESEND "resend"
#define CONNECT "connect"

void Die(char *mess) { perror(mess); exit(1); }

int main(int argc, char *argv[]) {
	/*variables for TCP*/
	int sock_tcp;
	struct sockaddr_in tcp_echoserver;
	char out_buffer[TCPBUFFSIZE];
	char command[TCPBUFFSIZE];
	char UPort[5];
	unsigned int tcp_echolen;

	
	/*variables for the UDP sender*/
  int sock_udp;
  struct sockaddr_in udp_echoserver;
  struct sockaddr_in udp_echoclient;
  char reply_buffer[TCPBUFFSIZE];
  char buffer[TCPBUFFSIZE];
  unsigned int udp_echolen, clientlen, serverlen;
  int received = 0;
  int UDPport = 0;
  
/*I need a sliding buffer to send the data out

Needs to be addressable by ACK number for 
retransmission. Or am I going to just send
FEC data instead? */

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
	memset(&buffer, 0, TCPBUFFSIZE);
	strcpy(buffer, OPEN);

	if (send(sock_tcp, buffer, sizeof(buffer), 0) < 1) {
  		Die("Mismatch in number of sent bytes");
	}
	
/* Receive the word back from the server */
	fprintf(stdout, "Received: ");
	  int bytes = 1;
	while (UDPport == 0) {
      
 /*
 		Use TCP as control. Once UDP details passed start sending data.
 		Send the data in blocks of 10 datagrams.
 		After 10th wait for ACK, then clear, send next 10.
 		When \EOF send CLOSE
 */
  
    bytes = recv(sock_tcp, command, TCPBUFFSIZE-1, 0);
    
  	command[bytes] = '\0';        /* Assure null terminated string */
  	fprintf(stdout,"We saw % d bytes saying %s \n",bytes,  command);
  	
  	if(strncmp(UPort, "U", 1)) {
  		long tmp;
  		char * pEnd;
  		tmp = strtol(command+1, &pEnd, 10 );
  		UDPport = (int) tmp;
  		fprintf(stdout, "We see port %d", UDPport);
  	}
  	else{
  		/*So this was a message with UDP details. We need to parse the next bytes which
  		tell us which port is open*/
  		
  		fprintf(stdout, "not a U\n");
  	}
  	bytes = 0;
	}
	
	/*So now we know that UDPport is the port tha the other end is listening on
	What is the best approach to sending the actual data? Probably need to 
	divide it into functions. Start by just sending a big blob of data and getting
	an ACK back...
	*/
	
  	fprintf(stdout, "\n");
  	close(sock_udp);
  	close(sock_tcp);
  	exit(0);
}



