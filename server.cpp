#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <math.h> 
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
	int cachedLastReceivedSeq = 0;

	bool waitForFINACK = false;

	int internalNumWrapped = 0;

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
					if (cachedLastReceivedSeq > received.AckNum) {
						numTimesWrapped++;
						internalNumWrapped = 0;
						cout << "New numTimesWrapped: " << numTimesWrapped << endl;
					}
					cachedLastReceivedSeq = received.AckNum;
					oldestTimestamp = getCurrentTimestamp();
					//seq_num = received.AckNum;
					bool SlowStart = false;
					if (!waitForFINACK) {
						int numNewPackets = 1;
						//if (cwnd <= ssthresh) {
						//	SlowStart = true;
						//	if (cwnd + 1024 <= ssthresh) {
						//		numNewPackets = 2;
						//		cwnd += 1024;
						//	}
						//	else {
						//		numNewPackets = 1;
						//	}
						//}
						//else {
						//	//TODO Impl Congestion
						//	int currentCWNDPackets = cwnd / 1024;
						//	int numToIncreaseCWND = 1.0 / currentCWNDPackets * 1024;
						//	if (cwnd + numToIncreaseCWND <= received.Window) {
						//		int temp = (cwnd + numToIncreaseCWND) / 1024;
						//		if (temp != currentCWNDPackets) {
						//			numNewPackets = 2;
						//		}
						//		else {
						//			numNewPackets = 1;
						//		}
						//		cwnd += numToIncreaseCWND;
						//	}
						//}

						/*bool SlowStart = true;
						if (cwnd <= ssthresh) {
							numNewPackets = 2;
							cwnd += 1024;
						}
						else {
							SlowStart = false;
							int currentCWNDPackets = cwnd / 1024;
							int numToIncreaseCWND = 1.0 / currentCWNDPackets * 1024;
							if (cwnd + numToIncreaseCWND <= received.Window) {
								if ((cwnd + numToIncreaseCWND) / 1024 != cwnd) {
									numNewPackets = 2;
								}
								cwnd += numToIncreaseCWND;
							}
						}*/

						if (cwnd <= ssthresh) {
							numNewPackets = 2;
							cwnd += 1024;

						}
						else {
							/*int currentCWNDPackets = cwnd / 1024;
							int numToIncreaseCWND = ceil(1.0 / currentCWNDPackets * 1024);
							cout << "currentCWNDPackets: " << currentCWNDPackets << ", numToIncreaseCWND: " << numToIncreaseCWND << ", (cwnd + numToIncreaseCWND) / 1024: " << (cwnd + numToIncreaseCWND) / 1024 << ", cwnd: " << cwnd << endl;
							if (cwnd + numToIncreaseCWND <= received.Window) {
								if ((cwnd + numToIncreaseCWND) / 1024 != cwnd) {
									numNewPackets = 2;
								}
								cwnd += numToIncreaseCWND;
							}*/
							if (cwnd + 1024 <= received.Window) {
								numNewPackets = 2;
								cwnd += 1024;
							}
							
						}
						

						vector<TCPHeader> toSendOut;
						for (int i = 0; i < numNewPackets; i++) {
							char* currPay;
							int lastSeqNum = unackedSeqNums.back();// [(cwnd + i * 1024) / 1024 - 1];
							int contentIndex = lastSeqNum - ISN + numTimesWrapped*MAX_SEQ_NUM + internalNumWrapped*MAX_SEQ_NUM;
							//cout << "Trying content index:" << contentIndex << endl;
							TCPHeader response(lastSeqNum, 0, received.Window, 0, 0, 0);
							if (1024 + contentIndex < contentSize) {
								currPay = new char[1024];
								memcpy(currPay, contentArr + contentIndex, 1024);
								cout << "Sending data packet " << response.SeqNum << " " << cwnd << " " << ssthresh << endl;
								//cout << "Sending data packet " << response.SeqNum << " Content Index is: " << contentIndex << endl;
								response.setPayload(currPay, 1024);
								int toPush = lastSeqNum + 1024 <= MAX_SEQ_NUM ? lastSeqNum + 1024 : lastSeqNum + 1024 - MAX_SEQ_NUM;
								if (toPush < lastSeqNum) {
									internalNumWrapped++;
								}
								unackedSeqNums.push_back(toPush);
								toSendOut.push_back(response);
							}
							else {
								currPay = new char[contentSize - contentIndex];
								memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
								response.F = 1;
								/*cout << "Sending data packet " << response.SeqNum << ", this is FIN"
									<< " Content Index is: " << contentIndex << " Content Size is: " << contentSize - contentIndex << endl;*/
								cout << "Sending data packet " << response.SeqNum << " " << cwnd << " " << ssthresh << endl;
								response.setPayload(currPay, contentSize - contentIndex);
								int toPush = lastSeqNum + (contentSize - contentIndex) <= MAX_SEQ_NUM ? lastSeqNum + (contentSize - contentIndex) : lastSeqNum + (contentSize - contentIndex) - MAX_SEQ_NUM;
								unackedSeqNums.push_back(lastSeqNum + (contentSize - contentIndex));
								if (toPush < lastSeqNum) {
									internalNumWrapped++;
								}
								//numNewPackets = i + 1;
								toSendOut.push_back(response);
								waitForFINACK = true;
								break;
							}

						}
						for (TCPHeader h : toSendOut) {
							if (sendto(sockfd, h.encode(), h.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
								perror("sendto");
								exit(1);
							}
						}
						unackedSeqNums.erase(unackedSeqNums.begin());
					}
				}
				else {
					//if (dupCount == 3) {
					//	dupCount = 0;
					//	//int headSeqNum = unackedSeqNums[0];
					//	int contentIndex = received.AckNum - ISN + numTimesWrapped*MAX_SEQ_NUM;
					//	TCPHeader response(received.AckNum, 0, received.Window, 0, 0, 0);
					//	char* currPay;
					//	if (numToCopy + contentIndex < contentSize) {
					//		currPay = new char[numToCopy];
					//		memcpy(currPay, contentArr + contentIndex, numToCopy);
					//		cout << "Sending data packet " << response.SeqNum << " Restransmission (Dup Timeout)" << " Content Index is: " << contentIndex << endl;
					//		response.setPayload(currPay, numToCopy);
					//	}
					//	else {
					//		currPay = new char[contentSize - contentIndex];
					//		memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
					//		response.F = 1;
					//		cout << "Sending data packet " << response.SeqNum << ", this is FIN" << " Content Index is: " << contentIndex << endl;
					//		response.setPayload(currPay, contentSize - contentIndex);
					//	}
					//	unackedSeqNums.clear();
					//	unackedSeqNums.insert(unackedSeqNums.begin(),received.AckNum);
					//	if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
					//		perror("sendto");
					//		exit(1);
					//	}
					//} else{
					//	dupCount++;
					//}
					//cout << "getCurrentTimestamp(): " << getCurrentTimestamp() << " - oldestTimestamp: " << oldestTimestamp << " = " << getCurrentTimestamp() - oldestTimestamp << endl;
					if (getCurrentTimestamp() - oldestTimestamp > 500) {
						int headSeqNum = received.AckNum;// unackedSeqNums[0] - 1024;
						int contentIndex = headSeqNum - ISN + numTimesWrapped*MAX_SEQ_NUM;
						TCPHeader response(headSeqNum, 0, received.Window, 0, 0, 0);
						char* currPay;
						if (numToCopy + contentIndex < contentSize) {
							currPay = new char[numToCopy];
							memcpy(currPay, contentArr + contentIndex, numToCopy);
							cout << "Sending data packet " << response.SeqNum << " " << cwnd << " " << ssthresh << " Restransmission " << endl;
							response.setPayload(currPay, numToCopy);
						}
						else {
							currPay = new char[contentSize - contentIndex];
							memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
							response.F = 1;
							cout << "Sending data packet " << response.SeqNum << " " << cwnd << " " << ssthresh << " Restransmission " << endl;
							response.setPayload(currPay, contentSize - contentIndex);
						}
						if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
							perror("sendto");
							exit(1);
						}
						internalNumWrapped = 0;
						unackedSeqNums.clear();
						unackedSeqNums.push_back(headSeqNum);
						waitForFINACK = false;
						ssthresh = cwnd / 2;
						cwnd = 1024;
					}
				}
			}
			//We've sent a FIN to the client and now it's FIN-ACK-ing us
			else if(received.A && received.F){
				// close(sockfd);
				cout << "Received FIN-ACK, we're done" << endl;
				cout << "Total Time" << (getCurrentTimestamp() - firstTime) / 1000 << endl;
				return 0;
			}
		}
		else{
			if (begunTransfer) {
				int headSeqNum = unackedSeqNums[0] -1024;
				int contentIndex = unackedSeqNums[0] - 1024 - ISN + numTimesWrapped*MAX_SEQ_NUM;
				TCPHeader response(headSeqNum, 0, rec_window, 0, 0, 0);
				char* currPay;
				if (numToCopy + contentIndex < contentSize) {
					currPay = new char[numToCopy];
					memcpy(currPay, contentArr + contentIndex, numToCopy);
					cout << "Sending data packet " << response.SeqNum << " " << cwnd << " " << ssthresh << " Restransmission " << endl;
					response.setPayload(currPay, numToCopy);
				}
				else {
					currPay = new char[contentSize - contentIndex];
					memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
					response.F = 1;
					cout << "Sending data packet " << response.SeqNum << " " << cwnd << " " << ssthresh << " Restransmission " << endl;
					response.setPayload(currPay, contentSize - contentIndex);
				}
				if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
					perror("sendto");
					exit(1);
				}
				unackedSeqNums.clear();
				unackedSeqNums.push_back(headSeqNum);
				waitForFINACK = false;
				internalNumWrapped = 0;
				ssthresh = cwnd / 2;
				cwnd = 1024;
			}

		}
	}
}
