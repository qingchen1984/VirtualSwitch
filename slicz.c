/**
 * Tomasz Potanski
 * 321 150
 * slicz
 * Sieci komputerowe, zadanie 3
 */

#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include "err.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

//define section
#define RECEIVED_COUNTER            	0
#define LIMIT_ILOSCI_ZDARZEN     		136
#define LIMIT_AKTYWNYCH_PORTOW       	100
#define ROZMIAR_BUFFORA_TCP        		1600
#define LIMIT_SIECI_VLAN      			4095
#define LIMIT_ILOSCI_POLACZEN     		20
#define ERROR_COUNTER             		2
#define ROZMIAR_BUFFORA   				64000
#define LIMIT_ADRESOW_MAC       		4096
#define SENT_COUNTER             		1

//globals
int ilosc_aktywnych_portow = 0;
char tcp_buffor_size_table[LIMIT_ILOSCI_POLACZEN + 1][ROZMIAR_BUFFORA_TCP];
int counters_table[LIMIT_AKTYWNYCH_PORTOW][3];
int length_of_sum_received[LIMIT_ILOSCI_POLACZEN + 1];
int tablica_gniazd_tcp[LIMIT_ILOSCI_POLACZEN + 1];
int tablica_gniazd_udp[LIMIT_AKTYWNYCH_PORTOW];
char *tablica_konfiguracji[LIMIT_AKTYWNYCH_PORTOW][LIMIT_SIECI_VLAN + 8];
int ilosc_polaczen_tcp = 0;
char tablica_mac[LIMIT_ADRESOW_MAC][3][20];
int ilosc_adreos_w_tablicy_mac = 0;
char temporary_vlan[11];
int priorytety[LIMIT_ILOSCI_ZDARZEN + 1];
unsigned long tablica_adresow_ip_udp[LIMIT_AKTYWNYCH_PORTOW];
short tablica_vlanow[LIMIT_AKTYWNYCH_PORTOW][LIMIT_SIECI_VLAN + 5];
unsigned short tablica_portow_udp[LIMIT_AKTYWNYCH_PORTOW];
const char *TCP_port = "42420";
int gniazdo_tcp, wynik_odblokowania;
int epoll_file_descriptor, nbuffor, nsock;
char buf[ROZMIAR_BUFFORA_TCP];
struct epoll_event event;
struct epoll_event *events;

//function declaration
int sprawdz_tagowanie_vlanow(int index);
int stworz_port_udp(int index, int e_file_descriptor, int nbuffer);
int otworz_gniazda(int epoll_file_descriptor);
int daj_minimalny_numer_portu(int index);
void inicjalizacja();
void wyslij_ramke(char *buf, int rb, int m_i, int nsock, int ind, int t);
int czytaj_z_gniazda_tcp(int numer_buffora);
int sprawdz_address_mac(char *src, char *tmpv);
int parsuj_ramke(char *buffor, int firstly_added, int index, int *tagged);
int odblokuj_socket(int s_file_descriptor);
int zdarzenie_na_tcp(int sfd);
void gra_wstepna();
int zdarzenie_na_udp(int sfd);
int ustaw_gniazdo_tcp(const char *port);
void delete_port_udp(int index);
int skoryguj_andress_port(struct sockaddr_in *from_addr, int index);
void wyslij_datagram(char *prievous_buffor, int read_bytes, int nvlan,
		int nsock, int index, int tagowanie);
int parsuj_argumenty_programu(char *c);
int daj_numer_portu(char *port);
void never_ending_story();
void odczytaj_argumenty(int argc, char *argv[]);

//--------------
int main(int argc, char *argv[]) {

	inicjalizacja();
	odczytaj_argumenty(argc, argv);
	gra_wstepna(); // ;)

	never_ending_story();

	free(events);
	close(gniazdo_tcp);

	return 0;
}
//---------------

void gra_wstepna() {
	if ((gniazdo_tcp = ustaw_gniazdo_tcp(TCP_port)) == -1) {
		syserr("error in setting up tcp socket! Please, don't panic!");
	}

	if ((wynik_odblokowania = odblokuj_socket(gniazdo_tcp)) == -1) {
		syserr("error: odblokowanie socketu tcp");
	}
	if ((wynik_odblokowania = listen(gniazdo_tcp, SOMAXCONN)) == -1) {
		syserr("error while listen() tcp call! Don't panic!");
	}

	epoll_file_descriptor = epoll_create1(0);
	if (epoll_file_descriptor == -1) {
		syserr("epoll_create1");
	}

	event.events = EPOLLIN;
	event.data.fd = gniazdo_tcp;

	if ((wynik_odblokowania = epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD,
			gniazdo_tcp, &event)) == -1) {
		syserr("epoll_ctl");
	}

	events = calloc(LIMIT_ILOSCI_ZDARZEN, sizeof event);
	if (otworz_gniazda(epoll_file_descriptor)) {
		fprintf(stderr, "udp socket creating error\n");
		exit(1);
	}
}

void never_ending_story() {
	while (1) {
		int n, i;
		n = epoll_wait(epoll_file_descriptor, events, LIMIT_ILOSCI_ZDARZEN, -1);
		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
					|| (!(events[i].events & EPOLLIN))) {

				fprintf(stderr, "epoll error\n");
				close(events[i].data.fd);
				continue;
			} else if (gniazdo_tcp == events[i].data.fd) {

				while (1) {
					struct sockaddr in_address_scokaddr_instance;
					socklen_t in_length_socklen_t_instance;
					int in_file_descriptor;
					char host_buffer[NI_MAXHOST], server_buffer[NI_MAXSERV];

					in_length_socklen_t_instance =
							sizeof in_address_scokaddr_instance;

					if ((in_file_descriptor = accept(gniazdo_tcp,
							&in_address_scokaddr_instance,
							&in_length_socklen_t_instance)) == -1) {
						if (!(errno == EAGAIN) && !(errno == EWOULDBLOCK)) {
							perror("error in accept accept");
							break;
						} else {
							break;
						}
					}

					wynik_odblokowania = getnameinfo(
							&in_address_scokaddr_instance,
							in_length_socklen_t_instance, host_buffer,
							sizeof host_buffer, server_buffer,
							sizeof server_buffer,
							NI_NUMERICHOST | NI_NUMERICSERV);

					if ((wynik_odblokowania = odblokuj_socket(
							in_file_descriptor)) == -1) {
						perror(
								"error in epoll_ctl during dobblokuj_socket() function call!");
						abort();
					}

					event.events = EPOLLIN;
					event.data.fd = in_file_descriptor;
					if ((wynik_odblokowania = epoll_ctl(epoll_file_descriptor,
							EPOLL_CTL_ADD, in_file_descriptor, &event)) == -1) {
						perror("error in epoll_ctl() function call!");
						abort();
					}

					int j = 1;
					while (j <= LIMIT_ILOSCI_POLACZEN) {
						if (tablica_gniazd_tcp[j] == 0) {
							tablica_gniazd_tcp[j] = in_file_descriptor;
							break;
						}
						j++;
					}

					if (write(in_file_descriptor, "SLICZ\n", 6) < 0) {
						fprintf(stderr,
								"Error in writing handshake to client tcp socket: SLICZ\n");
					}
				}
			} else if ((nbuffor = zdarzenie_na_tcp(events[i].data.fd))) {

				int done = 0;
				int res = czytaj_z_gniazda_tcp(nbuffor);
				if (res == -2 || res == -1) {
					done = 1;
					if (write(tablica_gniazd_tcp[nbuffor], "reading error\n", 9)
							< 0 && res == -1) {
						fprintf(stderr, "writing error\n");
					}
				} else if (res == 1) {
					int minimalny_port = LIMIT_AKTYWNYCH_PORTOW + 1;
					int length2;
					char send[ROZMIAR_BUFFORA_TCP];
					char *current_buffor_size = tcp_buffor_size_table[nbuffor];

					if (strstr(current_buffor_size, "getconfig\n")
							== current_buffor_size) {
						if (write(tablica_gniazd_tcp[nbuffor], "OK\n", 3) < 0) {
							fprintf(stderr,
									"write to socket client error: OK\n");
						}

						while ((minimalny_port = daj_minimalny_numer_portu(
								minimalny_port)) != -1) {
							if (tablica_portow_udp[minimalny_port] != 0) {
								struct sockaddr_in tp;
								tp.sin_addr.s_addr =
										tablica_adresow_ip_udp[minimalny_port];
								length2 = sprintf(send, "%s/%s:%d/",
										tablica_konfiguracji[minimalny_port][0],
										inet_ntoa(tp.sin_addr),
										tablica_portow_udp[minimalny_port]);
							} else {
								length2 =
										sprintf(send, "%s//",
												tablica_konfiguracji[minimalny_port][0]);
							}

							int ind = 3;
							while (tablica_konfiguracji[minimalny_port][ind]
									!= NULL ) {
								if (ind != 3) {
									length2 += sprintf(length2 + send, ",");
								}
								length2 +=
										sprintf(send + length2, "%s",
												tablica_konfiguracji[minimalny_port][ind]);
								if (tablica_vlanow[minimalny_port][atoi(
										tablica_konfiguracji[minimalny_port][ind])]
										== 2) {
									length2 += sprintf(send + length2, "t");
								}
								ind++;
							}
							length2 += sprintf(send + length2, "\n");

							if (write(tablica_gniazd_tcp[nbuffor], send,
									length2) < 0) {
								fprintf(stderr,
										"write to socket client error\n");
							}
						}

						if (write(tablica_gniazd_tcp[nbuffor], "END\n", 4)
								< 0) {
							fprintf(stderr,
									"write to socket client error: END\n");
						}
					} else if (strstr(current_buffor_size, "counters\n")
							== current_buffor_size) {
						if (write(tablica_gniazd_tcp[nbuffor], "OK\n", 3) < 0) {
							fprintf(stderr,
									"write to client socket error: OK\n");
						}

						while ((minimalny_port = daj_minimalny_numer_portu(
								minimalny_port)) != -1) {
							length2 = sprintf(send,
									"%s recvd:%d sent:%d errs:%d\n",
									tablica_konfiguracji[minimalny_port][0],
									counters_table[minimalny_port][0],
									counters_table[minimalny_port][1],
									counters_table[minimalny_port][2]);
							if (write(tablica_gniazd_tcp[nbuffor], send,
									length2) < 0) {
								fprintf(stderr,
										"write to socket client error\n");
							}
						}

						if (write(tablica_gniazd_tcp[nbuffor], "END\n", 4)
								< 0) {
							fprintf(stderr,
									"write to socket client error: END\n");
						}
					} else if (strstr(current_buffor_size, "setconfig ")
							== current_buffor_size) {
						char *port_number, *vlan;
						struct sockaddr_in temporary_sockaddr_in_instance;
						char *ipc = "", *portc = "";
						int error = 0, index = 0;
						current_buffor_size[strlen(current_buffor_size) - 1] =
								'\0';
						strsep(&current_buffor_size, " ");
						char *a = malloc(
								(strlen(current_buffor_size) + 1)
										* sizeof(char));
						char *tofree = a;
						strcpy(a, current_buffor_size);

						if ((port_number = strsep(&a, "/")) == NULL || (portc =
								strsep(&a, "/")) == NULL
								|| (ipc = strsep(&portc, ":")) == NULL
								|| (portc == NULL && strlen(ipc) > 0)) {
							error = -1;
						}

						if (error == -1) {
							if (write(tablica_gniazd_tcp[nbuffor],
									"ERR invalid command\n", 20) < 0) {
								fprintf(stderr,
										"write to client socket error\n");
							}
							free(tofree);
							continue;
						}

						index = daj_numer_portu(port_number);

						for (i = 3; (vlan = strsep(&a, ",")) != NULL ; i++) {
							if (strcmp(vlan, "") == 0) {
								i++;
								break;
							}
							tablica_konfiguracji[(
									(i == 3 && index == -1) ?
											ilosc_aktywnych_portow : index)][i] =
									vlan;
						}

						int check = 0;
						if (i != 3) {
							if (tablica_konfiguracji[index][0] != NULL) {
								free(tablica_konfiguracji[index][0]);
							}
							tablica_konfiguracji[index][0] = port_number;
							tablica_konfiguracji[index][1] = ipc;
							tablica_konfiguracji[index][2] = portc;
							tablica_konfiguracji[index][i] = NULL;
							if (tablica_konfiguracji[index][2] == NULL) {
								tablica_adresow_ip_udp[index] = 0;
								tablica_portow_udp[index] = 0;
							} else {
								inet_aton(tablica_konfiguracji[index][1],
										&temporary_sockaddr_in_instance.sin_addr);
								tablica_adresow_ip_udp[index] =
										temporary_sockaddr_in_instance.sin_addr.s_addr;
								tablica_portow_udp[index] =
										htons(
												(unsigned short) atoi(
														tablica_konfiguracji[index][2]));

							}

							if (index == ilosc_aktywnych_portow) {
								ilosc_aktywnych_portow++;
								if (stworz_port_udp(index,
										epoll_file_descriptor, nbuffor)) {
									check = 1;
								}
							} else if (sprawdz_tagowanie_vlanow(index) == -1) {
								delete_port_udp(index);
								if (write(tablica_gniazd_tcp[nbuffor],
										"ERR error while opening udp port\n",
										33) < 0 && nbuffor != -1) {
									fprintf(stderr, "err udp port open\n");
								}
								check = 1;
							}
						}

						if (check) {
							length_of_sum_received[nbuffor] = 0;
							continue;
						}

						if (i == 3) {
							if (index == -1) {
								if (write(tablica_gniazd_tcp[nbuffor],
										"ERR invalid command\n", 20) < 0)
									fprintf(stderr,
											"write to client socket error\n");
								free(tofree);
							} else if (index != -1) {
								free(tofree);
								delete_port_udp(index);
							}
						}

						if ((i != 3 || index != -1)) {
							fprintf(stderr, "write to client socket error\n");
						}

						if (write(tablica_gniazd_tcp[nbuffor], "OK\n", 3) < 0) {
							fprintf(stderr,
									"write to client socket error: OK\n");
						}

					} else if (write(tablica_gniazd_tcp[nbuffor],
							"ERR invalid command\n", 20) < 0)
						fprintf(stderr, "writing to client socket error\n");

					length_of_sum_received[nbuffor] = 0;
				}

				if (done) {
					tablica_gniazd_tcp[nbuffor] = 0;
					length_of_sum_received[nbuffor] = 0;
					close(events[i].data.fd);
				}
			} else if ((nsock = zdarzenie_na_udp(events[i].data.fd)) != -1) {


				socklen_t length_socklen_t_instance;
				int fres, j, index = -1;
				int mac_index;
				int t = 0;
				ssize_t read_bytes;
				struct sockaddr_in from_address_sockaddr_in_instance;
				length_socklen_t_instance =
						(socklen_t) sizeof(from_address_sockaddr_in_instance);

				if ((read_bytes = recvfrom(events[i].data.fd, buf,
						(size_t)(sizeof(buf) - 1), 0,
						(struct sockaddr *) &from_address_sockaddr_in_instance,
						&length_socklen_t_instance)) < 0
						|| length_socklen_t_instance < 0) {
					delete_port_udp(nsock);
					fprintf(stderr, "recvfrom() function call error\n");
					continue;
				}

				if ((fres = skoryguj_andress_port(
						&from_address_sockaddr_in_instance, nsock)) < 0) {
					fprintf(stderr, "packet from wrong address\n");
					continue;
				}

				if ((mac_index = parsuj_ramke(buf, fres, nsock, &t)) < 0) {
					fprintf(stderr, "some error with packet parsing\n");
					continue;
				}

				counters_table[nsock][RECEIVED_COUNTER]++;

				for (j = 0; j < ilosc_aktywnych_portow; j++)
					if (strcmp(tablica_mac[mac_index][2],
							tablica_konfiguracji[j][0]) == 0) {
						index = j;
						break;
					}

				if (nsock != index) {
					wyslij_ramke(buf, read_bytes, mac_index, nsock, index, t);
				} else {
					fprintf(stderr, "nsock == index\n");
				}
			} else {
				exit(1);
			}
		}
	}
}

int daj_numer_portu(char *port) {
	int i;
	for (i = 0; i < ilosc_aktywnych_portow; i++) {
		if ((strcmp(port, tablica_konfiguracji[i][0]) == 0)
				|| (port == NULL && tablica_konfiguracji[i][0] == NULL )) {
			return i;
		}
	}

	return -1;
}

void inicjalizacja() {
	int i, j;

	for (i = 0; i <= LIMIT_ILOSCI_POLACZEN; i++) {
		for (j = 0; j < ROZMIAR_BUFFORA_TCP; j++) {
			tcp_buffor_size_table[i][j] = '\0';
		}
		tablica_gniazd_tcp[i] = 0;
		length_of_sum_received[i] = 0;
	}

	for (i = 0; i < LIMIT_ADRESOW_MAC; i++) {
		tablica_mac[i][0][0] = '\0';
		tablica_mac[i][1][0] = '\0';
		tablica_mac[i][2][0] = '\0';
	}

	for (i = 0; i < LIMIT_AKTYWNYCH_PORTOW; i++) {
		for (j = 0; j < 3; j++) {
			counters_table[i][j] = 0;
		}
		tablica_gniazd_udp[i] = 0;
		tablica_portow_udp[i] = 0;
		tablica_adresow_ip_udp[i] = 0;

		for (j = 0; j - 2 <= LIMIT_SIECI_VLAN; j++) {
			tablica_vlanow[i][j] = 0;
			tablica_konfiguracji[i][j] = NULL;
		}
	}
}

int czytaj_z_gniazda_tcp(int numer_buffora) {
	ssize_t length_ssize_t_instance = 0;
	while (1) {
		length_ssize_t_instance = read(tablica_gniazd_tcp[numer_buffora],
				(tcp_buffor_size_table[numer_buffora]
						+ length_of_sum_received[numer_buffora]), 1);
		length_of_sum_received[numer_buffora] += length_ssize_t_instance;
		if (length_ssize_t_instance == 0) {
			return -2;
		}
		if (length_ssize_t_instance == -1) {
			if (errno != EAGAIN) {
				fprintf(stderr, "error while reading from tcp socket\n");
				return -1;
			}
			if (tcp_buffor_size_table[numer_buffora][length_of_sum_received[numer_buffora]
					- 1] == '\n') {
				tcp_buffor_size_table[numer_buffora][length_of_sum_received[numer_buffora]] =
						'\0';
				return 1;
			}
			return 0;
		}

		if (tcp_buffor_size_table[numer_buffora][length_of_sum_received[numer_buffora]
				- 1] == '\n' && length_ssize_t_instance == 1) {
			tcp_buffor_size_table[numer_buffora][length_of_sum_received[numer_buffora]] =
					'\0';
			return 1;
		}
		if (length_of_sum_received[numer_buffora] == ROZMIAR_BUFFORA_TCP) {
			return -1;
		}
	}
}

int daj_minimalny_numer_portu(int index) {
	int i, maksymalna_dlugosc, minimalna_dlugosc, dlugosc_konfiguracji, result =
			-1;
	const char* maksimum = "123456789";
	const char* minimum = "0";

	if (index != LIMIT_AKTYWNYCH_PORTOW + 1) {
		minimum = tablica_konfiguracji[index][0];
	}

	for (i = 0; i < ilosc_aktywnych_portow; i++) {
		maksymalna_dlugosc = strlen(maksimum);
		minimalna_dlugosc = strlen(minimum);
		dlugosc_konfiguracji = strlen(tablica_konfiguracji[i][0]);
		if (((dlugosc_konfiguracji == maksymalna_dlugosc)
				&& (strcmp(maksimum, tablica_konfiguracji[i][0]) > 0))
				|| (dlugosc_konfiguracji < maksymalna_dlugosc)

						&&

						((dlugosc_konfiguracji > minimalna_dlugosc)
								|| (dlugosc_konfiguracji == minimalna_dlugosc
										&& strcmp(minimum,
												tablica_konfiguracji[i][0]) < 0))

						) {
			result = i;
			maksimum = tablica_konfiguracji[i][0];

		}
	}
	return result;
}

void odczytaj_argumenty(int argc, char *argv[]) {
	int command;
	while ((command = getopt(argc, argv, ":c:p:")) != -1) {
		if (command == 'c') {
			TCP_port = optarg;
		} else if (command == '?') {
			fprintf(stderr, "Nieznana opcja '-%c'.\n", optopt);
			exit(1);
		} else if (command == 'p') {
			if (parsuj_argumenty_programu(optarg) != 0) {
				fprintf(stderr, "Nieznana konfiguracja\n");
				exit(1);
			}
		} else if (command == ':') {
			fprintf(stderr, "Ta opcja (-%c) wymaga argumentu.\n", optopt);
			exit(1);
		}
	}
	if (optind != argc) {
		fprintf(stderr, "Uzycie: ./slicz -c <port_tcp> -p <konfiguracje>");
		exit(1);
	}
}
int parsuj_argumenty_programu(char *c) {
	char *config = malloc(sizeof(char) * (strlen(c) + 1));
	char *tofree = config;
	int i;
	struct sockaddr_in temporary_sockaddr_in_instance;

	strcpy(config, c);

	tablica_konfiguracji[ilosc_aktywnych_portow][0] = strsep(&config, "/");
	tablica_konfiguracji[ilosc_aktywnych_portow][2] = strsep(&config, "/");
	if (tablica_konfiguracji[ilosc_aktywnych_portow][0] == NULL
			|| tablica_konfiguracji[ilosc_aktywnych_portow][2] == NULL) {
		free(tofree);
		return -1;
	}

	if ((tablica_konfiguracji[ilosc_aktywnych_portow][1] = strsep(
			&tablica_konfiguracji[ilosc_aktywnych_portow][2], ":")) == NULL) {
		free(tofree);
		return -1;
	}
	if (tablica_konfiguracji[ilosc_aktywnych_portow][2] == NULL
			&& strlen(tablica_konfiguracji[ilosc_aktywnych_portow][1]) > 0) {
		free(tofree);
		return -1;
	}
	if (tablica_konfiguracji[ilosc_aktywnych_portow][2] != NULL) {
		inet_aton(tablica_konfiguracji[ilosc_aktywnych_portow][1],
				&temporary_sockaddr_in_instance.sin_addr);
		tablica_portow_udp[ilosc_aktywnych_portow] = htons(
				(unsigned short) atoi(
						tablica_konfiguracji[ilosc_aktywnych_portow][2]));
		tablica_adresow_ip_udp[ilosc_aktywnych_portow] =
				temporary_sockaddr_in_instance.sin_addr.s_addr;

	}

	for (i = 3;
			(tablica_konfiguracji[ilosc_aktywnych_portow][i] = strsep(&config,
					",")) != NULL ; i++) {
	}

	if (i == 3) {
		free(tofree);
		return -1;
	}

	ilosc_aktywnych_portow += 1;
	return 0;
}

int odblokuj_socket(int s_file_descriptor) {
	int flags, s;
	flags = fcntl(s_file_descriptor, F_GETFL, 0);
	if (flags == -1) {
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl(s_file_descriptor, F_SETFL, flags);
	if (s == -1) {
		return -1;
	}

	return 0;
}

int zdarzenie_na_tcp(int sfd) {
	int i;
	for (i = 1; i <= LIMIT_ILOSCI_POLACZEN; i++) {
		if (tablica_gniazd_tcp[i] == sfd) {
			return i;
		}
	}
	return 0;
}

int zdarzenie_na_udp(int sfd) {
	int i;
	for (i = 0; i < LIMIT_AKTYWNYCH_PORTOW; i++) {
		if (tablica_gniazd_udp[i] == sfd) {
			return i;
		}
	}
	return -1;
}

void delete_port_udp(int index) {
	int i, j;

	if (tablica_konfiguracji[index][0] != NULL) {
		free(tablica_konfiguracji[index][0]);
	}

	tablica_konfiguracji[index][0] = NULL;
	tablica_adresow_ip_udp[index] = 0;
	tablica_portow_udp[index] = 0;

	priorytety[index] = 0;

	close(tablica_gniazd_udp[index]);
	for (i = index; tablica_konfiguracji[i][0] != NULL ; i++) {
		for (j = 0; tablica_konfiguracji[i + 1][j] != NULL ; j++) {
			tablica_konfiguracji[i][j] = tablica_konfiguracji[i + 1][j];
			tablica_gniazd_udp[i] = tablica_gniazd_udp[i + 1];
		}
		tablica_gniazd_udp[i] = 0;
		tablica_konfiguracji[i][j] = NULL;
	}
	ilosc_aktywnych_portow -= 1;
}

int sprawdz_tagowanie_vlanow(int index) {
	int i, nvlan, length, tag, count = 0;
	for (i = 0; i - 1 <= LIMIT_SIECI_VLAN; i++) {
		tablica_vlanow[index][i] = 0;
	}

	for (i = 3; tablica_konfiguracji[index][i] != NULL ; i++) {
		length = strlen(tablica_konfiguracji[index][i]);
		count++;
		tag = 1;
		if (tablica_konfiguracji[index][i][length - 1] == 't') {
			tag = 2;
			count--;
			tablica_konfiguracji[index][i][length - 1] = '\0';
		}
		if (count > 1) {
			return -1;
		}

		nvlan = atoi(tablica_konfiguracji[index][i]);
		if (nvlan == 0) {
			return -1;
		}
		if (tablica_vlanow[index][nvlan]
				!= 0|| nvlan < 1 || nvlan > LIMIT_SIECI_VLAN) {return
-			1;
		}

		if ((tablica_vlanow[index][nvlan] = tag) == 1) {
			tablica_vlanow[index][LIMIT_SIECI_VLAN + 1] = nvlan;
		}
	}

	return 0;
}

int stworz_port_udp(int index, int e_file_descriptor, int nbuffer) {
	int sendbuf = ROZMIAR_BUFFORA, sock, s;
	struct sockaddr_in client_address_sockaddr_in_instance;
	struct epoll_event slicz_event_epoll;

	client_address_sockaddr_in_instance.sin_addr.s_addr = htonl(INADDR_ANY );
	client_address_sockaddr_in_instance.sin_port = htons(
			atoi(tablica_konfiguracji[index][0]));
	client_address_sockaddr_in_instance.sin_family = AF_INET;

	tablica_gniazd_udp[index] = (sock = socket(AF_INET, SOCK_DGRAM, 0));
	priorytety[index] = 0;

	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
	if (sprawdz_tagowanie_vlanow(index) == -1 || sock < 0) {
		delete_port_udp(index);
		if (write(tablica_gniazd_tcp[nbuffer], "ERR while opening udp port\n",
				27) < 0 && nbuffer != -1) {
			fprintf(stderr, "error while opening udp port\n");
		}
		return 1;
	}

	if ((s = odblokuj_socket(sock)) < 0) {
		delete_port_udp(index);
		if (nbuffer != -1
				&& write(tablica_gniazd_tcp[nbuffer],
						"ERR while opening udp port\n", 27) < 0)
			fprintf(stderr, "error while opening udp port\n");
		return 1;
	}

	if (bind(sock, (struct sockaddr *) &client_address_sockaddr_in_instance,
			(socklen_t) sizeof(client_address_sockaddr_in_instance)) < 0) {
		delete_port_udp(index);
		if (nbuffer != -1
				&& write(tablica_gniazd_tcp[nbuffer],
						"ERR while opening udp port\n", 27) < 0) {
			fprintf(stderr, "error while opening udp port\n");
		}
		return 1;
	}

	slicz_event_epoll.data.fd = sock;
	slicz_event_epoll.events = EPOLLIN;
	if ((s = epoll_ctl(e_file_descriptor, EPOLL_CTL_ADD, sock,
			&slicz_event_epoll)) < 0) {
		delete_port_udp(index);
		if (nbuffer != -1
				&& write(tablica_gniazd_tcp[nbuffer],
						"ERR while opening udp port\n", 27) < 0) {
			fprintf(stderr, "error while opening udp port\n");
		}
		return 1;
	}

	return 0;
}

int ustaw_gniazdo_tcp(const char *port) {
	struct addrinfo *result, *addrinfo_instance_tmp;
	int s, integer_file_descriptor;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_flags = 0;
	hints.ai_socktype = SOCK_STREAM;

	if ((s = getaddrinfo(NULL, port, &hints, &result)) != 0) {
		return -1;
	}

	addrinfo_instance_tmp = result;
	if ((integer_file_descriptor = socket(addrinfo_instance_tmp->ai_family,
			addrinfo_instance_tmp->ai_socktype,
			addrinfo_instance_tmp->ai_protocol)) == -1) {
		return -1;
	}

	if ((s = bind(integer_file_descriptor, addrinfo_instance_tmp->ai_addr,
			addrinfo_instance_tmp->ai_addrlen)) < 0) {
		return -1;
	}

	freeaddrinfo(result);
	return integer_file_descriptor;
}

int otworz_gniazda(int epoll_file_descriptor) {
	int i;
	for (i = 0; i < ilosc_aktywnych_portow; i++) {
		if (stworz_port_udp(i, epoll_file_descriptor, -1)) {
			return 1;
		}
	}
	return 0;
}

int skoryguj_andress_port(struct sockaddr_in *from_addr, int index) {
	if (tablica_portow_udp[index] != 0
			&& tablica_portow_udp[index] == from_addr->sin_port
			&& tablica_adresow_ip_udp[index] == from_addr->sin_addr.s_addr) {
		return 0;
	}

	if (tablica_portow_udp[index] == 0) {
		tablica_portow_udp[index] = from_addr->sin_port;
		tablica_adresow_ip_udp[index] = from_addr->sin_addr.s_addr;
		return 1;
	}

	return -1;
}

int sprawdz_mac(char *src, char *tmpv) {
	int i;
	for (i = 0; i < LIMIT_ADRESOW_MAC; i++) {
		if (atoi(tablica_mac[i][1]) == atoi(tmpv)
				&& strcmp(tablica_mac[i][0], src) == 0) {
			return 0;
		}
	}
	return -1;
}

int parsuj_ramke(char *buffor, int firstly_added, int index, int *tagged) {
	char dst[7];
	char src[7];
	int i, counter = 0, nvlan = 0;
	*tagged = 0;

	strncpy(dst, buffor, 6);
	strncpy(src, buffor + 6, 6);
	dst[6] = src[6] = '\0';

	if (buffor[12] == -127 && buffor[13] == 0) {
		nvlan = (256 * (buffor[14] & 1)) + (512 * (buffor[14] & 2))
				+ (1024 * (buffor[14] & 4)) + (2048 * (buffor[14] & 8))
				+ (int) buffor[15];
		buffor[14] = buffor[14] & 15;
		*tagged = 1;
	} else if (tablica_vlanow[index][LIMIT_SIECI_VLAN + 1] != 0) {
		nvlan = tablica_vlanow[index][LIMIT_SIECI_VLAN + 1];
	} else {
		fprintf(stderr, "invalid tags\n");
		return -1;
	}
	snprintf(temporary_vlan, 10, "%d", nvlan);

	if (sprawdz_mac(src, temporary_vlan) < 0) {
		strncpy(tablica_mac[ilosc_adreos_w_tablicy_mac][0], src, 6);
		snprintf(tablica_mac[ilosc_adreos_w_tablicy_mac][1], 10, "%d", nvlan);
		strcpy(tablica_mac[ilosc_adreos_w_tablicy_mac][2],
				tablica_konfiguracji[index][0]);
		tablica_mac[ilosc_adreos_w_tablicy_mac][0][6] = '\0';

		ilosc_adreos_w_tablicy_mac = (ilosc_adreos_w_tablicy_mac + 1)
				% LIMIT_ADRESOW_MAC;
	}

	for (i = 0; i < 6; i++) {
		if (dst[i] == -1) {
			counter++;
		}
	}

	if (counter == 6) {
		return LIMIT_ADRESOW_MAC + 1;
	}

	for (i = 0; i < LIMIT_ADRESOW_MAC; i++) {
		if (strcmp(dst, tablica_mac[i][0]) == 0
				&& nvlan == atoi(tablica_mac[i][1])) {
			return i;
		}
	}

	fprintf(stderr, "mac addres could not be found\n");
	return -1;
}

void wyslij_datagram(char *prievous_buffor, int read_bytes, int nvlan,
		int nsock, int index, int tagowanie) {
	int write_bytes, length;
	int i;
	char buffor[ROZMIAR_BUFFORA_TCP];
	struct sockaddr_in slicz_address;

	for (i = 0; i < read_bytes; i++) {
		buffor[i] = prievous_buffor[i];
	}

	if (tagowanie == 0) {
		if (tablica_vlanow[index][nvlan] == 2) {
			for (i = read_bytes; i >= 12; i--) {
				buffor[i + 4] = buffor[i];
			}
			read_bytes += 4;
			buffor[12] = (char) -127;
			buffor[13] = (char) 0;
			buffor[14] = buffor[15] = 0;
			buffor[14] = ((((buffor[14] | (nvlan & 256)) | (nvlan & 512))
					| (nvlan & 1024)) | (nvlan & 2048));
			buffor[15] =
					((((((((buffor[15] | (nvlan & 1)) | (nvlan & 2))
							| (nvlan & 4)) | (nvlan & 8)) | (nvlan & 16))
							| (nvlan & 32)) | (nvlan & 64)) | (nvlan & 128));
			buffor[14] = buffor[14] & 15;
		}
	} else if (tablica_vlanow[index][nvlan] == 1) {
		for (i = 12; i <= read_bytes; i++) {
			buffor[i] = buffor[i + 4];
		}
		read_bytes -= 4;
	}

	if (tablica_vlanow[nsock][nvlan] == 0
			|| tablica_vlanow[index][nvlan] == 0) {
		fprintf(stderr,
				"konfiguracja nie jest obslugiwana: tablica_vlanow - %d\n",
				nvlan);
		return;
	}

	slicz_address.sin_port = tablica_portow_udp[index];
	slicz_address.sin_addr.s_addr = tablica_adresow_ip_udp[index];
	slicz_address.sin_family = AF_INET;

	length = (socklen_t) sizeof(slicz_address);
	write_bytes = sendto(tablica_gniazd_udp[index], buffor, read_bytes, 0,
			(struct sockaddr *) &slicz_address, length);
	if (write_bytes < 0) {
		counters_table[index][ERROR_COUNTER]++;
	} else {
		counters_table[index][SENT_COUNTER]++;
	}
}

void wyslij_ramke(char *buf, int rb, int m_i, int nsock, int ind, int t) {
	int i, j;
	if (m_i == LIMIT_ADRESOW_MAC + 1) {
		for (i = 0; i < LIMIT_ADRESOW_MAC; i++) {
			if (atoi(temporary_vlan) == atoi(tablica_mac[i][1])
					&& atoi(tablica_mac[i][2])
							!= atoi(tablica_konfiguracji[nsock][0])) {
				for (j = 0; j < ilosc_aktywnych_portow; j++) {
					if (atoi(tablica_mac[i][2])
							== atoi(tablica_konfiguracji[j][0])) {
						ind = j;
						break;
					}
				}
				if (ind == -1) {
					break;
				}
				wyslij_datagram(buf, rb, atoi(temporary_vlan), nsock, ind, t);
			}
		}
	} else if (ind != -1) {
		wyslij_datagram(buf, rb, atoi(temporary_vlan), nsock, ind, t);
	} else {
		fprintf(stderr, "mac unknown\n");
	}
}

