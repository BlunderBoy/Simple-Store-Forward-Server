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
#include <vector>
#include <iostream>
#include <netinet/tcp.h>

#define null NULL
#define BUFLEN 256	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	1000	// numarul maxim de clienti in asteptare
#define LUNGIME_TOPIC 51	// lungime topic


void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

class MesajServer
{
    public:
	char mesaj[255];
	int tip_mesaj; //0 pentru id_request, 1 pentru id_response, 2 pentru sub/unsub, 3 close_connection, 4 mesaj_udp
	char id[10];
};

class Topic
{
	public:
	char nume[LUNGIME_TOPIC];
	int sf;
};

class Client
{
    public:
    int conectat;
	char id[10];
	int descriptor;
	unsigned short port;
	struct in_addr ip;
	std::vector<Topic> abonamente;
};


class MesajeUDP
{
	public:
	char topic[LUNGIME_TOPIC];
	unsigned char tip_date; //0,1,2,3
	char continut[1500];
	unsigned short port;
	struct in_addr ip;
};


class Database
{
    public:
	std::vector<Client> clienti_conectati;
	std::vector<Client> posibili_clienti;
	std::vector<Client> toti_clientii;
	std::vector<std::pair<MesajeUDP, char*>> store; //mesaj + promisiuni

     void sterge(int descriptor)
     {
        for(unsigned long i = 0; i < posibili_clienti.size(); i++)
        {
            if(posibili_clienti[i].descriptor == descriptor)
            {
                posibili_clienti.erase(posibili_clienti.begin() + i);
                break;
            }
        }
     }

     void deconectare(int descriptor)
     {
        for(unsigned long i = 0; i < clienti_conectati.size(); i++)
        {
            if(clienti_conectati[i].descriptor == descriptor)
            {
                clienti_conectati.erase(clienti_conectati.begin() + i);
				toti_clientii[i].conectat = 0;
                break;
            }
        }
		for(unsigned long i = 0; i < toti_clientii.size(); i++)
        {
            if(toti_clientii[i].descriptor == descriptor)
            {
				toti_clientii[i].conectat = 0;
                break;
            }
        }
     }

    char* getId(int descriptor)
    {
        for(unsigned long i = 0; i < clienti_conectati.size(); i++)
        {
            if(clienti_conectati[i].descriptor == descriptor)
            {
                return clienti_conectati[i].id;
            }
        }
		return null;
    }

    int errorCheck(char check[])
    {
        int returnat = 1;
        for(unsigned long i = 0; i < clienti_conectati.size(); i++)
        {
            if(strcmp(check, clienti_conectati[i].id) == 0)
            {
                returnat = 0;
            }
        }
        return returnat;
    }

	void add_daca_nu_exista(Client client)
	{
		int check = 1;
		for(auto cl : toti_clientii)
		{
			if(strcmp(client.id, cl.id) == 0)
			{
				check = 0;
			}
		}
		if(check)
		{
			toti_clientii.push_back(client);
		}
	}


	Client find_client(Database database, char id[])
	{
		for(auto x : database.toti_clientii)
		{
			if(strcmp(id, x.id) == 0)
			{
				return x;
			}
		}
		return database.toti_clientii[0];
	}

	void aboneaza_client(char id[], char topic[], int sf) //daca nu e abonat clientul il adaug
	{
		int check = 1;
		for(unsigned long i = 0; i < toti_clientii.size(); i++){
			if(strcmp(id, toti_clientii[i].id) == 0)
			{
				check = 1;
				for(unsigned long j = 0; j < toti_clientii[i].abonamente.size(); j++)
				{
					if(strcmp(topic, toti_clientii[i].abonamente[j].nume) == 0)
					{
						check = 0;
						break;
					}
				}
				if(check)
				{
					Topic topic_nou;
					memcpy(topic_nou.nume, topic, strlen(topic));
					topic_nou.sf = sf;
					toti_clientii[i].abonamente.push_back(topic_nou);
				}
				break;
			}
		}
	}

	void dezaboneaza_client(char id[], char topic[]) //daca e abonat clientul il dezabonez
	{
		for(unsigned long i = 0; i < toti_clientii.size(); i++)
		{
			if(strcmp(id, toti_clientii[i].id) == 0)
			{
				for(unsigned long j = 0; j < toti_clientii[i].abonamente.size(); j++)
				{
					if(strcmp(topic, toti_clientii[i].abonamente[i].nume) == 0)
					{
						toti_clientii[i].abonamente.erase(toti_clientii[i].abonamente.begin() + j);
					}
				}
			}
		}
	}

	int exista_clientul(Client client)
	{
		int check = 0;
		for(auto x : toti_clientii)
		{
			if(strcmp(x.id, client.id) == 0)
			{
				check = 1;
				break;
			}
		}
		return check;
	}
	
	void update(Client client)
	{
		for(unsigned long i = 1; i < toti_clientii.size(); i++)
		{
			if(strcmp(toti_clientii[i].id, client.id) == 0)
			{
				toti_clientii[i].descriptor = client.descriptor;
				toti_clientii[i].conectat = 1;
				break;
			}
		}
	}
	//returneaza descriptorul pe care e conectat clientul sau 0 daca nu e conectat
	int e_conectat(Client client)
	{
		for(auto x : clienti_conectati)
		{
			if(strcmp(x.id, client.id) == 0)
			{
				return x.descriptor;
			}
		}
		return 0;
	}

	void pastreaza(MesajeUDP m, char id[])
	{
		std::pair<MesajeUDP, char*> mesaj; //mesaj + promisiuni
		mesaj.first = m;
		mesaj.second = (char*)malloc(strlen(id));
		strcpy(mesaj.second, id);
		store.push_back(mesaj);
		// std::cout<<"-------\n";
		// for(auto x :  store)
		// {
		// 	std::cout << x.first.topic << " " << x.second << "\n";
		// }
	}

	 void send_mesage(MesajeUDP mesaj)
	 {
		for(unsigned long i = 0; i < toti_clientii.size(); i++)
		{
			for(unsigned long j = 0; j < toti_clientii[i].abonamente.size(); j++)
			{
				if(strcmp(toti_clientii[i].abonamente[j].nume, mesaj.topic) == 0) //daca e abonat la topic
				{
					int connected = e_conectat(toti_clientii[i]);
					if(connected)
					{
						MesajServer avertisment;
						avertisment.tip_mesaj = 4;
						int n = send(connected, &avertisment, sizeof(avertisment), 0);//mesaj cum ca urmeaza un mesaj de la udp
						DIE(n < 0, "send");

						n = send(connected, &mesaj, sizeof(mesaj), 0);	
						DIE(n < 0, "send");
					}
					else
					{
						if(toti_clientii[i].abonamente[j].sf)
						{
							pastreaza(mesaj, toti_clientii[i].id);
						}	
					}
				}
			}		
		}
	 }

	Client cauta_client(Client client)
	{
		for(unsigned long i = 0; i < toti_clientii.size(); i++)
		{
			if(strcmp(client.id, toti_clientii[i].id) == 0)
			{
				return toti_clientii[i];
			}
		}
		return toti_clientii[0];
	}

	void cauta_si_trimite(Client client)
	{
		Client actual_client = cauta_client(client);
		for(unsigned long i = 0; i < store.size(); i++)
		{
			if(strcmp(store[i].second, client.id) == 0)
			{
				MesajServer avertisment;
				avertisment.tip_mesaj = 4;
				int n = send(client.descriptor, &avertisment, sizeof(MesajServer), 0);//mesaj cum ca urmeaza un mesaj de la udp
				DIE(n < 0, "send");

				n = send(client.descriptor, &store[i], sizeof(MesajeUDP), 0);	
				DIE(n < 0, "send");
				store.erase(store.begin() + i);
				i--;
			}
		}
	}
}; 

void send_close_connection(Database database)
{
	MesajServer closeConnection;
    closeConnection.tip_mesaj = 3;
	for(auto client : database.clienti_conectati)
	{
		if(client.conectat)
		{
			int n = send(client.descriptor, &closeConnection, sizeof(closeConnection), 0);
			DIE(n < 0, "send");
		}
	}
}

int main(int argc, char *argv[])
{
	int socket_server_tcp, newsockfd, port_server, socket_server_udp;
	int n, i, valoare_returnata;
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
	//tcp
	socket_server_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(socket_server_tcp < 0, "socket");
	//udp
	socket_server_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(socket_server_udp < 0, "socket");

	port_server = atoi(argv[1]);
	DIE(port_server == 0, "atoi");

	memset((char *) &date_server, 0, sizeof(date_server));
	date_server.sin_family = AF_INET;
	date_server.sin_port = htons(port_server);
	date_server.sin_addr.s_addr = INADDR_ANY;

	//tcp
	valoare_returnata = bind(socket_server_tcp, (struct sockaddr *) &date_server, sizeof(struct sockaddr));
	DIE(valoare_returnata < 0, "bind");

	//udp
	valoare_returnata = bind(socket_server_udp, (struct sockaddr *) &date_server, sizeof(struct sockaddr));
	DIE(valoare_returnata < 0, "bind");

	//tcp
	valoare_returnata = listen(socket_server_tcp, MAX_CLIENTS);
	DIE(valoare_returnata < 0, "listen");
	//udp e connection-less, nu are listen, datele sunt transmise la recv printr-o structura sockaddr

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni tcp) in multimea read_fds
	FD_SET(socket_server_tcp, &set_descriptori); //tcp
	FD_SET(socket_server_udp, &set_descriptori); //udp
	// adaugam si stdin la multimea de descriptori in cazul in care vrem sa dam exit
	FD_SET(0, &set_descriptori);

	//ar trebui sa avem in set : stdin stdout stderr socket_server (socket_server + 1) ...
	valoare_maxima_descriptor = socket_server_udp;

	Database database; 
	
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
						send_close_connection(database);
						break;
					}
				} else if (i == socket_server_tcp) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					lungime_sockaddr_client = sizeof(date_client);
					newsockfd = accept(socket_server_tcp, (struct sockaddr *) &date_client, &lungime_sockaddr_client);
					DIE(newsockfd < 0, "accept");
					int nodelay = 1;
					setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &set_descriptori);
					if (newsockfd > valoare_maxima_descriptor) {  //probabil e always true
						valoare_maxima_descriptor = newsockfd;
					}

					Client posibil_client;
					posibil_client.descriptor = newsockfd;
					posibil_client.port = date_client.sin_port;
					posibil_client.ip = date_client.sin_addr;
					database.posibili_clienti.push_back(posibil_client);

					MesajServer handshake;
					memset(&handshake, 0, sizeof(handshake));
					handshake.tip_mesaj = 5;
					n = send(newsockfd, &handshake, sizeof(handshake), 0);
					DIE(n < 0, "send");
				} 
				else if (i == socket_server_udp)
				{
					MesajeUDP mesaj;
					memset(&mesaj, 0, sizeof(mesaj));
					struct sockaddr_in info_sender;
					socklen_t lungime_info = sizeof(info_sender);
					recvfrom(socket_server_udp, &mesaj, sizeof(mesaj), 0, (struct sockaddr*)&info_sender, &lungime_info);
					mesaj.ip = info_sender.sin_addr;
					mesaj.port = info_sender.sin_port;
					database.send_mesage(mesaj);
				}
				else
				{
					// s-au primit date pe unul din socketii de Client,
					// asa ca serverul trebuie sa le receptioneze
					MesajServer primit;
					n = recv(i, &primit, 1800, 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
                        if(database.getId(i) != null)
                        {
						    printf("Client %s disconnected.\n", database.getId(i));
                            database.deconectare(i);
                        }
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &set_descriptori);
					} else {
						if(primit.tip_mesaj == 1) //raspuns la request
						{
							Client client;
                            if(database.errorCheck(primit.mesaj) == 0)
                            {
                                MesajServer closeConnection;
                                closeConnection.tip_mesaj = 3;
                                n = send(i, &closeConnection, sizeof(closeConnection), 0);
                                continue;
                            }
							memset(client.id, 0, 10);
							//creez un client nou daca acesta nu este deja conectat
                            for(auto posibil_client : database.posibili_clienti)
                            {
                                if(posibil_client.descriptor == i)
                                {
                                    client.descriptor = i;
									client.ip = posibil_client.ip;
									client.port = posibil_client.port;
									client.conectat = 1;
									memcpy(client.id, primit.mesaj, strlen(primit.mesaj));
                                }
                            }				
                            database.sterge(i);
                            database.clienti_conectati.push_back(client);

							if(database.exista_clientul(client))
							{
								database.update(client);
								database.cauta_si_trimite(client);
							}
							else
							{
								database.add_daca_nu_exista(client);
							}
							

							// //check SF
							printf("New client %s connected from %s:%d\n",
							client.id, inet_ntoa(client.ip), ntohs(client.port));
						}
						else //daca sunt date
						{
							char *token = (char *)malloc(LUNGIME_TOPIC);
							token = strtok(primit.mesaj, " ");
							if(strcmp(token, "subscribe") == 0)
							{
								//topic
								char* buffer_topic = (char*) malloc(LUNGIME_TOPIC);
								token = strtok(null, " ");
								strcpy(buffer_topic, token);
								buffer_topic[strlen(token)] = 0;

								//sf
								token = strtok(null, " ");
								int sf = atoi(token);
								
								//adaugare
								database.aboneaza_client(primit.id, buffer_topic, sf);
								free(buffer_topic);
							}
							if(strcmp(token, "unsubscribe") == 0)
							{
								//topic	}
								char buffer_topic[LUNGIME_TOPIC];
								token = strtok(null, " ");
								strcpy(buffer_topic, token);
								buffer_topic[strlen(token)] = 0;

								database.dezaboneaza_client(primit.id, buffer_topic);
								database.dezaboneaza_client(primit.id, buffer_topic);
							}
						}
						
						fflush(stdout);
					}
				}
			}
		}
	}
	close(socket_server_tcp);
	close(socket_server_udp);
	return 0;
}
