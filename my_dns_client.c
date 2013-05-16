#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "dns_message.h"

/*
 * Convert from www.google.com format to
 * 3www6google3com0 format
 */
void change_to_dns(unsigned char *dns,unsigned char *host) {
	int lock = 0, i;
	strcat((char *)host,".");
	
	for (i =0; i < (int)strlen((char *)host); i++) {
		if (host[i] == '.') {
			*dns++ = i - lock;
			for (; lock < i; lock++) {
				*dns++ = host[lock];
			}
			lock++;
		}
	}
	*dns++ = '\0';
}

/*
 * Convert from 3www6google3com0 format to 
 * www.google.com
 */
void change_to_host(unsigned char *host) {
	char p;
	int i,j;
	char *dns = (char *)malloc(strlen((const char *)host));
	
	strcpy(dns,(const char *)host);
	
	for (i = 0; i < strlen(dns); i++) {
		p = dns[i];
		for (j = 0; j < (int)p; j++) {
			*host++ = dns[++i];
		}
		if (i < strlen(dns) - 1) {
			*host++ = '.';
		} 
	}
}

unsigned char *read_name(unsigned char *reader, unsigned char *buffer, int *count) {
	unsigned char *name = malloc((sizeof(unsigned char)) * 256);
	unsigned int p = 0, jumped = 0, offset;
	
	if (name == NULL) {
		perror("malloc error in read_name");
	}
	
	*count = 1;
	while (*reader != 0) {
		if (*reader >= 192) {
			offset = (*reader) * 256 + *(reader + 1) - 49152;
			reader = buffer + offset - 1;
			jumped = 1;
		} else {
			name[p++] = *reader;
		}
		
		reader = reader + 1;
		if (jumped == 0) {
			*count = *count + 1;
		}
	}
	
	if (jumped == 1) {
		*count = *count + 1;
	}
	
	name[p] = '\0';
	change_to_host(name);	
	
	return name;
}

int get_host_by_name(unsigned char *host, unsigned char *server, int query_type, int fd) {
	unsigned char *reader, *qname;
	unsigned char buf[1024];
	
	struct sockaddr_in server_addr, a;
	int sockfd;
	
	dns_header_t *dns_header = NULL;
	dns_question_t *dns_ques = NULL;
	
	int i,j, stop;
	
	/* Handling sockets */
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	
	if (sockfd < 0) {
		perror("Error opening the socket");
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(53);
	inet_aton((char *)server, &server_addr.sin_addr);	
	
	/* Handling dns header */
	dns_header = (dns_header_t *)&buf;
	dns_header->id = (unsigned short) htons(getpid());
	dns_header->qr = 0; // this is a querry
	dns_header->qdcount = htons(1); // one entry in question section
	dns_header->rd = 1; // recursion desired
	
	/* Handling dns question */
	qname = (unsigned char*)&buf[sizeof(dns_header_t)];
	change_to_dns(qname,host);
	
	dns_ques = (dns_question_t *) &buf[sizeof(dns_header_t) + strlen((const char *)qname) + 1];
	dns_ques->qtype = htons(query_type);  // query type 
	dns_ques->qclass = htons(1); // IN class
	
	/* Sending the dns query */
	size_t length = sizeof(dns_header_t) + strlen((const char *)qname) + 1 + sizeof(dns_question_t);
	if (sendto(sockfd, (char *) buf, length, 0, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		perror("Error in sendto");
		return -1;
	}
	
	/* Recieving answer */
	int fdmax;
	fd_set read_fds;
	struct timeval timeout;
	
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	
	FD_ZERO(&read_fds);
	FD_SET(sockfd,&read_fds);
	
	fdmax = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
	
	if (fdmax == -1) {
		printf("Error in select\n");
		return -1;
	}
	
	if (fdmax == 0) {
		printf("Timeout dns server!\n");
		return -1;
	}
	
	if (FD_ISSET(sockfd, &read_fds)) {
		i = sizeof(server_addr);
		if (recvfrom(sockfd, (char *)buf, 1024, 0, (struct sockaddr *) &server_addr, &i) < 0) {
			printf("Error in recvfrom\n");
			return -1;
		}
		
		dns_header = (dns_header_t *)buf;
		/* move ahead the header and question section */
		reader = &buf[length];
		char *header = "\n\n ANSWER SECTION:\n";
		if (ntohs(dns_header->ancount) > 0) {
			write(fd,header,strlen(header));
		}
		
		stop = 0;
		res_record answer;
		
		for (i = 0; i < ntohs(dns_header->ancount); i++) {
			memset(&answer,0,sizeof(answer));
			answer.name = read_name(reader,buf,&stop);
			reader += stop;
			
			answer.resource = (dns_rr_t *)reader;
			reader += sizeof(dns_rr_t);
			
			memset(header,0,strlen(header));
			sprintf(header,"%s\tIN\t",answer.name);
			write(fd,header,strlen(header));
			
			switch (ntohs(answer.resource->type)) {
				case 1 : // IPv4 address
					{
						answer.rdata = (unsigned char*)malloc(ntohs(answer.resource->rdlength));
						
						for (j = 0; j < ntohs(answer.resource->rdlength); j++) {
							answer.rdata[j] = reader[j];
						}
						answer.rdata[ntohs(answer.resource->rdlength)] = '\0';
						
						long *p;
						p=(long*)answer.rdata;
						a.sin_addr.s_addr=(*p); 
						
						memset(header,0,strlen(header));
						sprintf(header,"A\t%s\n",inet_ntoa(a.sin_addr));
						write(fd,header,strlen(header));
						break;
					}
				case 6 : /* Start Of a zone of Authority */
					{
						unsigned char *mname = read_name(reader,buf,&stop);
						reader += stop;
						
						unsigned char *rname = read_name(reader,buf,&stop);
						reader += stop;
						
						unsigned int serial;
						memcpy(&serial,reader,4);
						reader += 4;
						
						unsigned int retry, refresh, expire, minimum;
						memcpy(&refresh,reader,4);
						reader += 4;
						memcpy(&retry,reader,4);
						reader += 4;
						memcpy(&expire,reader,4);
						reader += 4;
						memcpy(&minimum,reader,4);
						reader += 4;
						
						memset(header,0,strlen(header));
						sprintf(header,"SOA\t%s\t%s\t%i\t%i\t%i\t%i\t%i\n",mname,rname,serial,refresh,retry,expire,minimum);
						write(fd,header,strlen(header));				
						
						break;
					}
				case 15 : /* Mail exchange */
					{
						break;
					}
				default :
					{
						
					}
			}
			
			
		}
	}
	
}

// ./my_dns_client domain_name reg_type
int main(int argc, char* argv[]) {
	if (argc != 3) {
		printf("Usage: ./my_dns_client domain_name reg_type\n");
		return -1;
	}
	
	return 0;
}
