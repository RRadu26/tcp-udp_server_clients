#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "defh.h"
#include<bits/stdc++.h>

protheader_client header_ids;


void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	//	se initializeaza clientul, se stabileste o conexiune cu serverul si
	//	se seteaza socketul aferent acesteia.

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN2];

	fd_set read_fds;	
	fd_set tmp_fds;		
	int fdmax;			

	if (argc < 3) {
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	//	se trimite un pachet catre server cu ID-ul clientului 
	header_ids.identificator = 0xcafe;
	header_ids.type = 1; 
 
	memcpy(header_ids.content, argv[1], 10);
	n = send(sockfd, &header_ids, sizeof(header_ids), 0);
	memset(header_ids.content, 0, sizeof(header_ids.content) - 1);
	
	int flag = 1;
	n = setsockopt(sockfd, SOL_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
	DIE(n < 0, "nagle");


	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		if (FD_ISSET(0, &tmp_fds)) {
			//	se citesc comenzi de la tastatura
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			DIE(n < 0, "read");
			//	daca s-a dat comanda exit se iese din loop si se inchide programul
			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
			//	daca s-a dat comanda subscribe se trimite un pachet folosind protocolul
			//	subscribe_p care semnaleaza acestuia dorinta clientului de a se abona la
			//	acel topic
			if (strncmp(buffer, "subscribe", 9) == 0) {
				header_ids.type = 3;
				subscribe_p header_subs;
				header_subs.type = 1;
				char *token = strtok(buffer, " ");
				token = strtok(NULL, " ");
				memset(header_subs.topic, 0, sizeof(header_subs.topic) - 1);
				strcpy(header_subs.topic, token);
				token = strtok(NULL, " ");
				header_subs.sf = atoi(token);
				strcpy(header_subs.id, argv[1]);
				memcpy(header_ids.content, &header_subs, sizeof(header_subs));
				n = send(sockfd, &header_ids, sizeof(header_ids), 0);
				memset(header_ids.content, 0, sizeof(header_ids.content) - 1);
				continue;
			}
			//	daca s-a dat comanda unsubscribe se trimite un pachet folosind protocolul
			//	subscribe_p care semnaleaza acestuia dorinta clientului de a se dezabona la
			//	acel topic
			if (strncmp(buffer, "unsubscribe", 11) == 0) {
				header_ids.type = 3;
				subscribe_p header_subs;
				header_subs.type = 2;
				char *token = strtok(buffer, " ");
				token = strtok(NULL, " \n");
				memset(header_subs.topic, 0, sizeof(header_subs.topic) - 1);
				strcpy(header_subs.topic, token);
				header_subs.sf	= 0;	
				strcpy(header_subs.id, argv[1]);
				memcpy(header_ids.content, &header_subs, sizeof(header_subs));
				n = send(sockfd, &header_ids, sizeof(header_ids), 0);
				memset(header_ids.content, 0, sizeof(header_ids.content) - 1);
			}
		}
		//	se primesc mesaje de la client
		if (FD_ISSET(sockfd, &tmp_fds)) {
			
        	memset(buffer, 0, BUFLEN2);
		    n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
		  	DIE(n < 0, "recv");
            protheader_client prc;;
			memcpy(&prc, buffer, sizeof(protheader_client));	
			if (n == 0) {
				close(sockfd);
				break;
			}
			//	mesajul primit trebuie sa aiba identificatorul 0xcafe
			if(prc.identificator == 0xcafe) {
				//	tipul 2 al mesajului este primit in cazul in care clientul a incercat sa
				//	se conecteze la server cu un ID deja folosit de alt client conectat. In acest
				//	caz see opreste rularea clientului curent
				if(prc.type == 2) {	
					break;
				}
				//	tipul 3 al mesajului este primit ca raspuns la requestul de subscribe/unsubscribe
				//	al clientului.
				if(prc.type == 3) {
					subscribe_p header_subs;
					memcpy(&header_subs, prc.content, sizeof(subscribe_p));	
					//	tipul 3 al protocolului subscribe este subscribe response
					if(header_subs.type == 3)
						std::cout << "Subscribed to topic.\n";
					//	tipul 3 al protocolului subscribe este unsubscribe response
					else if(header_subs.type == 4)
						std::cout << "Unsubscribed from topic.\n";
					continue;				
				}
				//	tipul 4 al mesajului este primit in cazul in care pe server a ajuns un pachet trimit de UDP avand
				//	ca topic ceva la care clientul curent este abonat.
				if(prc.type == 4) {
					sendUDP_p sendUDP;
					memcpy(&sendUDP, prc.content, sizeof(sendUDP_p));
					std::cout << inet_ntoa(sendUDP.from.sin_addr) 
					<< ":" << ntohs(sendUDP.from.sin_port) << " - ";
					receiveUDP_p receiveUDP;
					memcpy(&receiveUDP, prc.content + sizeof(sendUDP_p), sizeof(receiveUDP_p));
					std::cout << receiveUDP.topic << " - ";
					switch (receiveUDP.tip_date) {
						case 0: 
						{
							std::cout << "INT - ";
							char sign = (char) *(receiveUDP.continut);
							if(sign == 1)
								std::cout << "-";
							unsigned int numbernetwork ;
							memcpy(&numbernetwork, (receiveUDP.continut) + 1, 4);
							unsigned int number = ntohl(numbernetwork);
							
							std::cout << number<< std::endl;
							break;
						}
						case 1:
						{ 
							std::cout << "SHORT_REAL - ";
							unsigned short int numbernetwork;
							memcpy(&numbernetwork , receiveUDP.continut, 2);
							int number = ntohs(numbernetwork);		
							float nr = (float)number/100;
							std::ios oldState(nullptr);

							printf("%.2f\n", nr);				
							
							break;
						}
						case 2:
						{
							std::cout <<"FLOAT - ";
							char sign = (char) *(receiveUDP.continut);
							if(sign == 1)
								std::cout << "-";
							unsigned int numberf;
							unsigned int numbernetwork ;
							char powzece = receiveUDP.continut[5];
							memcpy(&numbernetwork, (receiveUDP.continut) + 1, 4);
							unsigned int number = ntohl(numbernetwork);
							float nr = (float)number / (float)(pow(10, powzece));
							printf("%f\n", nr);		
							break;
						}
						case 3:
						{
							std::cout << "STRING - " ;
							printf("%s\n", receiveUDP.continut);
							break;
						}
					}
				continue;	
				}
			}	
		}		
	}

	close(sockfd);

	return 0;
}