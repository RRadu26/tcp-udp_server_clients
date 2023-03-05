#include "defh.h"
#include "algorithm"

//	functie care se ocupa de conexiunea cu clientii UDP, in caz ca este primit un mesaj pe sockeul special alocat.
//	aceasta primeste un mesaj si il trimite mai departe, folosind un protocol special(sendUDP_p si receiveUDP_p),
//	tuturor clientilor abonati la topicul semnalat. In cazul in care unul sau mai multi clienti sunt deconectati dar
//	au fd-ul setat pe 1, se adauga la lista to_sendV pachetul care va trebui trimis in cazul reconectarii 
//	clientului/clientilor , se adauga de asemenea intr-o lista proprie clientilor cu pachete ce vor trebui primite
//	(exista 2 liste deoarece sunt liste de pointeri, pentru usurarea implementarii)
void receive_UDP(int sockid, char *buffer, 	std::map<std::string, std::vector<std::pair<int ,client *>>> *topics, std::list<to_send* > *to_sendV) {
	int n;
	struct sockaddr_in from;
	socklen_t len = sizeof(from);
	memset(&from, 0, sizeof(from));
	//	se primeste mesajul in buffer
	n = recvfrom(sockid, buffer, BUFLEN, 0, (struct sockaddr *)&from, &len);
	DIE(n < 0, "recvfrom");
	
	//	se copiaza mesajul intr-o strucutra receiveUDP_p, compartimentata precum este specificat in cerinta
	receiveUDP_p udp_message_header;
	memcpy(&udp_message_header, buffer, sizeof(receiveUDP_p));

	//	se afla numele topicului caruia apartine mesajul
	std::string topic_name(udp_message_header.topic);

	//	se construiesc protocoalele aferente 
    protheader_client client_header;
	client_header.identificator = 0xcafe;
	client_header.type = 4;
	sendUDP_p sendUDP_p;
	memcpy(&(sendUDP_p.from), &from, sizeof(sockaddr_in));

	memcpy(client_header.content, &sendUDP_p, sizeof(sendUDP_p));
	memcpy(client_header.content + sizeof(sendUDP_p), &udp_message_header, sizeof(receiveUDP_p));
	bool lc = false;
	to_send *client_header_tosend = new to_send;
	memcpy(&(client_header_tosend->header), &client_header, sizeof(protheader_client));
	//	se itereaza prin vectorul specific abonatilor la topicul topic_name
	for(auto &c: (*topics)[topic_name]) {
		//	daca clientul este conectat, mesajul se trimite instant
		if(c.second->connected) {
			n = send(c.second->sok, &client_header, sizeof(protheader_client), 0);
			DIE(n < 0 , "send");
		}
		//	daca clientul nu este conectat dar are fd-ul 1 se efectueaza operatiile descrise mai sus
		else if(c.first == 1){
			if(lc == false) {
				client_header_tosend->noclientsleft = 1;
				(*to_sendV).emplace_front(client_header_tosend);
				lc = true;
				c.second->snd.emplace_front(client_header_tosend);
			}
			else {
				(*to_sendV).front()->noclientsleft++;
				c.second->snd.emplace_front(client_header_tosend);
			}			
		}
	}
}

//	functie care se ocupa cu deconectarea unui client TCP. Inchide socketul clientului respectiv 
//	si seteaza client->connected pe false
void disconnect_TCP(int clsok, fd_set *read_fds, std::vector<client *> *clients) {
	char dclient[10];
	for(auto &c: (*clients)) {
		if (c->sok == clsok) {
			strcpy(dclient, c->id);
			c->connected = false;
			c->sok = -1;
			break; 
		}
	}
	std::cout << "Client "<< dclient << " disconnected.\n";
	close(clsok);
	FD_CLR(clsok, read_fds);		
}

//	funcite ce afiseaza daca un client s-a conectat la server
void print_connected_TCP(std::string id, sockaddr_in cli_addr) {
	std::cout << "New client "<< id 
	<< " connected from "  << inet_ntoa(cli_addr.sin_addr)
	<< ":" << ntohs(cli_addr.sin_port) << "\n";	
}
//	functie care se ocupa cu conectarea unui client TCP la server.Dupa ce conexiunea a fost realizata, 
//	serverul nu stie ID-ul clientului respectiv, de aceea clientul trimite la conectare un mesaj
//	ce contine id-ul propriu. Serverul verifica apoi daca clientul cu ID-ul respectiv se conecteaza
//	pentru prima data la server, a mai fost conectat sau daca este conectat deja si actioneaza in
//	consecinta.
void connect_TCP(protheader_client *client_header, std::vector<client *> *clients, int *newsockfd, int clsok, fd_set *read_fds, sockaddr_in cli_addr, std::list<to_send* > *to_sendV) {
	bool known_client = false;
	int n;
	//	se itereaza prin toti clientii cunoscuti
	for(auto &c:(*clients)) {
		//	se compara numele clientului din clients cu id-ul trimis de noul client
		//	in caz ca nu sunt egale, se trece mai departe
		if(strcmp(c->id, client_header->content) != 0)
			continue;
		known_client = true;
		//	daca clientul cu ID-ul este deja conectat se trimite un mesaj catre clientul nou
		//	ce ii semnaleaza acest lucru, acesta isi va termina executarea. Serverul inchide
		//	conexiunea anterior creata si sterge ultimul client din clients(cel creat pentru
		//	acest client, acesta fiind invalid)
		if(c->connected) {
			std::cout << "Client " << c->id <<" already connected.\n";
			(*client_header).type = 2;										
			n = send(clsok, client_header, sizeof(protheader_client), 0);
			FD_CLR(clsok, read_fds);
			close(clsok);
		} 
		//	daca clientul a mai fost conectat inainte dar acum este offline, se seteaza socketul
		//	acestuia ca fiind cel nou alocat. Se sterge ultimul client creat(este invalid, nu au
		//	de ce sa existe 2 clienti in clients cu acelasi ID). Se verifica daca exista pachete
		//	trimise de clientii UDP pe topicuri la care a fost abonat cu fd 1 clientul acesta,
		//	se trimit
		else {
			c->connected = true;
			c->sok = *newsockfd;
			print_connected_TCP(c->id, cli_addr);
			for(auto itr = c->snd.cbegin(); itr != c->snd.end(); itr++) {
				n = send(c->sok, &((*itr)->header), sizeof(protheader_client), 0);
				DIE(n < 0 , "send");
				(*itr)->noclientsleft--;
				c->snd.erase(itr--);
			}
			for(auto itr = (*to_sendV).cbegin(); itr != (*to_sendV).end(); itr++) {
				if((*itr)->noclientsleft == 0) {
					delete (*itr);
					(*to_sendV).erase(itr--);
				}
			}
		}
			(*clients).pop_back();
		break;
		
	}
	//	in cazul in care clientul cu ID-ul specificat nu a mai fost conectat la server, se
	//	seteaza ID-ul noului client creat.
	if(!known_client) {
		strncpy((*clients)[clients->size()-1]->id, client_header->content, 10);
		print_connected_TCP((*clients)[clients->size()-1]->id, cli_addr);
	}
} 

//	functie ce se ocupa cu subscriptiile clientilor la topicuri. Se ruleaza daca un client trimite un mesaj de
//	tipul subscribe sau unsubscribe prin protocolul subscribe_p 
void subscribe_TCP(std::vector<client *> *clients, protheader_client *client_header, std::map<std::string, std::vector<std::pair<int ,client *>>> *topics, int clsok ) {
	int n;
	client *client_cautat;
	//	se cauta in clients clientul vizat
	for(auto &c:(*clients)){
		if(c->sok == clsok) {
			client_cautat = c;
			break;
		}
	}

	subscribe_p subscribe_header;
	memcpy(&subscribe_header, client_header->content, sizeof(subscribe_p));
	std::string topic_name(subscribe_header.topic);
	//	se verifica daca topicul exista sau nu deja.
	if((*topics).find(topic_name) != (*topics).end()) {
		//	daca type-ul este 1, mesajul este de tip subscribe.
		if(subscribe_header.type == 1) {
			bool already_subscribed = false;
			//	se verifica daca clientul este deja abonat la topic, daca este nu se va mai abona inca odata
			for(auto &cauta:(*topics)[topic_name]) {
				if(cauta.second == client_cautat) {
					already_subscribed = true;
					break;
				}
			}
			if(already_subscribed)
				return;
				//	se adauga o pereche dintre un pointer si fd-ul clientului in vectorul aferent
				//	topic_name
			(*topics)[topic_name].push_back(std::make_pair(subscribe_header.sf, client_cautat)); 
			subscribe_header.type = 3;
		}
		//	daca tipul mesajului nu este 1(va fi 2) inseamna ca mesajul este de tip unsubscribe
		//	se sterge clientul din vectorul topic_name 
		else {
			for(int j = 0 ; j< (*topics)[topic_name].size() ; j++) {
				if(client_cautat == (*topics)[topic_name][j].second)
					(*topics)[topic_name].erase((*topics)[topic_name].begin() + j);
			}
			subscribe_header.type = 4;
		}
		memcpy(client_header->content, &subscribe_header, sizeof(protheader_client));
		n = send(client_cautat->sok, client_header, sizeof(protheader_client), 0);
		return;
	}
	//	se executa in cazul in care topicul nu exista deja. Se adauga un topic nou
	//	si clientul in vectorul asociat acestuia
	if(subscribe_header.type == 1) {
		std::vector<std::pair<int ,client *>> cv;
		cv.push_back(std::make_pair(subscribe_header.sf, client_cautat));
		(*topics).insert(std::make_pair(topic_name, cv));
		subscribe_header.type = 3;
		memcpy(client_header->content, &subscribe_header, sizeof(protheader_client));
		n = send(client_cautat->sok, client_header, sizeof(protheader_client), 0);
	
	}
}

//	functie ce se ocupa cu rularea serverului
void run_server(char *argv) {

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	//	vector ce contine toti clientii TCP, conectati sau deconectati(pana la momentul actual)
    std::vector<client *> clients;

	//	un map pentru fiecare topic la care a fost vreodata abonat orice client, format dintr-un string ce
	//	contine numele topicului si un vector de perecti ce contine clientii abonati la topic(pointer catre acestia)
	//	si fd-ul 
	std::map<std::string, std::vector<std::pair<int ,client *>>> topics;

	//	o lista cu toate pachetele ce trebuie trimise catre clientii deconectati cu fd-ul 1 in caz ca se reconecteaza
	std::list<to_send* > to_sendV;

	//	se initializeaza serverul, se seteaza socketurile pentru listen TCP si UDP 
	int sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	fd_set read_fds;	
	fd_set tmp_fds;		
	int fdmax;			


	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	int sockid = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(sockid < 0, "socket");	
	
	portno = atoi(argv);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = bind(sockid, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	int flag = 1;
	
	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");


	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	FD_SET(sockid, &read_fds);
	n = setsockopt(sockfd, SOL_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
	DIE(n < 0, "nagle");

	fdmax = std::max(sockfd, sockid);
	//	Loop
    while(1) {
        tmp_fds = read_fds; 
		//	se foloseste select pentru multiplexarea I/O
	    ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		//	se verifica daca utilizatorul a introdus la tastatura comanda exit,
		//	daca da se inchide serverul
		if (FD_ISSET(0, &tmp_fds)) {
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			DIE(n < 0, "read");

			if (strncmp(buffer, "exit", 4) == 0) {
            	close(sockfd);
				close(sockid);
				for(auto &c:clients) {
					delete c;
					if(c->connected)
						close(c->sok);
				}
				for(auto &t:to_sendV)
					delete t;
				return;
			}
		}

	    for (i = 1; i <= fdmax; i++) {
            memset(buffer, 0, BUFLEN);
		    if (FD_ISSET(i, &tmp_fds) == 0)
				continue;

			//	verifica daca s-a primit ceva pe socketul de UDP
			if (i == sockid) {
				receive_UDP(sockid, buffer, &topics, &to_sendV);
				continue;
			}

			//	verifica daca un client TCP doreste sa se conecteze la server,
			//	daca da accepta conexiunea cu el si seteaza un socket pentru comunicare
			//	se adauga noul client in vectorul de clienti. Se va verifica daca acesta
			//	este deja conectat sau daca are pachete de primit mai tarziu
			if (i == sockfd) {
			    clilen = sizeof(cli_addr);
			    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);					    
				DIE(newsockfd < 0, "accept");
                
				FD_SET(newsockfd, &read_fds);
				flag = 1 ;
				n = setsockopt(newsockfd, SOL_TCP, TCP_NODELAY, (char*) &flag, sizeof(int));
                DIE(n < 0, "nagle");
				if (newsockfd > fdmax) 
                        fdmax = newsockfd;
                clients.push_back(new client(newsockfd)); 
				continue;                       
            }
			//	comunicare cu clientii TCP
			n = recv(i, buffer, sizeof(buffer) - 1, 0);
		    DIE(n < 0, "recv");
		    if (n == 0) {
				disconnect_TCP(i, &read_fds, &clients);
				continue;
			}
			//	se declara un header de protocol standard peste comunicarea cu clientii TCP
            protheader_client client_header;
			memcpy(&client_header, buffer, sizeof(protheader_client));
			//	orice mesaj trimis de un client TCP are un identificator egal cu 0xcafe
			if(client_header.identificator == 0xcafe) {
				//	daca tipul protocolului e 1 serverul a primit un mesaj de tipul id send, dupa
				//	conectare clientii TCP trimit un pachet cu ID-ul lor catre server
				if(client_header.type == 1) {
					connect_TCP(&client_header, &clients, &newsockfd, i, &read_fds, cli_addr, &to_sendV);
				} 
				//	daca tipul protocolului e 3 atunci serverul a primit un mesaj de tipul subscribe/unsubscribe
				else if(client_header.type == 3) 
					subscribe_TCP(&clients, &client_header, &topics, i);
			}
    	}
	}    
}

int main(int argc, char *argv[]) {
    if(argc < 2)
        exit(0);
    run_server(argv[1]);
    return 0;
}
