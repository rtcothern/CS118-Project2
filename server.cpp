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
#include <map>
#include <vector>

#define BUFSIZE 1032
#define MAX_SEQ_NUM 30720

typedef std::string string;

typedef std::vector<char> ByteBlob;

struct sentPacket {
	int seqNum;
	timeval timer;
};


int
main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	socklen_t addrlen = sizeof(remaddr);
	int recvlen;
	int sockfd;
	char buf[BUFSIZE];
	int rec_window;
	vector<sentPacket*> sentVec;
	int cwnd = 1;
	int ssthresh = 4;
	bool allClear = true;


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
	int dupCount = 0;
	int numTimesWrapped = 0;
	bool begunTransfer = false;

	timeval timestamp;
	gettimeofday(&timestamp, NULL);
	//unackedPacketTimstamps[seq_num] = timestamp.tv_usec / 1000;
	srand(timestamp.tv_usec);
	seq_num = rand() % MAX_SEQ_NUM; //Set a random initial sequence number
	cout << "Initial Seq Num: " << seq_num << endl;
	ISN = seq_num;
	contentIndex = 0;

	while (true) {
		recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		if (recvlen > 0) {
			// buf[recvlen] = 0;
			TCPHeader received = TCPHeader::decode(buf, recvlen);

			//Initial connection request, recieved SYN, we're sending SYN-ACK
			if(received.S){
				begunTransfer = false;
				rec_window = received.Window;
				cout << "Received SYN packet, sending SYN-ACK..." << endl;
				TCPHeader synAckHeader(seq_num, 0, received.Window, true, true, false);

				if (sendto(sockfd, synAckHeader.encode(), synAckHeader.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
					perror("sendto");
					exit(1);
				}
			}

			//Last phase of 3-way HS, or normal data flow
			else if(received.A && !received.F){
				begunTransfer = true;
				cout << "Receiving ACK packet " << received.AckNum << endl;

				if (received.AckNum == 1024 - (MAX_SEQ_NUM - seq_num)) {
					numTimesWrapped++;
				}

				//Remove this packet from the sent vector of outstanding packets
				if(received.AckNum - 1024 > 0){
					for(std::vector<sentPacket*>::iterator it = sentVec.begin(); it != sentVec.end(); ++it) {
							if(((*it)->seqNum) == received.AckNum - 1024){
								sentVec.erase(it);
								break;
							}
					}
				}else{ //This else is to account for a wrap around happening
					for(std::vector<sentPacket*>::iterator it = sentVec.begin(); it != sentVec.end(); ++it) {
							if(((*it)->seqNum) == MAX_SEQ_NUM + (received.AckNum - 1024)){
								sentVec.erase(it);
								break;
							}
					}
				}

				//Check if all outstanding data packets have been recieved correctly if not let timout get it
				if(sentVec.empty()){

					if(cwnd>ssthresh){
						cwnd+=1;
					}
					else{
						cwnd = cwnd*2;
					}

					cout << "CWND: " << cwnd << endl;

						//cout << "In The Loop" << endl;
						for(int x=0;x<cwnd;x++){
							int packetSeq = received.AckNum + 1024*x;
							if(packetSeq > MAX_SEQ_NUM){
								packetSeq-=MAX_SEQ_NUM;
							}
							TCPHeader response(packetSeq, 0, received.Window, false, false, 0);
							seq_num = received.AckNum;
							cout << "The sequence number is: " << seq_num << endl;
							char* currPay;
							if (numToCopy + contentIndex < contentSize) {
								currPay = new char[numToCopy];
								memcpy(currPay, contentArr + contentIndex, numToCopy);

								if((MAX_SEQ_NUM - contentIndex + numToCopy) < 0){
									contentIndex = contentIndex + numToCopy - MAX_SEQ_NUM;
								}else{
									contentIndex += numToCopy;
								}

								cout << "Sending data packet " << packetSeq <<  " Content Index is: " << contentIndex- numToCopy << endl;

								//create a struct for the sent packet
								sentPacket* s = new sentPacket();
								s->seqNum = response.SeqNum;
								gettimeofday(&(s->timer), NULL);

								//cout << "The sequence number of the sent packet is: " << to_string(s->seqNum) << endl;
								//cout << "The time the packet was sent is: " << to_string(s->timer.tv_usec /1000) << endl;

								sentVec.push_back(s);
								response.setPayload(currPay, numToCopy);

							}
							else {
								currPay = new char[contentSize - contentIndex];
								memcpy(currPay, contentArr + contentIndex, contentSize - contentIndex);
								response.F = 1;
								cout << "Sending data packet " << packetSeq << ", this is FIN"
								<< " Content Index is: " << contentIndex << " Content Size is: " << contentSize - contentIndex << endl;
								//create a struct for the sent packet
								sentPacket* s = new sentPacket();
								s->seqNum = response.SeqNum;
								gettimeofday(&(s->timer), NULL);

								//cout << "The sequence number of the sent packet is: " << to_string(s->seqNum) << endl;
								//cout << "The time the packet was sent is: " << to_string(s->timer.tv_usec /1000) << endl;

								sentVec.push_back(s);
								response.setPayload(currPay, contentSize - contentIndex );
								if((MAX_SEQ_NUM - contentIndex + numToCopy) < 0){
									contentIndex = contentIndex + numToCopy - MAX_SEQ_NUM;
								}else{
									contentIndex += numToCopy;
								}
								if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
									perror("sendto");
									exit(1);
								}
								break;

							}
							//free(currPay);
							if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
								perror("sendto");
								exit(1);
							}
							delete [] currPay;
						}

						/*
						if (received.AckNum == seq_num + 1024
							|| received.AckNum == seq_num + 1
							|| received.AckNum == 1024 - (MAX_SEQ_NUM - seq_num)) {



						}*/
						/*else {

							dupCount++;
							if (dupCount == 3) {
								TCPHeader response(seq_num, 0, rec_window, 0, 0, 0);
								char* currPay;
								int contInd = seq_num + numTimesWrapped*MAX_SEQ_NUM - ISN - 1;
								if (numToCopy + contInd < contentSize) {
									currPay = new char[numToCopy];
									memcpy(currPay, contentArr + contInd, numToCopy);
									cout << "Sending data packet " << response.SeqNum << " Retransmission (Dup)" << " Content Index is: " << contInd << endl;
									sentVec.push_back(response.SeqNum);
									response.setPayload(currPay, numToCopy);
								}
								else {
									currPay = new char[contentSize - contInd];
									memcpy(currPay, contentArr + contInd, contentSize - contInd);
									response.F = 1;
									cout << "Sending data packet " << response.SeqNum << ", this is FIN" << " Retransmission (Dup)" << " Content Index is: " << contInd << endl;
									sentVec.push_back(response.SeqNum);
									response.setPayload(currPay, contentSize - contInd);
									//contentIndex += numToCopy;
								}
								//free(currPay);
								if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
									perror("sendto");
									exit(1);
								}
								delete [] currPay;
							}
						}*/


					//cout << "Packets Out Right Meow" << endl;
					for(std::vector<sentPacket*>::iterator it = sentVec.begin(); it != sentVec.end(); ++it) {
							//cout << to_string(*it) << endl;
					}



				}
				else{
					/*for (sentPacket* s : sentVec) {
						cout << "Element: " << s->seqNum << endl;

					}*/
				}

			}

			//We've sent a FIN to the client and now it's FIN-ACK-ing us
			else if(received.A && received.F){
				// close(sockfd);
				cout << "Received FIN-ACK, we're done" << endl;
				return 0;
			}

		}
		//Deal with a timeout
		else{
			if(seq_num < 0)
			 std::cout << "Waiting on Client..." << std::endl;
			else if(begunTransfer){
				TCPHeader response(sentVec[0]->seqNum, 0, rec_window, 0, 0, 0);
				char* currPay;
				int contInd = sentVec[0]->seqNum + numTimesWrapped*MAX_SEQ_NUM - ISN - 1;
				if (numToCopy + contInd < contentSize) {
					currPay = new char[numToCopy];
					memcpy(currPay, contentArr + contInd, numToCopy);
					cout << "Sending data packet " << response.SeqNum << " Restransmission (Timeout)" << " Content Index is: " << contInd << endl;
					//create a struct for the sent packet
					/*sentPacket* s = new sentPacket();
					s->seqNum = response.SeqNum;*/
					gettimeofday(&(sentVec[0]->timer), NULL);

					//cout << "The sequence number of the sent packet is: " << to_string(sentVec[0]->seqNum) << endl;
					//cout << "The time the packet was sent is: " << to_string(sentVec[0]->timer.tv_usec /1000) << endl;

					//sentVec.push_back(s);

					response.setPayload(currPay, numToCopy);
				}
				else {
					currPay = new char[contentSize - contInd];
					memcpy(currPay, contentArr + contInd, contentSize - contInd);
					response.F = 1;
					cout << "Sending data packet " << response.SeqNum << ", this is FIN" << " Content Index is: " << contInd << endl;
					//create a struct for the sent packet
					/*sentPacket* s = new sentPacket();
					s->seqNum = response.SeqNum;*/
					gettimeofday(&(sentVec[0]->timer), NULL);

					//cout << "The sequence number of the sent packet is: " << to_string(sentVec[0]->seqNum) << endl;
					//cout << "The time the packet was sent is: " << to_string(sentVec[0]->timer.tv_usec /1000) << endl;

					//sentVec.push_back(s);

					response.setPayload(currPay, contentSize - contInd);
					//contentIndex += numToCopy;
				}
				//free(currPay);
				if (sendto(sockfd, response.encode(), response.getPacketSize(), 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
					perror("sendto");
					exit(1);
				}
				delete [] currPay;
			}

		}
	}
}
