#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include "rdt2.h"

int main(int argc, char** argv){

	int sockfd = rdtsock();
	char **buf;
	struct sockaddr_in server_addr;

	rdtbind(sockfd, &server_addr);

	if(rdtrecv(buf, sockfd, 3) > 0){
		for(int i = 0; i < 3; i++)
			printf("%s\n", buf[i]);
	}

	return 0;
}
