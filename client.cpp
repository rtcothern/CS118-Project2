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
#include <map>

#define BUFLEN 1032//1032 is the maximum packet size
#define REC_WINDOW 30720
#define MAX_SEQ_NUM 30720

int main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	int sockfd;
  socklen_t slen=sizeof(remaddr);
	char buf[BUFLEN];	/* message buffer */
	int recvlen;		/* # bytes in acknowledgement message */
	//char rec_buf[REC_WINDOW];
	int current_ws = REC_WINDOW;
	int expected_seq;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	/* create a socket */
	if (sockfd < 0){
    std::cout << "cannot create socket" << std::endl;
  }

  if(argc < 3){
    std::cerr << "invalid number of command line arguments" << std::endl;
    return 0;
  }

  //Set the timeout to 500ms on the socket
  struct timeval tv;
  tv.tv_sec = 0;
  //MADE THIS LONGER
  tv.tv_usec = 500000;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
	  perror("Error");
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


	int numTimesWrapped = 0;

	//While loop for handshake
	while (true) {
		//Let's do the first handshake messages
		TCPHeader synHeader(0, 0, current_ws, false, true, false);
		cout << "Sending packet SYN" << endl;
		if (sendto(sockfd, synHeader.encode(), synHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen) == -1) {
			perror("sendto");
			exit(1);
		}
		recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
		if (recvlen >= 0) {
			TCPHeader synAckHeader = TCPHeader::decode(buf, recvlen);
			if (synAckHeader.S && synAckHeader.A) {
				cout << "Receiving packet SYN" << endl;
				expected_seq = synAckHeader.SeqNum;// +1;
				//cout << "Got this initial sequence number: " << synAckHeader.SeqNum << endl;
				/*if (expected_seq > MAX_SEQ_NUM) {
					expected_seq -= MAX_SEQ_NUM;
					numTimesWrapped++;
				}*/
				TCPHeader ackHeader(0, expected_seq, current_ws, true, false, false);
				if (sendto(sockfd, ackHeader.encode(), ackHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen) == -1) {
					perror("sendto");
					exit(1);
				}
				cout << "Sending packet " << ackHeader.AckNum << endl;
				break;
			}
			else {
				perror("syn-ack corrupted");
				exit(1);
			}
		}
		/*else {
			TCPHeader synHeader(0, 0, current_ws, false, true, false);
			if (sendto(sockfd, synHeader.encode(), synHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen) == -1) {
				perror("sendto");
				exit(1);
			}
			cout << "Sending SYN... Retransmission" << endl;
		}*/
	}
	//string total_payload = "";
	vector<char> testVec;
	std::map<int, char*> contentMap;
	std::vector<char*> contentVector;
	while(true){
		/* now receive an acknowledgement from the server */
		recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
		if (recvlen >= 0) {
				TCPHeader receiveheader = TCPHeader::decode(buf, recvlen);

				if(!receiveheader.F){
					if(receiveheader.SeqNum == expected_seq){
						cout << "Receiving packet " << receiveheader.SeqNum << endl;
						TCPHeader responseHeader(0, expected_seq, current_ws, 1, 0, 0);
						expected_seq = receiveheader.SeqNum + (recvlen-8);
						//contentMap[receiveheader.SeqNum+ numTimesWrapped*MAX_SEQ_NUM] = new char[1024];
						contentVector.push_back(receiveheader.getPayload());
						//memcpy(contentMap[receiveheader.SeqNum + numTimesWrapped*MAX_SEQ_NUM], receiveheader.getPayload(), 1024);
						if (expected_seq > MAX_SEQ_NUM) {
							expected_seq -= MAX_SEQ_NUM;
						}
						if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen) == -1) {
							perror("sendto");
							exit(1);
						}
						cout << "Sending packet " << responseHeader.AckNum << endl;

					}
					/*else if (contentMap.find(receiveheader.SeqNum) != contentMap.end()) {
						cout << "Receiving data packet " << receiveheader.SeqNum << endl;
						TCPHeader responseHeader(0, receiveheader.SeqNum, current_ws, 1, 0, 0);
						if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen) == -1) {
							perror("sendto");
							exit(1);
						}
						cout << "Sending ACK packet " << responseHeader.AckNum << endl;
					}*/
					else {
						cout << "Receiving packet " << receiveheader.SeqNum << endl;// << ", out of order?" << endl;
						TCPHeader responseHeader(0, expected_seq, current_ws, 1, 0, 0);
						/*contentMap[receiveheader.SeqNum + numTimesWrapped*MAX_SEQ_NUM] = new char[1024];
						memcpy(contentMap[receiveheader.SeqNum + numTimesWrapped*MAX_SEQ_NUM], receiveheader.getPayload(), 1024);*/
						if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen) == -1) {
							perror("sendto");
							exit(1);
						}
						cout << "Sending packet " << responseHeader.AckNum << endl;
					}
					
				} else if(receiveheader.F && !receiveheader.A && !receiveheader.S ) {
						if(receiveheader.SeqNum == expected_seq){
							cout << "Receiving packet " << receiveheader.SeqNum << endl;
							//testVec.insert(testVec.end(), receiveheader.getPayload(),receiveheader.getPayload()+recvlen-8);
							//contentMap[receiveheader.SeqNum] = new char[recvlen - 8];
							//memcpy(contentMap[receiveheader.SeqNum], receiveheader.getPayload(), recvlen - 8);
							//contentMap[receiveheader.SeqNum] = receiveheader.getPayload();
							char* lastChunk = new char[recvlen - 8];
							memcpy(lastChunk, receiveheader.getPayload(), recvlen - 8);
							//cout << "Recvlen - 8: " << recvlen - 8 << endl;
							//cout << receiveheader.getPayload() << endl;
							/*cout << "Payload After: " << contentMap[receiveheader.SeqNum] << endl;
							cout << "Payload After Iterative: ";
							for (int i = 0; i < recvlen-8; i++)
								cout << contentMap[receiveheader.SeqNum][i];
							cout << endl;*/
							cout << "Sending packet FIN" << endl;
							TCPHeader responseHeader(0, 0, current_ws, 1, 0, 1);
							if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen)==-1) {
								perror("sendto");
								exit(1);
							}
							// save to current directory
							std::ofstream os("received.data");
							if (!os) {
								std::cerr<<"Error writing to ..."<<std::endl;
							}
							else {
								//for(std::map<int, char*>::iterator x=contentMap.begin(); x!=contentMap.end(); ++x){

								//	for (int i = 0; i < 1024; i++) {
								//		//cout << x->second[i];
								//		os << x->second[i];
								//	}
								//	//cout << endl;

								//}
								for (char* c : contentVector) {
									for (int i = 0; i < 1024; i++) {
										os << c[i];
									}
								}
								//cout << "Made it here" << endl;
								//os << foo;
								//cout << "Payload For fooasdf: " << endl;
								for (int i = 0; i < recvlen - 8; i++) {
									//cout << lastChunk[i];
									os << lastChunk[i];
								}
								//cout << endl;
								os.close();
							}
							
							close(sockfd);
							return 0;
						}else{
							//contentMap[receiveheader.SeqNum] = receiveheader.getPayload();
							TCPHeader responseHeader(0, expected_seq, current_ws, 1, 0, 0);
							if (sendto(sockfd, responseHeader.encode(), responseHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, slen)==-1) {
								perror("sendto");
								exit(1);
							}
						}
				}
		}
		else {
			TCPHeader responseHeaderRetransmit(0, expected_seq, current_ws, 1, 0, 0);
			if (sendto(sockfd, responseHeaderRetransmit.encode(), 
				responseHeaderRetransmit.getPacketSize(), 0, 
				(struct sockaddr *)&remaddr, slen) == -1) {

				perror("sendto");
				exit(1);
			}
			cout << "Sending packet " << responseHeaderRetransmit.AckNum << " Retransmission" << endl;
		}

	}
	// close(sockfd);
	// return 0;
}
