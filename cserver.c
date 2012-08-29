/*******************************************
*
* receives data via TCP, sends UDP reply

Uses TCP as control channel
Receives data chunk. ACK with count and cksum

If ACK is wrong client will resend the chunk

*
********************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <openssl/sha.h>

#define MAXPENDING 5    /* Max connection requests */
#define BUFFSIZE 1400
#define CMDSIZE 60

void Die(char *mess) { perror(mess); exit(1); }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* 
* HandleClient is passed a TCP socket (for control) and a UDP
* port number (for creating the data channel). It listens on 
* the TCP socket. Once it receives an open command , it creates
* the UDP socket and sends back details of this to the client.
* 
* The client then sends the first N chunks of data. Once these
* are received it pauses, claculates the SHA1 of these chunks,
* sends this back to the client and waits for an ACK. This will
* either tell it to proceed, in which case it writes the data
* to file and goes back to listen for the next chunks, or it
* deletes what it has received and gets resent it.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void HandleClient(int rcvsock, int Uport) {
  char buffer[BUFFSIZE];  // not sure I need so many buffers
  char data[BUFFSIZE]; // a buffer for the immediate 
  unsigned char check[20];	// a buffer for the SHA1 checksum
  char command[CMDSIZE]; // a buffer for the TCP commands
  char ackCount[CMDSIZE];
  char sum, temp, tmp2;
  int ackPer = 0; //how often will we be sending acks? received in "open NN" command 
  int udp_clientsock, serverlen, bytes, j, kill, pointer, byteCount, pause;
  struct sockaddr_in udp_echoserver;
  int received = -1;
  int sent = -1;
  int totalBytes = 0;

/* Open a file to write the output. For now give it fixed name */
  int32_t fh = 0;
	const char* filename = "out.txt";
  if((fh = open(filename, O_WRONLY|O_TRUNC)) == -1){
    Die("Couldn't open the file");
  }

/* Receive initial message into buffer */
  if ((received = recv(rcvsock, buffer, CMDSIZE, 0)) < 0) {
    Die("Failed to receive initial bytes from client");
  }
 
/* use command as temporary store for "open" */
  fprintf(stderr, "TCP received %s \n", buffer);
  memset(&command, 0, sizeof(command));
	strcpy(command, "open");

/* check that the message we got said "open" 
* Longer term can I make this a standalone function?
*/

  if(!strncmp(buffer, command, 4))
  	printf("we got an open \n");
  for (j = 5; j < received; j++){
  	ackCount[j - 5] = buffer[j];
  }

/* included in the open message is the frequency at which 
* client expects to see the ACKs.
*/
  ackCount[received - 5] = '\0';
  ackPer = atoi(ackCount);
  printf("ACK every %s which is an int: %d\n", ackCount, ackPer);
  
/* Now we can create an appropriate size buffer to hold the
* incoming messages. Make this a function?
*/
  char tmp[ackPer * BUFFSIZE];
  memset(&tmp, 0, sizeof(tmp));       /* Clear memory */
  
/* crerate the new UDP socket */ 
	if ((udp_clientsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		Die("Failed to create UDP socket");
	}
/* Construct the UDP server sockaddr_in structure */
	memset(&udp_echoserver, 0, sizeof(udp_echoserver));       /* Clear struct */
	udp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
	udp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Any IP address */
	udp_echoserver.sin_port = htons(Uport);  	/*use port from command line*/

/* Bind to the UDP socket*/
  serverlen = sizeof(udp_echoserver);
  if (bind(udp_clientsock, (struct sockaddr *) &udp_echoserver, serverlen) < 0) {
  	Die("Failed to bind UDP server socket");
  } 
  
	if((connect(udp_clientsock, (struct sockaddr *) &udp_echoserver, serverlen) ) < 0) {
		Die("Failed to connect UDP server socket");
  } 
/* Send back details of UDP port that will be used*/
	sprintf(command, "U %d", Uport);  //longer term we should also echo ackPer
  command[6]  = '\0';
  printf("Sending back %s \n", command);
  if ((sent = send(rcvsock, command, 7, MSG_MORE)) != 7) {
    Die("Failed to send UDP port command ");
  }

/* Create a loop that only exits once told to by the client */
	while(kill != 1){
	
/* set the pointer and byteCount to 0*/
  	pointer = 0;
  	byteCount = 0;
  	totalBytes = 0;
		
/* create a loop for receiving the data. NB this is the awkward
* bit as we need to be able to return to this loop repeatedly.
* Perhaps the solution is to have this as a permanent loop but
* every ackPer segments we call another function to do the checking?
*/
 	 	while (1) {
  		
/* Receive a message from the client */
  		if ((bytes = recv(udp_clientsock, data, BUFFSIZE, 0)) < 0) {
	 			Die("Failed to receive message");
  		}
  		byteCount += bytes;  //keeps a running copunt of number of bytes
  		totalBytes += bytes;
/* use memcpy to copy to the tmp storage */
 			memcpy(&tmp[byteCount], data, bytes);
 			
/* Increment the pointer. Check to see if we have had ackPer
* segments and pause if we have. Then calculate the SHA1 and
* send this by TCP. Client will respond yes or no 
*/		
 			pointer++;
 			if(pointer%ackPer == 0) {
 				printf("Time to calculate checksum\n");
 				while (1){
 					memset(&check, 0, 20);
 					sent = 0;	
  				SHA1(tmp, byteCount, check);
  				printf("Check = ");
  				for(j = 0; j < 20; j++){
  					
  					printf("%c", check[j]);
  				}
  				printf(".\n");
 					strcpy(command, "ACK ");  //build the ack message
    			memcpy(&command[4], check, 20);
    			char number[9];
    			sprintf(number, " %d", totalBytes);
    			fprintf(stderr, "totalBytes is %d\n", totalBytes);
    			strcpy(&command[24], number);
    			
					fprintf(stderr, "Sending %s.\n", command);
					if ((sent = send(rcvsock, command, 30, MSG_MORE)) != 30) {
      			Die("Failed to send TCP ack in reply");
    			}
    			/* Receive  message into buffer */
  				if ((received = recv(rcvsock, buffer, CMDSIZE, 0)) < 0) {
   					 Die("Failed to receive return from client");
  					}
 
					/* use command as temporary store for "open" */
  				fprintf(stderr, "TCP received %s \n", buffer);
  				memset(&command, 0, sizeof(command));
					strcpy(command, "yes");

				/* check that the message we got said "open" 
				* Longer term can I make this a standalone function?
				*/
  				if(!strncmp(buffer, command, 4)){
  					printf("we got an open (and received is %d)\n", received);
  					break;
 					}
 				}
 			}
		}
  }
  
}



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* 
* main function opens a TCP socket, listens on that socket
* and spawns a new socket when client tries to connect
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int main(int argc, char *argv[]) {

  int tcp_serversock, tcp_clientsock;
  struct sockaddr_in tcp_echoserver, tcp_echoclient;

/* Check for the correct useage */
  if (argc != 3) {
    fprintf(stderr, "USAGE: echoserver <TCP port> <UDP port>\n");
    exit(1);
  }
  
  int UDPport = atoi(argv[2]);
  
/* Create the TCP socket */
  if ((tcp_serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    Die("Failed to create socket");
  }
  
/* Construct the TCP server sockaddr_in structure */
  memset(&tcp_echoserver, 0, sizeof(tcp_echoserver));      /* Clear struct */
  tcp_echoserver.sin_family = AF_INET;                     /* Internet/IP */
  tcp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Incoming addr */
  tcp_echoserver.sin_port = htons(atoi(argv[1]));          /* server port */

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
    
 /* Call the handler function (this does the heavy lifting) */
    HandleClient(tcp_clientsock, UDPport);
  }
}



