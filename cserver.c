
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
#include <fcntl.h>

#include <openssl/sha.h>

#define MAXPENDING 5    /* Max connection requests */
#define BUFFSIZE 1400
#define CMDSIZE 20

void Die(char *mess) { perror(mess); exit(1); }

void HandleClient(int rcvsock) {
  char buffer[BUFFSIZE];
  char data[BUFFSIZE];

  unsigned char check[21];
  char command[7];
  char ackCount[CMDSIZE];
  char sum, temp, tmp2;
  int ackPer = 0; //how often will we be sending acks? received in "open NN" command 
  int udp_clientsock, serverlen, bytes, j, kill, pointer, byteCount, pause;
  struct sockaddr_in udp_echoserver;
  int received = -1;
  int sent = -1;
  int totalBytes = 0;

  int32_t fh = 0;
	const char* filename = "out.txt";
  if((fh = open(filename, O_WRONLY|O_TRUNC)) == -1){
    Die("Couldn't open the file");
  }

  /* Receive message */
  if ((received = recv(rcvsock, buffer, CMDSIZE, 0)) < 0) {
    Die("Failed to receive initial bytes from client");
  }
  fprintf(stderr, "TCP received %s \n", buffer);
  memset(&command, 0, sizeof(command));
	strcpy(command, "open");

  if(!strncmp(buffer, command, 4))
  	printf("we got an open (and received is %d)\n", received);
  for (j = 5; j < received; j++){
  	ackCount[j - 5] = buffer[j];
  }
  ackCount[received - 5] = '\0';
  ackPer = atoi(ackCount);
  char tmp[ackPer * BUFFSIZE]; // 
  
  memset(&tmp, 0, sizeof(tmp));       /* Clear memory */
  printf("ACK every %s which is an int: %d\n", ackCount, ackPer);
  sent = 1;

  while (1) {
    /* Send back command */
    strcpy(command, "U 1313");  //longer term we should echo ackPer
    command[6]  = '\0';
    printf("Sending back %s \n", command);
    if ((sent = send(rcvsock, command, 7, MSG_MORE)) != 7) {
      	Die("Failed to send UDP port command ");
    }
	printf("out of the loop\n");
	
    /* Create the UDP socket */ 
	if ((udp_clientsock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		Die("Failed to create UDP socket");
	}
/* Construct the UDP server sockaddr_in structure */
	memset(&udp_echoserver, 0, sizeof(udp_echoserver));       /* Clear struct */
	udp_echoserver.sin_family = AF_INET;                  /* Internet/IP */
	udp_echoserver.sin_addr.s_addr = inet_addr("127.0.0.1");   /* Any IP address */
	udp_echoserver.sin_port = htons(1313);  	/*hard wire the UDP to port 1313*/
  
/*
We need to listen on the UDP socket. When we get date write it to some structure.
After N bytes we send back some sort of checksum (IP cksum?)
So... have a large buffer we fill. Once full cksum it, send back result by TCP.
If we get OK back (TCP) send the buffer to a file, then send back ready to receive,
repeat...
*/
  serverlen = sizeof(udp_echoserver);
  if (bind(udp_clientsock, (struct sockaddr *) &udp_echoserver, serverlen) < 0) {
  	Die("Failed to bind UDP server socket");
  } 

	//kill = 0;

 // while(kill != 1){
  	pointer = 0;
  	byteCount = 0;

 	 	do{
  		
		/* Receive a message from the client */
  		if ((bytes = recvfrom(udp_clientsock, data, BUFFSIZE, 0, (struct sockaddr *) &udp_echoserver, &serverlen)) < 0) {
	 		Die("Failed to receive message");
  		}
  		byteCount += bytes;
 			//use memcpy to copy to the next chunk...
 			memcpy(&tmp[byteCount], data, bytes);
 			pointer++;
 			j = fcntl(rcvsock, F_GETFL);
			printf("%d\n", j);
			
			if(pointer == ackPer){
				totalBytes += byteCount;
 	 			memset(&check, 0, 21);	
  			SHA1(tmp, byteCount, check);
  			check[20] = '\0';
  			pause = 1;
  			sent = 0;
  			printf("check = %s.\n", check);
  	
  			while(pause == 1){
  				//wait to hear an OK from the sender (client)
					//Then we can write those bytes to file		
    			strcpy(command, "ACK ");  //build the ack message
    			memcpy(&command[4], check, 20);
    			char number[9];
    			sprintf(number, " %d", totalBytes);
    			strcpy(&command[24], number);
					printf("length of command is %d. sending %s.\n",sizeof(command), command);
					j = fcntl(rcvsock, F_GETFL);
					printf("Is socket open? %d\n", j);
    			if ((sent = send(rcvsock, command, 30, MSG_MORE)) != 30) {
      			Die("Failed to send TCP ack in reply");
    			}
  				//now we will wait for an ack - if OK we write the data then loop back to next receive
					write(fh, tmp, byteCount);
					pause = 0;
				}			
		}
/*		j = fcntl(rcvsock, F_GETFL);  //  WHY IS IT NOW CLOSED???????? GRRRRRRRR!
			printf("%d\n", j);
		totalBytes += byteCount;
	/*calculate the SHA1
 
 	 	memset(&check, 0, 21);	
  	SHA1(tmp, byteCount, check);
  	check[20] = '\0';
  	pause = 1;
  	sent = 0;
  	printf("check = %s.\n", check);
  	
  	while(pause == 1){
  		//wait to hear an OK from the sender (client)
			//Then we can write those bytes to file		
    	strcpy(command, "ACK ");  //build the ack message
    	memcpy(&command[4], check, 20);
    	char number[9];
    	sprintf(number, " %d", totalBytes);
    	strcpy(&command[24], number);
			printf("length of command is %d. sending %s.\n",sizeof(command), command);
			j = fcntl(rcvsock, F_GETFL);
			printf("Is socket open? %d\n", j);
    	if ((sent = send(rcvsock, command, 30, MSG_MORE)) != 30) {
      		Die("Failed to send TCP ack in reply");
    	}
  		//now we will wait for an ack - if OK we write the data then loop back to next receive
			write(fh, tmp, byteCount);
			
			kill = 1;  //temporarily here to kill server prematurely*/
			//pause = 0;
		} while (pointer < ackPer);
		kill = 1;
	
 
  close(rcvsock);
  close(udp_clientsock);
  close(fh);
  }
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


