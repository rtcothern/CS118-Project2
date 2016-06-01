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
#define REC_WINDOW 30720

int main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	int sockfd, i;
  socklen_t slen=sizeof(remaddr);
	char buf[BUFLEN];	/* message buffer */
	int recvlen;		/* # bytes in acknowledgement message */
	char rec_buf[REC_WINDOW];
	int current_ws = REC_WINDOW;

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

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(0);

	if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}

	memset((char *) &remaddr, 0, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(port);
	if (inet_aton(hostIP, &remaddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	//Let's do the first handshake messages
	TCPHeader synHeader(0, 0, current_ws, false, true, false);
	cout << "Sending SYN..." << endl;
	if (sendto(sockfd, synHeader.encode(), synHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen)==-1) {
		perror("sendto");
		exit(1);
	}
	recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
	if (recvlen >= 0) {
		TCPHeader synAckHeader = TCPHeader::decode(buf, recvlen);
		if(synAckHeader.S && synAckHeader.A){
			cout << "Received SYN-ACK" << endl;
			TCPHeader ackHeader(0, synAckHeader.SeqNum+1, current_ws, true, false, false);
			if (sendto(sockfd, ackHeader.encode(), ackHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen)==-1) {
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
	vector<char> testVec;
	while(true){
		/* now receive an acknowledgement from the server */
		recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
		if (recvlen >= 0) {
				TCPHeader receiveheader = TCPHeader::decode(buf, recvlen);
				//cout << "recvlen: " << recvlen << endl;
				testVec.insert(testVec.end(), receiveheader.getPayload(),receiveheader.getPayload()+recvlen-8);

				//cout << string(total_payload) << endl;
				if(!receiveheader.F){
					cout << "Receiving data packet " << receiveheader.SeqNum << endl;
					TCPHeader responseHeader(0, receiveheader.SeqNum+1024+1, current_ws, 1, 0, 0);
					if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen)==-1) {
						perror("sendto");
						exit(1);
					}
					cout << "Sending ACK packet " << responseHeader.AckNum << endl;
				} else if(receiveheader.F && !receiveheader.A && !receiveheader.S) {
					cout << "Recieved FIN packet, sending FIN-ACK..." << endl;
					TCPHeader responseHeader(0, 0, current_ws, 1, 0, 1);
					if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen)==-1) {
						perror("sendto");
						exit(1);
					}

					// save to current directory
			    std::ofstream os("test.jpg");
			    if (!os) {
			      std::cerr<<"Error writing to ..."<<std::endl;
			    }
			    else {
			      //ByteBlob data = response.getData();
			      for(vector<char>::iterator x=testVec.begin(); x<testVec.end(); x++){
			        os << *x;
			      }
						//os << "\n";
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
