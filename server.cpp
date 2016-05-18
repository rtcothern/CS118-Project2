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

#include <fstream>
#include <sstream>

#define BUFSIZE 2048

typedef std::string string;

typedef std::vector<uint8_t> ByteBlob;

int
main(int argc, char **argv)
{
	struct sockaddr_in myaddr, remaddr;
	socklen_t addrlen = sizeof(remaddr);
	int recvlen;
	int sockfd;
	int msgcnt = 0;
	char buf[BUFSIZE];

	UdpHeader udp;
	std::cout << std::to_string(sizeof(udp)) << std::endl;


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

	if(contents.empty()){
		std::cerr << "Invalid File" << std::endl;
	}

	while (true) {
    std::cout << "waiting on port " << port << std::endl;
		recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		if (recvlen > 0) {
			buf[recvlen] = 0;
      std::cout << "Received message: " << buf << std::to_string(recvlen) << std::endl;
		}
		else{
      std::cout << "Ah shit... something went wrong..." << std::endl;
    }

		sprintf(buf, "ack %d", msgcnt++);
		printf("sending response \"%s\"\n", buf);
		if (sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, addrlen) < 0)
			perror("sendto");
	}
}
