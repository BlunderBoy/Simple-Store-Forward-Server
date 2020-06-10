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
#include <math.h>
#include <iostream>
#include <netinet/tcp.h>

#define null NULL
#define BUFLEN 256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	1000	// numarul maxim de clienti in asteptare
#define LUNGIME_TOPIC 50	// numarul maxim de clienti in asteptare


void usage(char *file)
{
	fprintf(stderr, "Usage: %s id server_address server_port\n", file);
	exit(0);
}

class mesaj_server
{
    public:
	char mesaj[255];
	int tip_mesaj; //5 pentru id_request, 1 pentru id_response, 2 pentru sub/unsub, 3 close_connection, 4 mesaj_udp
	char id[10];
};

class MesajeUDP
{
	public:
	char topic[50];
	unsigned char tip_date; //0,1,2,3
	char continut[1500];
	unsigned short port;
	struct in_addr ip;
};

void display_mesaj(MesajeUDP mesaj)
{
	switch(mesaj.tip_date)
	{
		case 0:
		{
			uint32_t *numar = (uint32_t*)(mesaj.continut + 1);
			if(mesaj.continut[0] == 1)
			{
				printf("%s:%d - %s - INT - -%u\n", 
				inet_ntoa(mesaj.ip), ntohs(mesaj.port),
				mesaj.topic, ntohl(*numar));
			}
			else
			{
				printf("%s:%d - %s - INT - %u\n", 
				inet_ntoa(mesaj.ip), ntohs(mesaj.port),
				mesaj.topic, ntohl(*numar));
			}
			break;
		}
		case 1: 
		{
			uint16_t *numar = (uint16_t*)mesaj.continut;
			double print = (double)(ntohs(*numar)) / 100;
			printf("%s:%d - %s - SHORT_REAL - %.3f\n", 
			inet_ntoa(mesaj.ip), ntohs(mesaj.port),
			mesaj.topic, print);
			break;
		}
		case 2:
		{
			uint32_t *numar = (uint32_t*)(mesaj.continut + 1);
			uint8_t *putere = (uint8_t*)(mesaj.continut + 1 + sizeof(uint32_t));
			float print = (ntohl(*numar)) * pow(10, (-1)*(*putere));
			if(mesaj.continut[0] == 1)
			{
				printf("%s:%d - %s - FLOAT - -%.3f\n", 
				inet_ntoa(mesaj.ip), ntohs(mesaj.port),
				mesaj.topic, print);
			}
			else
			{
				printf("%s:%d - %s - FLOAT - %.3f\n", 
				inet_ntoa(mesaj.ip), ntohs(mesaj.port),
				mesaj.topic, print);
			}
			break;
		}
		case 3: 
		{
			printf("%s:%d - %s - STRING - %s\n", 
			inet_ntoa(mesaj.ip), ntohs(mesaj.port),
			mesaj.topic, mesaj.continut);
			break;
		}
		default:
				break;
	}
	fflush(stdout);
}

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
	int nodelay = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));

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
			strcpy(aux, buffer);
			char *token;

			mesaj_server mesaj;
	
			token = strtok(buffer, " ");

			if (strcmp(token, "exit") == 0) {
				break;
			}

			if(strcmp(token, "subscribe") == 0) //TODO: error checking daca pui mai mult decat trebuie
			{	
				token = strtok(null, " ");
				char *topic = (char*)malloc(sizeof(char) * (strlen(token) + 1)); //50 de octeti pentru topic
				strcpy(topic, token);
				
				
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
					strcpy(mesaj.mesaj, aux);
					mesaj.tip_mesaj = 2;
					strcpy(mesaj.id, argv[1]);
					n = send(sockfd, &mesaj, 1500, 0);
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

			if(strcmp(token, "unsubscribe") == 0) 
			{	
				token = strtok(null, " ");
				if(token == null)
				{
					printf("Dupa unsubscribe trebuie sa urmeze si un topic.\n");
					continue;
				}
				char *topic = (char*)malloc(sizeof(char) * 50); //50 de octeti pentru topic
				strcpy(topic, token);
				
				token = strtok(null, " "); //verificam daca a mai fost pus ceva in plus ce nu trebuie
				if(token != null)
				{
					printf("Prea multe argumente la unsubscribe.\n");
					free(topic);
					continue;
				}
				strcpy(mesaj.mesaj, aux);
				mesaj.tip_mesaj = 2;
				strcpy(mesaj.id, argv[1]);
				n = send(sockfd, &mesaj, 1500, 0);
				DIE(n < 0, "send");
				printf("Unsubscribed succesfully from %s\n", topic);
				fflush(stdout);
				free(topic);
				continue;
			}
			
			printf("Nu ai introdus bine comanda de subscribe/unsubscribe.\n");
			printf("Incearca <subscribe topic store_forward> sau <unsubscribe topic>.\n");	
		}
		if (FD_ISSET(sockfd, &tmp_fds)) // date pe socket 
		{
			mesaj_server primit;
			recv(sockfd, &primit, sizeof(mesaj_server), 0);
			if(primit.tip_mesaj == 5) //daca e id_request
			{
				mesaj_server raspuns;
				memcpy(raspuns.mesaj, argv[1], strlen(argv[1]));
				raspuns.tip_mesaj = 1; //id_raspuns
				n = send(sockfd, &raspuns, sizeof(raspuns), 0);
				DIE(n < 0, "send");
			}
            if(primit.tip_mesaj == 3) //daca e close connection
            {
                break;
            }
			if(primit.tip_mesaj == 4)//daca vin date forwarded
			{
				MesajeUDP mesaj_primit;
				n = recv(sockfd, &mesaj_primit, sizeof(MesajeUDP), 0); //asteapta un mesaj forwarded
				display_mesaj(mesaj_primit);
			}
		}
	}

	close(sockfd);

	return 0;
}
