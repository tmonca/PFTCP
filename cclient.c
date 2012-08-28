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

#include <fcntl.h>

#include <openssl/sha.h>

#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#define TCPBUFFSIZE 20  /*A compromise length*/
#define UDPBUFFSIZE 1400 /*fixed for now*/
#define UDPDGRAM 1400

#define OPEN "open"
#define CLOSE "close"
#define RESEND "resend"
#define CONNECT "connect"
#define CHECK "check"

void Die(char *mess) { perror(mess); exit(1); }

int main(int argc, char *argv[]) {
	/*variables for TCP*/
	
	 if (argc != 4) {
  	fprintf(stderr, "USAGE: TCPecho <server_ip> <port> <frequency of acks>\n");
  	exit(1);
    }
	
	int sock_tcp;
	struct sockaddr_in tcp_echoserver;
	char out_buffer[TCPBUFFSIZE];
	char command[TCPBUFFSIZE];

	char UPort[5];
	unsigned int tcp_echolen;
	
	int pointer;
	
	/*variables for the UDP sender*/
  int sock_udp;
  int ackPer = atoi(argv[3]);
  struct sockaddr_in udp_echoserver;
  struct sockaddr_in udp_echoclient;
  char reply_buffer[TCPBUFFSIZE];
  char buffer[TCPBUFFSIZE];
  unsigned char data[UDPBUFFSIZE];
  unsigned char tmpBuff[ackPer][UDPBUFFSIZE];
  unsigned char check[20];
  unsigned int udp_echolen, clientlen, serverlen;
  int received = 0;
  int UDPport = 0;
  
/*I need a sliding buffer to send the data out

Needs to be addressable by ACK number for 
retransmission. Or am I going to just send
FEC data instead? */

  int j;
  int32_t fh = 0;
  const char* filename = "in.txt";
  if((fh = open(filename, O_RDONLY)) == -1){
    Die("Couldn't open the file");
  }
  long fpoint = 0;
	/*Check for correct usage*/

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
	tcp_echoserver.sin_port = htons(atoi(argv[2]));
	
	/* Bind the UDP socket */

	/* Establish TCP connection */
	if (connect(sock_tcp, (struct sockaddr *) &tcp_echoserver, sizeof(tcp_echoserver)) < 0) {
  		Die("Failed to connect with TCP server");
	}
/* Send the command to the server */
	memset(&buffer, 0, TCPBUFFSIZE);
	strcpy(buffer, OPEN);
	printf("buffer is %s.\n", buffer);
	strcpy(&buffer[4], " ");
		printf("now buffer is %s.\n", buffer);
	strcpy(&buffer[5], argv[3]);
	printf("and now buffer is %s\n", buffer);
	if (send(sock_tcp, buffer, sizeof(buffer), MSG_MORE) < 1) {
  		Die("Failed sending TCP open - wrong no sent bytes");
	}	
/* Receive the word back from the server */
	fprintf(stdout, "Received: ");
	int bytes = 1;
/*Find out which port to use for UDP*/
	while (UDPport == 0) {
    bytes = recv(sock_tcp, command, TCPBUFFSIZE-1, 0); 
  	command[bytes] = '\0';        /* Assure null terminated string */
  	fprintf(stdout,"We saw % d bytes saying %s \n",bytes,  command);
  	
  	if(strncmp(UPort, "U", 1)) {
  		long tmp;
  		char * pEnd;
  		tmp = strtol(command+1, &pEnd, 10 );
  		UDPport = (int) tmp;
  		fprintf(stdout, "We are told to use UDP port %d\n", UDPport);
  	}
  	else{
  		/*So this was a message with UDP details. We need to parse the next bytes which
  		tell us which port is open*/
  		
  		fprintf(stdout, "not a U\n");
  	}
  	bytes = 0;
	}

  if ((sock_udp = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
  	Die("Failed to create socket");
  }

/* Construct the server sockaddr_in structure */
	memset(&udp_echoserver, 0, sizeof(udp_echoserver));       /* Clear struct */
	udp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
	udp_echoserver.sin_addr.s_addr = inet_addr(argv[1]); /* IP address */
	udp_echoserver.sin_port = htons(UDPport);       /* server port */
	bytes = -1;
	int remains = -1;
	int counter = 0;
	do{
   	bytes = read(fh, data, UDPBUFFSIZE);  
 		if(bytes != UDPBUFFSIZE){
 			remains = bytes;
 			printf("set reamins to %d, bytes was %d\n", remains, bytes);
			if ((sendto(sock_udp, data, remains, 0, (struct sockaddr *) &udp_echoserver, sizeof	(udp_echoserver))) != remains) {
	    		Die("Mismatch in number of sent bytes");
			}
			counter++;
		}
  	else{
  		if ((sendto(sock_udp, data, UDPBUFFSIZE, 0, (struct sockaddr *) &udp_echoserver, sizeof	(udp_echoserver))) != UDPBUFFSIZE) {
	    		Die("Mismatch in number of sent bytes");
	    }
	  	counter++;
		}
		strcpy(tmpBuff[counter], data);
	//printf(" count %d", counter);
	}
	while(counter < ackPer);
	printf(" count %d\n", counter);
	while(1){
		bytes = recv(sock_tcp, command, TCPBUFFSIZE-1, 0); 
  	command[bytes] = '\0';        /* Assure null terminated string */
  	fprintf(stdout,"We saw % d bytes saying %s \n",bytes,  command);
  }
  close(sock_udp);
  close(sock_tcp);
  close(fh);
  exit(0);
}


