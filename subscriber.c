#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

#define null NULL
#define BUFLEN 256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	1000	// numarul maxim de clienti in asteptare
#define LUNGIME_TOPIC 50	// numarul maxim de clienti in asteptare


void usage(char *file)
{
	fprintf(stderr, "Usage: %s id server_address server_port\n", file);
	exit(0);
}

typedef struct
{
	char mesaj[255];
	int tip_mesaj; //0 pentru id_request, 1 pentru id_response, 2 pentru date
}mesaj_server;

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	char aux[BUFLEN];

	if (argc != 4) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	int fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds;
		int functionalitate = select(fdmax + 1, &tmp_fds, null, null, null);
		DIE(functionalitate < 0, "select");
		
		if(FD_ISSET(0, &tmp_fds)) //date la stdin
		{
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
			buffer[strcspn(buffer, "\n")] = 0;
			memcpy(aux, buffer, BUFLEN);
			char *token;

			mesaj_server mesaj;
	
			token = strtok(buffer, " ");

			if (strcmp(token, "exit") == 0) {
				break;
			}

			if(strcmp(token, "subscribe") == 0) //TODO: error checking daca pui mai mult decat trebuie
			{	
				token = strtok(null, " ");
				char *topic = (char*)malloc(sizeof(char) * 50); //50 de octeti pentru topic
				memcpy(topic, token, 50);
				
				
				token = strtok(null, " "); //topicul nu il verificam, presupunem ca exista
				if(token == null)
				{
					printf("Dupa topic trebuie sa urmeze si optiunea de SF.\n");
					free(topic);
					continue;
				}

				char *sf = (char*)malloc(1); //1 octet pentru topic (0/1 in ascii)
				memcpy(sf, token, 1);

				token = strtok(null, " "); //verificam daca a mai fost pus ceva in plus ce nu trebuie
				if(token != null)
				{
					printf("Prea multe argumente la subscribe.\n");
					free(sf); 
					free(topic);
					continue;
				}

				if(atoi(sf) == 0 || atoi(sf) == 1) //SF poate fi doar 0/1
				{
					memcpy(mesaj.mesaj, aux, 40);
					mesaj.tip_mesaj = 2;
					n = send(sockfd, &mesaj, sizeof(mesaj), 0);
					DIE(n < 0, "send");	
					printf("Subscried to %s succesfully! (SF = %d)\n", topic, atoi(sf));
					free(topic);
					free(sf);
					continue;
				}
				else
				{
					printf("Store forward trebuie sa fie o valoare de 0 sau 1\n");
					free(sf); 
					free(topic);
					continue;
				}
			}

			if(strcmp(token, "unsubscribe") == 0) //TODO: error checking daca pui mai mult decat trebuie
			{	
				token = strtok(null, " ");
				if(token == null)
				{
					printf("Dupa unsubscribe trebuie sa urmeze si un topic.\n");
					continue;
				}
				char *topic = (char*)malloc(sizeof(char) * 50); //50 de octeti pentru topic
				memcpy(topic, token, 50);
				
				token = strtok(null, " "); //verificam daca a mai fost pus ceva in plus ce nu trebuie
				if(token != null)
				{
					printf("Prea multe argumente la unsubscribe.\n");
					free(topic);
					continue;
				}
				memcpy(mesaj.mesaj, aux, 40);
				mesaj.tip_mesaj = 2;
				n = send(sockfd, &mesaj, sizeof(mesaj), 0);
				DIE(n < 0, "send");
				printf("Unsubscribed succesfully from %s", topic);
				free(topic);
				continue;
			}
			
			printf("Nu ai introdus bine comanda de subscribe/unsubscribe.\n");
			printf("Incearca <subscribe topic store_forward> sau <unsubscribe topic>.\n");	
		}
		if (FD_ISSET(sockfd, &tmp_fds)) // date pe socket 
		{
			mesaj_server primit;
			recv(sockfd, &primit, sizeof(primit), 0);
			if(primit.tip_mesaj == 0) //daca e id_request
			{
				mesaj_server raspuns;
				memcpy(raspuns.mesaj, argv[1], strlen(argv[1]));
				raspuns.tip_mesaj = 1; //id_raspuns
				n = send(sockfd, &raspuns, sizeof(raspuns), 0);
				DIE(n < 0, "send");
			}
		}
	}

	close(sockfd);

	return 0;
}
