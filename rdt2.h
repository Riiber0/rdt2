#ifndef RDT2_H_
#define RDT2_H_

#define MAX_MESAGE 255
#define MAX_WINDOW 64
#define HEAD_LEN sizeof(h_rdt2)
#define MAX_MSG 255
#define MAX_REQ MAX_MSG+HEAD_LEN
#define APP_PORT 6675
#define INITIAL_TIME_OUT 5000

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct h_rdt2{

		unsigned short checksum; //std checksum size
		unsigned short seq_num: 6; //sequence number
		unsigned short msg_size: 8; //payload size 
		unsigned short ack: 1; //ack flag
		unsigned short ret: 1; //control parameter for tests and mesure

	}h_rdt2;

	int rdtsock();
	int rdtbind(int , struct sockaddr_in*);
	int rdtsend(char**, char*, int, int);
	int rdtrecv(char**, int, int);
	int rdt_timeout_set(int sockfd, int timeout);

#ifdef __cplusplus
}
#endif

#endif
