#include <stdio.h>
#include <stdlib.h>
#include "rdt2.h"

int main(int argc, char** argv){

	char* dest_ip = argv[1];

	int sockfd = rdtsock();
	rdt_timeout_set(sockfd, INITIAL_TIME_OUT);

	char** msg;
	
	msg[0] = "msg 1";
	msg[1] = "msg 2";
	msg[2] = "smg 3";

	rdtsend(msg, dest_ip, sockfd, 3);

	printf("fim\n");

	return 0;
}
