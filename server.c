#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <stdlib.h>
#include <string.h>

#define null NULL
#define BUFLEN 256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	1000	// numarul maxim de clienti in asteptare
#define LUNGIME_TOPIC 50	// numarul maxim de clienti in asteptare


void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

typedef struct
{
	char mesaj[255];
	int tip_mesaj; //0 pentru id_request, 1 pentru id_response, 2 pentru date
}mesaj_server;

typedef struct 
{
	char id[10];
	int descriptor;
	unsigned short port;
	struct in_addr ip;
	char **topics; //adresele topicelor ca sa nu stocam informatii in plus daca avem
	// 2 clineti care sunt abonati la acelasi topic
}client;


typedef struct
{
	client clienti[MAX_CLIENTS];
	client posibili_clienti[MAX_CLIENTS];
	int mapare_abonamente[MAX_CLIENTS][100]; //mapare client id la topic
	char abonamente[MAX_CLIENTS][LUNGIME_TOPIC]; //topicuri
	int numar_clienti;
	int numar_posibili_clienti;
}bazaDate;

int main(int argc, char *argv[])
{
	int socket_server, newsockfd, port_server;
	int n, i, valoare_returnata;
	char buffer[BUFLEN];
	struct sockaddr_in date_server, date_client;
	socklen_t lungime_sockaddr_client;

	fd_set set_descriptori;	// multimea de citire folosita in select()
	fd_set set_temporar;		// multime folosita temporar
	int valoare_maxima_descriptor;			// valoare maxima fd din multimea read_fds

	if (argc != 2) {
		usage(argv[0]);
	}

	//pregatim descriptorii
	FD_ZERO(&set_descriptori);
	FD_ZERO(&set_temporar);

	//pregatim serverul sa asculte conexiuni noi
	socket_server = socket(AF_INET, SOCK_STREAM, 0);
	DIE(socket_server < 0, "socket");

	port_server = atoi(argv[1]);
	DIE(port_server == 0, "atoi");

	memset((char *) &date_server, 0, sizeof(date_server));
	date_server.sin_family = AF_INET;
	date_server.sin_port = htons(port_server);
	date_server.sin_addr.s_addr = INADDR_ANY;

	valoare_returnata = bind(socket_server, (struct sockaddr *) &date_server, sizeof(struct sockaddr));
	DIE(valoare_returnata < 0, "bind");

	valoare_returnata = listen(socket_server, MAX_CLIENTS);
	DIE(valoare_returnata < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(socket_server, &set_descriptori);
	// adaugam si stdin la multimea de descriptori in cazul in care vrem sa dam exit
	FD_SET(0, &set_descriptori);

	//ar trebui sa avem in set : stdin stdout stderr socket_server (socket_server + 1) ...
	valoare_maxima_descriptor = socket_server;

	bazaDate database; 
	database.numar_clienti = 0;
	database.numar_posibili_clienti = 0;
	
	int run_server = 1;

	while (run_server) {
		set_temporar = set_descriptori; 
		
		valoare_returnata = select(valoare_maxima_descriptor + 1, &set_temporar, null, null, null);
		DIE(valoare_returnata < 0, "select");

		for (i = 0; i <= valoare_maxima_descriptor; i++) {
			if (FD_ISSET(i, &set_temporar)) {
				if(i == 0) // daca trebuie sa iesim, orice alt output este ignorat
				{
					char buffer[10];
					scanf("%s", buffer);
					buffer[strcspn(buffer, "\n")] = 0;
					if(strcmp(buffer, "exit") == 0)
					{
						run_server = 0;
						break;
					}
				} else if (i == socket_server) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					lungime_sockaddr_client = sizeof(date_client);
					newsockfd = accept(socket_server, (struct sockaddr *) &date_client, &lungime_sockaddr_client);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &set_descriptori);
					if (newsockfd > valoare_maxima_descriptor) {  //probabil e always true
						valoare_maxima_descriptor = newsockfd;
					}

					client posibil_client;
					posibil_client.descriptor = newsockfd;
					posibil_client.port = date_client.sin_port;
					posibil_client.ip = date_client.sin_addr;
					database.posibili_clienti[database.numar_posibili_clienti] = posibil_client;
					database.numar_posibili_clienti++;

					mesaj_server handshake;
					handshake.tip_mesaj = 0;
					n = send(newsockfd, &handshake, sizeof(handshake), 0);
					DIE(n < 0, "send");
				} else{
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					mesaj_server primit;
					n = recv(i, &primit, sizeof(primit), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						printf("Client %d disconnected.\n", i);
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &set_descriptori);
					} else {
						//printf ("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, primit.mesaj);
						if(primit.tip_mesaj == 1) //raspuns la request
						{
							int index = 0;
							client client;
							memset(client.id, 0, 10);
							for (int j = 0; j < database.numar_posibili_clienti; j++)
							{
								if(database.posibili_clienti[i].descriptor == j) //daca descriptorul cu care a fost creat clientul este la fel cu descriptorul pe care s-au primit date
								{
									index = j;
									client.descriptor = i;
									client.ip = database.posibili_clienti[j].ip;
									client.port = database.posibili_clienti[j].port;
									memcpy(client.id, primit.mesaj, strlen(primit.mesaj));
									database.clienti[database.numar_clienti] = client;
								}
							}
							for(int j = index; j < index - 1; j++) 
							{
								database.posibili_clienti[j] = database.posibili_clienti[j + 1];
							}
							memset(&database.posibili_clienti + database.numar_posibili_clienti, 0, sizeof(client));
							database.numar_posibili_clienti--;
							database.numar_clienti++;
							
							printf("New client %s connected from %s:%d\n",
							client.id, inet_ntoa(client.ip), ntohs(client.port));
						}
						else //daca sunt date
						{
							printf("date - %s", primit.mesaj);
						}
						
						fflush(stdout);
					}
				}
			}
		}
	}
	close(socket_server);
	return 0;
}
