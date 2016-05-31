#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "port.h"
#include "showip.h"
#include <unistd.h>
#include <sys/types.h>
#include "tcp.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iterator>
#include <vector>

#define BUFLEN 1032//1032 is the maximum packet size
#define MSGS 5	/* number of messages to send */

int main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	int sockfd, i;
  socklen_t slen=sizeof(remaddr);
	char buf[BUFLEN];	/* message buffer */
	int recvlen;		/* # bytes in acknowledgement message */

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	/* create a socket */
	if (sockfd < 0){
    std::cout << "cannot create socket" << std::endl;
  }

  if(argc < 3){
    std::cerr << "invalid number of command line arguments" << std::endl;
    return 0;
  }

  //Get Command Line Arguments
  char* hostN = argv[1];
  string portN = argv[2];
  const char* hostIP = getIP(hostN).c_str();
  int port = std::stoi(portN);

	/* bind it to all local addresses and pick any port number */

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(0);

	if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}

	/* now define remaddr, the address to whom we want to send messages */
	/* For convenience, the host address is expressed as a numeric IP address */
	/* that we will convert to a binary format via inet_aton */

	memset((char *) &remaddr, 0, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(port);
	if (inet_aton(hostIP, &remaddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	/* Time for Messages */
	int cwnd_size = 1024;
	//Let's do the first handshake messages
	TCPHeader synHeader(0, 0, cwnd_size, false, true, false);
	cout << "Sending SYN..." << endl;
	if (sendto(sockfd, synHeader.encode(), BUFLEN, 0, (struct sockaddr *)&remaddr, slen)==-1) {
		perror("sendto");
		exit(1);
	}
	recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
	if (recvlen >= 0) {
		TCPHeader synAckHeader = TCPHeader::decode(buf);
		if(synAckHeader.S && synAckHeader.A){
			cout << "Received SYN-ACK" << endl;
			TCPHeader ackHeader(0, synAckHeader.SeqNum+1, cwnd_size, true, false, false);
			if (sendto(sockfd, ackHeader.encode(), BUFLEN, 0, (struct sockaddr *)&remaddr, slen)==-1) {
				perror("sendto");
				exit(1);
			}
			cout << "Sending ACK packet " << ackHeader.AckNum << endl;
		} else {
			perror("syn-ack corrupted");
			exit(1);
		}
	}else{
		perror("error receiving syn-ack");
		exit(1);
	}

	string total_payload = "";

	while(true){
		//printf("Sending packet %d to %s port %d\n", i, hostIP, port);
		// TCPHeader header(0, 0, cwnd_size, true, false, false);
		// sprintf(buf, "This is packet %d", i);
		/* now receive an acknowledgement from the server */
		recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
		if (recvlen >= 0) {
				TCPHeader receiveheader = TCPHeader::decode(buf);
				// cout << "Header Flags - A: " << receiveheader.A
				// 	<< ", S: " << receiveheader.S << ", F: " << receiveheader.F << endl;

				if(receiveheader.getPayload() != NULL){
					total_payload.append(receiveheader.getPayload());
				}

				//cout << string(total_payload) << endl;
				if(!receiveheader.F){
					//cout << "Receiving data packet " << receiveheader.SeqNum << endl;
					TCPHeader responseHeader(0, receiveheader.SeqNum+1024+1, cwnd_size, 1, 0, 0);
					if (sendto(sockfd, responseHeader.encode(), BUFLEN, 0, (struct sockaddr *)&remaddr, slen)==-1) {
						perror("sendto");
						exit(1);
					}
					cout << "Sending ACK packet " << responseHeader.AckNum << endl;
				} else if(receiveheader.F && !receiveheader.A && !receiveheader.S) {
					cout << "Recieved FIN packet, sending FIN-ACK..." << endl;
					TCPHeader responseHeader(0, 0, cwnd_size, 1, 0, 1);
					if (sendto(sockfd, responseHeader.encode(), BUFLEN, 0, (struct sockaddr *)&remaddr, slen)==-1) {
						perror("sendto");
						exit(1);
					}

					// save to current directory
			    std::ofstream os("test.txt");
			    if (!os) {
			      std::cerr<<"Error writing to ..."<<std::endl;
			    }
			    else {
			      string data = total_payload.c_str();
						os << data;
						os.close();
			    }


					close(sockfd);
					return 0;
				}
		}

	}
	// close(sockfd);
	// return 0;
}
