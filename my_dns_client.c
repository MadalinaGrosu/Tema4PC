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

int get_host_by_name(unsigned char *host, unsigned char *server, int query_type) {
	unsigned char *reader, *qname;
	unsigned char buf[1024];
	
	struct sockaddr_in server_addr, ans;
	
	
	
}

// ./my_dns_client domain_name reg_type
int main(int argc, char* argv[]) {
	if (argc != 3) {
		printf("Usage: ./my_dns_client domain_name reg_type\n");
		return -1;
	}
	
	return 0;
}
