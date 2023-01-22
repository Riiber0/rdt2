#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include "rdt2.h"
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

unsigned short current_sock_timeout = INITIAL_TIME_OUT;
unsigned short observed_deviation;

static struct sockaddr_in build_sockaddr(char* dest_ip){

	struct sockaddr_in dest_addr;
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(APP_PORT);
	dest_addr.sin_addr.s_addr = inet_addr(dest_ip);

	return dest_addr;
}

static char* rdt_build_message(h_rdt2 header, char* msg){

	char *buf = (char*)malloc(HEAD_LEN + header.msg_size);
	memcpy(buf, (char*)&header, HEAD_LEN);
	memcpy(&buf[HEAD_LEN], msg, header.msg_size);

	return buf;

}

static short checksum(unsigned char *buf, int count){

	register long sum = 0;
	short checksum = 0;

	while(count > 1){

		sum += *(unsigned short*)buf++;
		count -= 2;

	}

	if(count > 0)
		sum += *(unsigned short*)buf++;
	

	while(sum>>16){
		sum = (sum & 0xffff) + (sum >> 16);
	}
	checksum = ~sum;

	return checksum;

}


int rdt_timeout_set(int sockfd, int timeout){

	int sec = timeout/1000;
	int usec = (timeout % 1000)*1000;

	struct timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) == -1) {
		perror("setsockopt");
		return -1;
	}
	
	return 0;

}

int rdtsock(){

	return socket(PF_INET, SOCK_DGRAM, 0);

}

int rdtbind(int sockfd, struct sockaddr_in *addr){

	addr->sin_addr.s_addr = INADDR_ANY;
	addr->sin_port = htons(APP_PORT);
	addr->sin_family = AF_INET;

	if(bind(sockfd, (struct sockaddr*)addr, sizeof(*addr)) == -1){
		perror("bind");
		return -1;
	}

	return 0;
}

int rdtsend(char** msg, char* dest_ip, int sockfd, int send_window){

	//---checking socket timeout---
	struct timeval tv;
	socklen_t optlen = sizeof(struct timeval);
	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, &optlen) == -1) {
		perror("getsockopt");
		return -1;
	}
	if(tv.tv_usec == 0 && tv.tv_sec == 0){
		fprintf(stderr, "timeout not defind");
		return -1;
	}

	//---building messages---
	
	h_rdt2 *header_poiter;
	char *buf[64];

	for(int i = 0; i < send_window; i++){
		h_rdt2 header = {0, i, strlen(msg[i]), 0, 0};

		buf[i] = rdt_build_message(header, msg[i]);
		header_poiter = (h_rdt2*)buf;
		header_poiter->checksum = checksum((unsigned char*)&buf[2], header_poiter->msg_size + HEAD_LEN - 2);
	}

	//---definig address---
	
	struct sockaddr_in dest_addr = build_sockaddr(dest_ip);

	//--defining variables for receiving ack---
	
	char req[HEAD_LEN];
	struct sockaddr_in r_addr;
	socklen_t r_addr_len = sizeof(struct sockaddr_in);
	bzero(&r_addr, r_addr_len);
	h_rdt2 ack_head;
	unsigned short ack_checksum;

	//---defining loop variables---
	
	unsigned short time_pass;
	unsigned short deviation;
	unsigned short timeout = current_sock_timeout;
	unsigned short timeout_counter = 0;
	unsigned char not_done = 1;
	unsigned char last_in_order = 0;
	struct timeval start, current;

	//---send recv loop--
	
	while(last_in_order < send_window-1){
		gettimeofday(&start, NULL);

		for(int i = last_in_order; i < send_window; i++){

			if(sendto(sockfd, buf[i], HEAD_LEN+header_poiter->msg_size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) == -1){
				perror("sendto");
				return -1;
			}
		}
		if(recvfrom(sockfd, req, HEAD_LEN, 0, (struct sockaddr *) &r_addr, &r_addr_len) == -1){

			if(errno != EAGAIN || errno != EWOULDBLOCK){

					perror("recvdrom");
					return -1;

			} else{

				timeout = timeout * 2;
				rdt_timeout_set(sockfd, timeout);
				timeout_counter++;
 				#ifdef RETFLAG
				if(!header_poiter->ret){

					header_poiter->ret = 1;
					header_poiter->checksum = checksum((unsigned char*)&buf[2], header_poiter->payload_len + HEAD_LEN - 2);

				}
				#endif

			}
		} else{

			memcpy(&ack_head, req, HEAD_LEN);
			ack_checksum = checksum((unsigned char*)&req[2], HEAD_LEN - 2);

			if(ack_checksum == ack_head.checksum){

				last_in_order = ack_head.seq_num;
				if(!timeout_counter){

					gettimeofday(&current, NULL);
					time_pass = (current.tv_sec *1000) + current.tv_usec/1000;
					time_pass -= (start.tv_sec *1000) + start.tv_usec/1000;
					deviation = observed_deviation * 0.75 + abs(current_sock_timeout - time_pass) * 0.25;
					current_sock_timeout = current_sock_timeout * 0.875 + (time_pass) * 0.125 +  4 * deviation;
				}
			} 
			#ifdef RETFLAG
			else {
				header_poiter->ret = 1;
				header_poiter->checksum = checksum((unsigned char*)&buf[2], header_poiter->msg_size + HEAD_LEN - 2);

			}
			#endif
		}


	}

	return 0;
}

int rdtrecv(char** ret_buf, int sockfd, int recv_window){
	
	//--setting variales for receaving message---
	struct sockaddr_in src_addr;
	bzero(&src_addr, sizeof(struct sockaddr_in));
	socklen_t r_addr_len;
	char recv_buf[MAX_REQ];
	h_rdt2* recv_head;

	unsigned char last_in_order = 0;
	h_rdt2 *conf_head = (h_rdt2*)malloc(sizeof(h_rdt2));
	conf_head->ack = 1;
	conf_head->msg_size = 0;
	conf_head->ret = 0;
	char* conf_buf;

	//---recv loop---
	while(recvfrom(sockfd, recv_buf, MAX_REQ, 0, (struct sockaddr*)&src_addr, &r_addr_len) != -1 && last_in_order < recv_window-1){
		recv_head = (h_rdt2*)recv_buf;

		if(recv_head->seq_num > last_in_order+1){

			conf_head->seq_num = last_in_order;
			conf_head->checksum = checksum((unsigned char*)&conf_head[2], HEAD_LEN - 2);
			if(sendto(sockfd, &conf_head, HEAD_LEN, 0, (struct sockaddr*)&src_addr, sizeof(src_addr)) == -1){
				perror("sendto");
				return -1;
			}
		}
		if(ret_buf[recv_head->seq_num] == NULL){

			ret_buf[recv_head->seq_num] = &recv_buf[HEAD_LEN];
		}
	}

	free(conf_head);
	return 0;
}
