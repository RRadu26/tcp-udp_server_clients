#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <netinet/tcp.h>
#include <list>
#define MAX_CLIENTS 50
#define BUFLEN 1551
#define BUFLEN2 1577
#define DIE(assertion, call_description)	\
	do {						 			\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)
     
typedef struct protheader_client{
	int identificator ;
	int type; // 1 - id send ; 2 - id already connected ; 3 - subscribe ; 4 - send_topic_message
	char content[1567];
}protheader_client;

typedef struct subscribe_p{
	char id[10];
	int type; // 1 - subscribe ; 2 - unsubscribe ; 3 - confirm subscribed ; 4 - confirm unsubscribed
	char topic[50];
	int sf;
}subscribe_p;

typedef struct receiveUDP_p{
	char topic[50];
	char tip_date;
	char continut[1500];
}receiveUDP_p;

typedef struct sendUDP_p{
	struct sockaddr_in from;
}sendUDP_p;
typedef struct to_send{
	protheader_client header;
	int noclientsleft;
}tosend;
class client {
    public:
		std::list<to_send *> snd;
        char id[10];
        int sok;
        bool connected;
        client(int sok) {
            connected = true;
            this->sok =sok;
        }
};