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
#define TCPBUFFSIZE 60  /*A compromise length*/
#define UDPBUFFSIZE 1400 /*fixed for now*/
#define UDPDGRAM 1400

#define OPEN "open"
#define CLOSE "close"
#define RESEND "resend"
#define CONNECT "connect"
#define CHECK "check"
#define YES "yes"
#define NO "not"

void Die(char *mess) { perror(mess); exit(1); }

int checking(int TCPSock, unsigned char * buffer);

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
 	char data[UDPBUFFSIZE];
  char tmpBuff[ackPer][UDPBUFFSIZE];
  char checkBuffer[ackPer * UDPBUFFSIZE];
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
  		Die("Failed sending TCP open - wrong no. sent bytes");
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
	int end = -1;
	int counter = 0;
	int cumulate = 0;
	int UDPbytes = 0;
	int rtn = -666;
	int k;

	int sizeofUDP = sizeof(udp_echoserver);
	while(end != 1){
		for(j = 0; j < ackPer; j++){
			UDPbytes = read(fh, data, UDPBUFFSIZE); 
			memcpy(&checkBuffer[cumulate], data, UDPbytes);
			counter++;
			cumulate += UDPbytes;
			if(UDPbytes < UDPBUFFSIZE || counter%ackPer == 0){
				
				if(UDPbytes < UDPBUFFSIZE ){
					printf("got to the end with %d bytes\n", UDPbytes);
					end = 1;
				}
				if ((sendto(sock_udp, data, UDPbytes, 0, (struct sockaddr *) &udp_echoserver, sizeofUDP)) != UDPbytes) {
	    		Die("Mismatch in number of sent bytes (EOF)");
	   		}
	   		
				memset(&check, 0, 20);	
  			SHA1(checkBuffer, cumulate, check);
  			printf("Check = ");
  			for(k = 0; k < 20; k++){		
  				printf("%x", check[k]);
  			}
  			printf(".\n");
	   		
	   		rtn = checking(sock_tcp, check);
	   		cumulate = 0;
				//break;
			}
			else if ((sendto(sock_udp, data, UDPBUFFSIZE, 0, (struct sockaddr *) &udp_echoserver, sizeofUDP)) != UDPBUFFSIZE) {
	    	Die("Mismatch in number of sent bytes");
	   	}
			
			printf("%d\n", counter);
		}

		
	}
	printf(" count %d, rtn %d\n", counter, rtn);
	bytes = 0;
	
  close(sock_udp);
  close(sock_tcp);
  close(fh);
  exit(0);
}

int checking(int TCPSock, unsigned char * buffer){
	int j, flag;
	int bytes = 0;
	char receive[TCPBUFFSIZE];
	char command[TCPBUFFSIZE];
	char tmp[20];
	//we have already calculated the SHA1 in buffer
	//so we listen for a response:
	memset(&receive, 0, 35);
	while(bytes == 0){
  	bytes = recv(TCPSock, receive, TCPBUFFSIZE-1, 0);
  	if (bytes > 0){
  		receive[bytes] = '\0';    /* Assure null terminated string */
  		fprintf(stdout,"We saw % d bytes saying %s \n",bytes,  receive);
  		printf("which is a SHA1 of ");
  		for(j = 4; j < 24; j++){		
  				printf("%x", (unsigned char)receive[j]);
  				tmp[j-4] = receive[j];
  			}
  			printf("\n");
  			//so send back a reply
  			
  		if((strncmp(buffer,tmp, 20)) == 0){
  			memset(&command, 0, TCPBUFFSIZE);
				strcpy(command, YES);
				command[3], '\0';
				printf("command is %s", command);
				flag = 1;
			}
			else {
				memset(&command, 0, TCPBUFFSIZE);
				strcpy(command, NO);
				command[3], '\0';
				printf("command is %s", command);
				flag = 0;
			}
				
			if (send(TCPSock, command, sizeof(buffer), MSG_MORE) < 1) {
  				Die("Failed sending TCP check - wrong no sent bytes");
			}

			return flag;
		}
  }
  return -1;
	
}

/*int checking(int TCPSock, unsigned char * buffer, int length){
	unsigned char check[20];
	char command[TCPBUFFSIZE];
	char receive[TCPBUFFSIZE];
	int j, flag;
	int bytes = 0;
	memset(&check, 0, 20);	
  SHA1(buffer, length, check);
  printf("Check = ");
  for(j = 0; j < 20; j++){		
  		printf("%c", check[j]);
  }
  printf(".\n");
  while(bytes == 0){
  	bytes = recv(TCPSock, receive, TCPBUFFSIZE-1, 0);
  	if (bytes > 0){
  		receive[bytes] = '\0';    /* Assure null terminated string 
  		fprintf(stdout,"We saw % d bytes saying %s \n",bytes,  receive);
  			//so send back a reply
  		if((strncmp(check,receive,20)) == 0){
  			memset(&command, 0, TCPBUFFSIZE);
				strcpy(command, YES);
				command[3], '\0';
				printf("command is %s", command);
				flag = 1;
			}
			else {
				memset(&command, 0, TCPBUFFSIZE);
				strcpy(command, NO);
				command[3], '\0';
				printf("command is %s", command);
				flag = 0;
			}
				
			if (send(TCPSock, command, sizeof(buffer), MSG_MORE) < 1) {
  				Die("Failed sending TCP check - wrong no sent bytes");
			}

			return flag;
		}
  }
  return -1;
}*/
