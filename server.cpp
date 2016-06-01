#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include "port.h"
#include "udp.h"
#include "tcp.h"

#include <fstream>
#include <sstream>

#define BUFSIZE 1032

typedef std::string string;

typedef std::vector<char> ByteBlob;

int
main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	socklen_t addrlen = sizeof(remaddr);
	int recvlen;
	int sockfd;
	int msgcnt = 0;
	char buf[BUFSIZE];

	//UdpHeader udp;
	//std::cout << std::to_string(sizeof(udp)) << std::endl;


	/* create a UDP socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		std::cerr << "cannot create socket" << std::endl;
		return 0;
	}

  if(argc < 3){
    std::cerr << "invalid number of command line arguments" << std::endl;
    return 0;
  }

  //Get Command Line Arguments
  string portN = argv[1];
  string file_dir = argv[2];
  int port = std::stoi(portN);

	/* bind the socket to any valid IP address and a specific port */
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}

  //Grab the file specified
  std::ifstream in(file_dir.c_str());
	ByteBlob contents((std::istreambuf_iterator<char>(in)),
	    std::istreambuf_iterator<char>());
	char * contentArr = &contents[0];
	int contentIndex = 0;
	int numToCopy = 1024;
	int contentSize = contents.size();

	if(contents.empty()){
		std::cerr << "Invalid File" << std::endl;
	}

	const int max_seq_num = 30720;
	int seq_num;
	int ack_num;

	while (true) {
    // std::cout << "waiting on port " << port << std::endl;
		recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		if (recvlen > 0) {
			// buf[recvlen] = 0;
      // std::cout << "Received message: " << buf << std::to_string(recvlen) << std::endl;
			TCPHeader received = TCPHeader::decode(buf, recvlen);

			//Initial connection request, recieved SYN, we're sending SYN-ACK
			if(received.S){
				cout << "Received SYN packet, sending SYN-ACK..." << endl;
				contentIndex = 0;
				seq_num = rand() % max_seq_num; //Set a random initial sequence number
				TCPHeader synAckHeader(seq_num, 0, received.Window, true, true, false);
				if (sendto(sockfd, synAckHeader.encode(), synAckHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen)==-1) {
					perror("sendto");
					exit(1);
				}
			}
			//Last phase of 3-way HS, or normal data flow
			else if(received.A && !received.F){
				cout << "Receiving ACK packet " << received.AckNum << endl;
				TCPHeader response(received.AckNum, 0, received.Window, false, false, 0);
				char* currPay;
				if(numToCopy + contentIndex < contentSize){
					currPay = new char[numToCopy];
					cout << "Sending values - contentIndex: " << contentIndex << ", contentSize: "
						<< contentSize << ", numToCopy: " << numToCopy << endl;
					memcpy(currPay, contentArr+contentIndex, numToCopy);
					contentIndex += numToCopy;
					cout << "Sending data packet " << response.SeqNum << endl;
					response.setPayload(currPay, numToCopy);
				}
				else {
					currPay = new char[contentSize - contentIndex];
					memcpy(currPay, contentArr+contentIndex, contentSize - contentIndex);
					response.F = 1;
					cout << "Sending data packet " << response.SeqNum << ", this is FIN" << endl;
					response.setPayload(currPay, contentSize - contentIndex);
				}



				//free(currPay);
				if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen)==-1) {
					perror("sendto");
					exit(1);
				}


			}
			//We've sent a FIN to the client and now it's FIN-ACK-ing us
			else if(received.A && received.F){
				// close(sockfd);
				cout << "Received FIN-ACK, we're done" << endl;
				return 0;
			}
		}
		else{
      std::cout << "Ah shit... something went wrong..." << std::endl;
    }
	}
}
