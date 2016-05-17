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

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iterator>

#define BUFLEN 2048
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

	/* now let's send the messages */

	for (i=0; i < MSGS; i++) {
		printf("Sending packet %d to %s port %d\n", i, hostIP, port);
		sprintf(buf, "This is packet %d", i);
		if (sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen)==-1) {
			perror("sendto");
			exit(1);
		}
		/* now receive an acknowledgement from the server */
		recvlen = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
                if (recvlen >= 0) {
                        buf[recvlen] = 0;	/* expect a printable string - terminate it */
                        printf("received message: \"%s\"\n", buf);
                }
	}
	close(sockfd);
	return 0;
}
