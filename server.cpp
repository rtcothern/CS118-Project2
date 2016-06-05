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
#include <sys/time.h>

#define BUFSIZE 1032
#define MAX_SEQ_NUM 30720

typedef std::string string;

typedef std::vector<char> ByteBlob;

int getCurrentTimestamp() {
	timeval timestamp;
	gettimeofday(&timestamp, NULL);
	return timestamp.tv_sec*1000 + timestamp.tv_usec / 1000;
}
int
main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	socklen_t addrlen = sizeof(remaddr);
	int recvlen;
	int sockfd;
	char buf[BUFSIZE];
	int rec_window;

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

	//Set the timeout to 500ms on the socket
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
	    perror("Error");
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

	//Seq numbers to timestamps
	//std::map<int, int> unackedPacketTimstamps;

	int seq_num = -1;
	int ISN = -1;
	int cwnd = 1024;
	int ssthresh = 30720;
	int dupCount = 0;
	int numTimesWrapped = 0;
	bool begunTransfer = false;

	int firstTime = getCurrentTimestamp();
	//unackedPacketTimstamps[seq_num] = timestamp.tv_usec / 1000;
	srand(firstTime);
	seq_num = rand() % MAX_SEQ_NUM; //Set a random initial sequence number
	ISN = seq_num;
	contentIndex = 0;
	int oldestTimestamp = firstTime;

	cout << "Initial Seq Num: " << seq_num << endl;

	vector<int> unackedSeqNums;
	unackedSeqNums.push_back(ISN);

	while (true) {
		recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		if (recvlen > 0) {
			TCPHeader received = TCPHeader::decode(buf, recvlen);

			//Initial connection request, recieved SYN, we're sending SYN-ACK
			if(received.S){
				begunTransfer = false;
				rec_window = received.Window;
				cout << "Received SYN packet, sending SYN-ACK..." << endl;
				TCPHeader synAckHeader(unackedSeqNums[0], 0, received.Window, true, true, false);
				oldestTimestamp = getCurrentTimestamp();

				if (sendto(sockfd, synAckHeader.encode(), synAckHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
					perror("sendto");
					exit(1);
				}
			}
			//Last phase of 3-way HS, or normal data flow
			else if(received.A && !received.F){
				begunTransfer = true;
				cout << "Receiving ACK packet " << received.AckNum << endl;
				
				/*if (received.AckNum == seq_num + 1024 
					|| received.AckNum == seq_num + 1
					|| received.AckNum == 1024 - (MAX_SEQ_NUM - seq_num)) {*/
				if(!unackedSeqNums.empty() && received.AckNum == unackedSeqNums[0]){
					/*if (received.AckNum == 1024 - (MAX_SEQ_NUM - seq_num)) {
						numTimesWrapped++;
					}*/
					oldestTimestamp = getCurrentTimestamp();
					//seq_num = received.AckNum;
					int numNewPackets = 0;
					if (cwnd <= ssthresh) {
						if (cwnd + 2048 <= received.Window)
							numNewPackets = 2;
						else if (cwnd + 1024 <= received.Window)
							numNewPackets = 1;
					}
					else {
						//TODO Impl Congestion Avoidance
					}
					vector<TCPHeader> toSendOut;
					for (int i = 0; i < numNewPackets; i++) {
						char* currPay;
						int lastSeqNum = unackedSeqNums[(cwnd + i*1024) / 1024 - 1];
						if (lastSeqNum == 0) {
							cout << "IT WAS ZERO!" << endl;
							cout << "cwnd: " << cwnd;
							for (auto h : unackedSeqNums) {
								cout << "ELEMENT: " << h << ", ";
							}
							cout << endl;
						}
						int contentIndex = lastSeqNum - ISN + numTimesWrapped*MAX_SEQ_NUM;
						TCPHeader response(lastSeqNum, 0, received.Window, 0, 0, 0);
						if (1024 + contentIndex < contentSize) {
							currPay = new char[1024];
							memcpy(currPay, contentArr + contentIndex, 1024);
							cout << "Sending data packet " << response.SeqNum << " Content Index is: " << contentIndex << endl;
							response.setPayload(currPay, 1024);
							unackedSeqNums.push_back(lastSeqNum + 1024);
						}
						else {
							currPay = new char[contentSize - contentIndex];
							memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
							response.F = 1;
							cout << "Sending data packet " << response.SeqNum << ", this is FIN"
								<< " Content Index is: " << contentIndex << " Content Size is: " << contentSize - contentIndex << endl;
							response.setPayload(currPay, contentSize - contentIndex);
							unackedSeqNums.push_back(lastSeqNum + (contentSize - contentIndex));
						}
						toSendOut.push_back(response);
					}
					for (TCPHeader h : toSendOut) {
						if (sendto(sockfd, h.encode(), h.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
							perror("sendto");
							exit(1);
						}
					}
					cout << "Removing head of unacked: " << unackedSeqNums[0] << endl;
					if (numNewPackets >= 1) {
						cwnd += 1024;
						cout << "Increasing cwnd by 1, cwnd is now: " << cwnd << endl << endl;
					}
					unackedSeqNums.erase(unackedSeqNums.begin());
				}
				else {
					//cout << "getCurrentTimestamp(): " << getCurrentTimestamp() << " - oldestTimestamp: " << oldestTimestamp << " = " << getCurrentTimestamp() - oldestTimestamp << endl;
					if (getCurrentTimestamp() - oldestTimestamp > 500) {
						int headSeqNum = unackedSeqNums[0];
						int contentIndex = unackedSeqNums[0] - ISN + numTimesWrapped*MAX_SEQ_NUM;
						TCPHeader response(headSeqNum, 0, received.Window, 0, 0, 0);
						char* currPay;
						if (numToCopy + contentIndex < contentSize) {
							currPay = new char[numToCopy];
							memcpy(currPay, contentArr + contentIndex, numToCopy);
							cout << "Sending data packet " << response.SeqNum << " Restransmission (Specific Timeout)" << " Content Index is: " << contentIndex << endl;
							response.setPayload(currPay, numToCopy);
						}
						else {
							currPay = new char[contentSize - contentIndex];
							memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
							response.F = 1;
							cout << "Sending data packet " << response.SeqNum << ", this is FIN" << " Content Index is: " << contentIndex << endl;
							response.setPayload(currPay, contentSize - contentIndex);
						}
						if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
							perror("sendto");
							exit(1);
						}
						/*unackedSeqNums.clear();
						unackedSeqNums.push_back(headSeqNum);
						ssthresh = cwnd / 2;
						cwnd = 1024;*/
					}
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
			if (begunTransfer) {
				int headSeqNum = unackedSeqNums[0];
				int contentIndex = unackedSeqNums[0] - ISN + numTimesWrapped*MAX_SEQ_NUM;
				TCPHeader response(headSeqNum, 0, rec_window, 0, 0, 0);
				char* currPay;
				if (numToCopy + contentIndex < contentSize) {
					currPay = new char[numToCopy];
					memcpy(currPay, contentArr + contentIndex, numToCopy);
					cout << "Sending data packet " << response.SeqNum << " Restransmission (General Timeout)" << " Content Index is: " << contentIndex << endl;
					response.setPayload(currPay, numToCopy);
				}
				else {
					currPay = new char[contentSize - contentIndex];
					memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
					response.F = 1;
					cout << "Sending data packet " << response.SeqNum << ", this is FIN" << " Content Index is: " << contentIndex << endl;
					response.setPayload(currPay, contentSize - contentIndex);
				}
				if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
					perror("sendto");
					exit(1);
				}
				/*unackedSeqNums.clear();
				unackedSeqNums.push_back(headSeqNum);
				ssthresh = cwnd / 2;
				cwnd = 1024;*/
			}

		}
	}
}
