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

#define DNS_PORT 53
#define BUFLEN 1024
#define NAME 256

dns_rr_t records[100];

unsigned char* convert(unsigned char* read_ptr, unsigned char* buffer, int *count){
	unsigned char *name;
	name = (unsigned char*) calloc(NAME, sizeof(unsigned char));

	unsigned int poz = 0, offset;
	int jumped = 0;
	int i, j;

	*count = 1;
	name[0] = '\0';

	while (*read_ptr != 0){	
		if (*read_ptr >= 192){	
			// 192 = 1100 0000 -> testeaza daca primii doi biti sunt 1.
			offset =(*read_ptr)*(1<<8)+*(read_ptr+1)-(((1<<16)-1)-((1<<14)-1));
			// scade 11000000 0000000
			read_ptr = buffer + offset - 1;
			jumped = 1;	
		}
		else
			name[poz++] = *read_ptr;
		read_ptr += 1;	

		if ( jumped==0 )
			count++;
	}

	name[poz] = '\0';	
	if (jumped==1)
		count++;

	for (i = 0; i < (int)strlen((const char*)name); ++i){
		poz = name[i];
		for (j = 0; j < (int)poz; ++j){
			name[i] = name[i + 1];
			++i;
		}
		name[i] = '.';
	}
	name[i-1] = '\0';

	return name;
}

char* type_to_string(int type){
	switch(type){
		case A: 
			return "A";
		case NS: 
			return "NS";
		case CNAME: 
			return "CNAME";
		case MX: 
			return "MX";
		case SOA: 
			return "SOA";
		case TXT: 
			return "TXT";
		default: 
			return "";
	}
}

char* class_type_to_string(int type){
	if (type == 1)
		return "IN";
	return "";
}

int string_to_type(char *type){
	if ( strcmp(type, "A") == 0 )
		return A;
	if ( strcmp(type, "NS") == 0 )
		return NS;
	if ( strcmp(type, "CNAME") == 0)
		return CNAME;
	if ( strcmp(type, "MX") == 0)
		return MX;
	if ( strcmp(type, "SOA") == 0)
		return SOA;
	if ( strcmp(type, "TXT") == 0)
		return TXT;
	return -1;
}

int main( int argc, char* argv[] ){
	
	if ( argc != 3 ){
		fprintf( stderr, "Usage: ./my_dns_client hostname/address type\n" );
    	exit(0);
	}

	FILE *f = fopen("dns_servers.conf","r");
	FILE *g = fopen("logfile","w");
	char destination[BUFLEN];
	int i;

	while ( (fgets(destination, BUFLEN, f)) > 0 ) {
		if ( strncmp(destination,"#",strlen("#"))!=0 && strncmp(destination," ",strlen(" "))!=0 && strncmp(destination,"\n",strlen("\n"))!=0 ){
			char *p;
			p = strtok(destination,"\n");
			memcpy(destination,p,sizeof(p));
			
			int type ;
			if (strncmp(argv[2],"A",sizeof("A"))==0)
				type = A;
			if (strncmp(argv[2],"MX",sizeof("MX"))==0)
				type = MX;
			if (strncmp(argv[2],"NS",sizeof("NS"))==0)
				type = NS;
			if (strncmp(argv[2],"CNAME",sizeof("CNAME"))==0)
				type = 	CNAME;
			if (strncmp(argv[2],"SOA",sizeof("SOA"))==0)
				type = SOA;
			if (strncmp(argv[2],"TXT",sizeof("TXT"))==0)
				type = TXT ;

			struct sockaddr_in serv_addr;
			int sockfd;			
			int readsocks;				

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(DNS_PORT);
			inet_aton((char *)destination, &serv_addr.sin_addr);	

			sockfd = socket(AF_INET, SOCK_DGRAM, 0);
			if (sockfd < 0){
				fprintf(stderr,"ERROR opening socket");
			}
			else {  
				unsigned char buf[BUFLEN] ;
				unsigned char *host = (unsigned char*) malloc( strlen(argv[1]) * (sizeof(unsigned char))) ;
				memcpy(host,argv[1],strlen(argv[1]));

				dns_header_t *dns_header = NULL ;	
				dns_question_t *dns_question = NULL ;	

				dns_header = (dns_header_t*)&buf;
				dns_header->id = (unsigned short) htons(1) ; 
				dns_header->rd = 1 ;
				dns_header->tc = 0 ;
				dns_header->aa = 0 ;
				dns_header->opcode = 0 ;
				dns_header->qr = 0 ;
				dns_header->rcode = 0 ;
				dns_header->z = 0 ;
				dns_header->ra = 0 ;
	
				dns_header->qdcount = htons(1);
				dns_header->ancount = htons(0);
				dns_header->nscount = htons(0);
				dns_header->arcount = htons(0);

				unsigned char *result;
				result = (unsigned char*) &buf[sizeof(dns_header_t)]; 
				memset(result,0,sizeof(result));

				int j,point;
				point = 0;
				for ( i = 0 ; i<strlen((char*)host) ; i++ ){
					if (host[i]=='.'){
						result[point] = i-point ; 
						for ( j=0 ; j<i-point ; j++ ){
							result[point+j+1]=host[point+j];
						}
						point = i+1;
					}
				}
				result[point] = i-point ;
				for ( j=0 ; j<i-point ; j++ ){
					result[point+j+1]=host[point+j];
				}
				point = i+1;
				result[point]=0; 
				result[point]='\0'; 

				dns_question = (dns_question_t*)&buf[sizeof(dns_header_t)+(strlen((const char*)result)+1)]; 
				dns_question->qclass = htons(1);	
				dns_question->qtype = htons(type);
				
				if (sendto(sockfd, (char *)buf, 
					sizeof(dns_header_t) + (strlen((const char*)result)+1) + sizeof(dns_question_t)
					, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
					fprintf(stderr,"ERROR in sendto");
				}
				else {
					fd_set readfds;
					struct timeval TIMEOUT;

					FD_ZERO(&readfds);
					FD_SET(sockfd, &readfds);

					TIMEOUT.tv_sec = 2;
					TIMEOUT.tv_usec = 0;

					readsocks = select(sockfd + 1, &readfds, NULL, NULL, &TIMEOUT);

					if ( readsocks == -1 ){
						fprintf(stderr,"Error in select");
					}
					else 
					if ( readsocks == 0 ){
						fprintf(stderr, "Timeout in select\n");
					}
					else {
						if ( FD_ISSET(sockfd, &readfds) ){
							
							int i = sizeof(serv_addr);

							if ( recvfrom(sockfd, (char *)buf, BUFLEN, 0, (struct sockaddr*) &serv_addr, (socklen_t*) &i) < 0 )
								fprintf(stderr,"Error in recvfrom");
							else {
								fprintf(g,"; %s - %s %s\n\n",destination,argv[1],argv[2]);

								dns_header = (dns_header_t*) buf;
							
								unsigned char* answer;
								answer = (unsigned char*) &buf[sizeof(dns_header_t)+(strlen((const char*)result))
									+1+sizeof(dns_question_t)]; 
								
								unsigned char *next = (unsigned char*) malloc(NAME*sizeof(unsigned char));

								int hey = 0;
							
								if ( htons(dns_header->ancount) > 0 ) { 
									fprintf(g,";; ANSWER SECTION:\n");
									for (i=0;i<htons(dns_header->ancount);i++){
										memset(next,0,NAME);
										memcpy(next,convert(answer,buf,&hey),NAME);
										strcat((char*)next,".");
										answer = answer + hey ;

										dns_rr_t *record=NULL;
										record=(dns_rr_t*)&answer[1];
										answer = answer + sizeof(dns_rr_t)-1;

										unsigned char *rdata = (unsigned char*) malloc((ntohs(record->rdlength)) *sizeof(unsigned char));
										memcpy(rdata,answer,ntohs(record->rdlength));

										rdata[ntohs(record->rdlength)]='\0';
										if (htons(record->type)==A) { 
											long *p;
											p = (long*)rdata;
											serv_addr.sin_addr.s_addr = (*p);
											answer = answer + ntohs(record->rdlength);
											fprintf(g,"%s %s %s %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),inet_ntoa(serv_addr.sin_addr));
										}
										else
										if ( htons(record->type) == MX ) { 
											unsigned short *p = NULL;
											p = (unsigned short *) answer;
											answer = answer + 2;

											unsigned char *q = (unsigned char*) malloc(NAME*sizeof(unsigned char));
											memset(q,0,NAME);
											memcpy(q,convert(answer,buf,&hey),NAME);
											answer = answer + ntohs(record->rdlength) - 2 ;
											fprintf(g,"%s %s %s %d %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),ntohs(*p),q);
											free(q);
										}
										else { 
											unsigned char *q = (unsigned char*) malloc(NAME*sizeof(unsigned char));
											memset(q,0,NAME);
											memcpy(q,convert(answer,buf,&hey),NAME);
											answer = answer + ntohs(record->rdlength) ;											

											fprintf(g,"%s %s %s %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),q);
											free(q);
										}
										
									}
									fprintf(g,"\n");
								}

								if ( htons(dns_header->nscount) > 0 ) { 
									fprintf(g,";; AUTHORITY SECTION:\n");
									for (i=0;i<htons(dns_header->nscount);i++){
										memset(next,0,NAME);
										memcpy(next,convert(answer,buf,&hey),NAME);
										strcat((char*)next,".");
										answer = answer + hey ;

										dns_rr_t *record=NULL;
										record=(dns_rr_t*)&answer[1];
										answer = answer + sizeof(dns_rr_t)-1;

										unsigned char *rdata = (unsigned char*) malloc((ntohs(record->rdlength)) *sizeof(unsigned char));
										memcpy(rdata,answer,ntohs(record->rdlength));
										rdata[ntohs(record->rdlength)]='\0';

										if (htons(record->type)==A) { 
											long *p;
											p = (long*)rdata;
											serv_addr.sin_addr.s_addr = (*p);
											answer = answer + ntohs(record->rdlength);
											fprintf(g,"%s %s %s %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),inet_ntoa(serv_addr.sin_addr));
										}
										else
										if ( htons(record->type) == MX ) { 
											unsigned short *p = NULL;
											p = (unsigned short *) answer;
											answer = answer + 2;

											unsigned char *q = (unsigned char*) malloc(NAME*sizeof(unsigned char));
											memset(q,0,NAME);
											memcpy(q,convert(answer,buf,&hey),NAME);
											answer = answer + ntohs(record->rdlength) - 2 ;
											fprintf(g,"%s %s %s %d %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),ntohs(*p),q);
											free(q);
										}
										else { 
											unsigned char *q = (unsigned char*) malloc(NAME*sizeof(unsigned char));
											memset(q,0,NAME);

											memcpy(q,convert(answer,buf,&hey),NAME);
											answer = answer + ntohs(record->rdlength) ;
			
											fprintf(g,"%s %s %s %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),q);
											free(q);
										}
										
									}
									fprintf(g,"\n");
								}

								if ( htons(dns_header->arcount) > 0 ) {
									fprintf(g,";; ADDITIONAL SECTION:\n");
									for (i=0;i<htons(dns_header->arcount);i++){
										memset(next,0,NAME);
										memcpy(next,convert(answer,buf,&hey),NAME);
										strcat((char*)next,".");
										answer = answer + hey ;

										dns_rr_t *record=NULL;

										record=(dns_rr_t*)&answer[1];
										answer = answer + sizeof(dns_rr_t)-1;

										unsigned char *rdata = (unsigned char*) malloc((ntohs(record->rdlength)) *sizeof(unsigned char));
										memcpy(rdata,answer,ntohs(record->rdlength));

										rdata[ntohs(record->rdlength)]='\0';

										if (htons(record->type)==A) { 
											long *p;
											p = (long*)rdata;
											serv_addr.sin_addr.s_addr = (*p);
											answer = answer + ntohs(record->rdlength);
											fprintf(g,"%s %s %s %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),inet_ntoa(serv_addr.sin_addr));
										}
										else 
										if ( htons(record->type) == MX ) { 
											unsigned short *p = NULL;
											p = (unsigned short *) answer;
											answer = answer + 2;

											unsigned char *q = (unsigned char*) malloc(NAME*sizeof(unsigned char));
											memset(q,0,NAME);
											memcpy(q,convert(answer,buf,&hey),NAME);
											answer = answer + ntohs(record->rdlength) - 2 ;
											fprintf(g,"%s %s %s %d %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),ntohs(*p),q);
										free(q);
										}
										else { 
											unsigned char *q = (unsigned char*) malloc(NAME*sizeof(unsigned char));
											memset(q,0,NAME);
											memcpy(q,convert(answer,buf,&hey),NAME);
											answer = answer + ntohs(record->rdlength) ;
											fprintf(g,"%s %s %s %s\n",next,class_type_to_string(ntohs(record->class)),type_to_string(ntohs(record->type)),q);
											free(q);
										}
									}
									fprintf(g,"\n");
								}	

								break ;
							}
						}
					}
				}
			}
		}
	}

	fclose(f);
	fclose(g);

	return 0;

}