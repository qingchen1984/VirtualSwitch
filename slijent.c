/**
 * Tomasz Potanski
 * 321 150
 * zadanie nr 3 z sieci komputerowych
 * slijent.c
 */

#include <stdio.h>
#include <errno.h>
#include "err.h"
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MTU 1522
#define LIMIT_ZDARZEN 136


//globals section
int file_descriptor, error_int, gniazdo, epoll_file_descriptor, epol_ctl_result,
		command;
struct addrinfo addr_hints_addrinfo_structure;
struct addrinfo *addr_result_addrinfo_structure;
struct sockaddr_in switch_address_sockaddr_in_structure;
struct sockaddr_in from_address_sockaddr_in_structure;
struct ifreq ifreq_struct_instance;
struct epoll_event *zdarzenia;
struct epoll_event zdarzenie_switcha;
struct epoll_event zdarzenie_interfacu;
char *numer_portu;
char *adres_hosta;
const char *nazwa_interfacu = "siktap";

/**
 * Petla switcha
 */
void never_ending_story();
/**
 * Ustawianie wartosci
 */
void init_configuration();
/**
 * Odczytywanie parametrow wywolania programu, zapewnienie odpowiedniej ilosci argumentow
 */
void parse_arguments(int argc, char* argv[]);

int main(int argc, char* argv[]) {

	parse_arguments(argc, argv);
	init_configuration();
	never_ending_story();

	//cleaning, closing
	free(zdarzenia);
	if (close(gniazdo) == -1) {
		syserr("error while closing socket!");
	}

	return 0;
}

void never_ending_story() {

	while (1) {
		int n, i;
		ssize_t write_bytes, read_bytes;
		socklen_t length;
		char buffor[MTU];

		n = epoll_wait(epoll_file_descriptor, zdarzenia, LIMIT_ZDARZEN, -1);
		for (i = 0; i < n; i++) {
			if (file_descriptor == zdarzenia[i].data.fd) {
				if ((read_bytes = read(zdarzenia[i].data.fd, buffor,
						sizeof(buffor))) < 0) {
					fprintf(stderr, "error in read() function call!\n");
					exit(1);
				}
				buffor[read_bytes + 1] = '\0';
				printf("Ramka z  interfacu (%d bajtow): <%s>\n",
						(int) read_bytes, buffor);

				length =
						(socklen_t) sizeof(switch_address_sockaddr_in_structure);
				if ((write_bytes =
						sendto(gniazdo, buffor, read_bytes, 0,
								(struct sockaddr *) &switch_address_sockaddr_in_structure,
								length)) != (ssize_t) read_bytes)
					syserr("error while sending datagram to client");
			} else if (gniazdo == zdarzenia[i].data.fd) {
				length = (socklen_t) sizeof(from_address_sockaddr_in_structure);
				read_bytes = recvfrom(gniazdo, buffor,
						(size_t)(sizeof(buffor) - 1), 0,
						(struct sockaddr *) &from_address_sockaddr_in_structure,
						&length);
				buffor[read_bytes + 1] = '\0';
				printf("Ramka od switcha (%d bajtow): <%s>\n", (int) read_bytes,
						buffor);

				if (length < 0) {
					fprintf(stderr, "error in recvfrom() function call!\n");
					close(zdarzenia[i].data.fd);
					exit(1);
				}

				if ((write_bytes = write(file_descriptor, buffor, read_bytes))
						< 0) {
					fprintf(stderr, "error in write() function call!\n");
					exit(1);
				}
			} else if ((zdarzenia[i].events & EPOLLERR)
					|| (!(zdarzenia[i].events & EPOLLIN))
					|| (zdarzenia[i].events & EPOLLHUP)) {
				fprintf(stderr, "error in epoll!\n");
				close(zdarzenia[i].data.fd);
			} else {
				exit(1);
			}
		}
	}
}

void parse_arguments(int argc, char* argv[]) {
	while ((command = getopt(argc, argv, ":d:")) != -1) {
		if (command == 'd') {
			nazwa_interfacu = optarg;
		} else if (command == '?') {
			fprintf(stderr, "Nieznana komenda '-%c'.\n", optopt);
			exit(1);
		} else if (command == ':') {
			fprintf(stderr, "Nastepujaca opcja: '-%c' wymaga argumentu.\n",
					optopt);
			exit(1);
		}
	}

	if (optind != argc - 1) {
		fprintf(stderr,
				"Nieprawidlowa ilosc argumentow, podaj <host>:<port>, bo jest to wymagane\n");
		fprintf(stderr, "opcjonalnie -d <nazwa interfacu>\n");
		exit(1);
	}
	adres_hosta = strsep(&argv[optind], ":");
	numer_portu = strsep(&argv[optind], ":");
	if ((numer_portu == NULL ) || (adres_hosta == NULL )) {
		fprintf(stderr,
				"Nieprawidlowa ilosc argumentow, podaj <host>:<port>, bo jest to wymagane\n");
		fprintf(stderr, "opcjonalnie -d <nazwa interfacu>\n");
		exit(1);
	}
	printf("adres hosta: %s\n", adres_hosta);
	printf("numer port: %s\n", numer_portu);
}

void init_configuration() {
	if ((file_descriptor = open("/dev/net/tun", O_RDWR)) < 0) {
		perror("error while opening(/dev/net/tun)");
		exit(1);
	}

	memset(&ifreq_struct_instance, 0, sizeof(ifreq_struct_instance));
	(void) memset(&addr_hints_addrinfo_structure, 0, sizeof(struct addrinfo));
	addr_hints_addrinfo_structure.ai_socktype = SOCK_DGRAM;
	addr_hints_addrinfo_structure.ai_protocol = IPPROTO_UDP;
	addr_hints_addrinfo_structure.ai_family = AF_INET;
	strncpy(ifreq_struct_instance.ifr_name, nazwa_interfacu, IFNAMSIZ);
	ifreq_struct_instance.ifr_flags = IFF_TAP | IFF_NO_PI;
	error_int = ioctl(file_descriptor, TUNSETIFF,
			(void *) &ifreq_struct_instance);
	if (error_int < 0) {
		perror("error during call ioctl(TUNSETIFF)");
		exit(1);
	}
	if (getaddrinfo(adres_hosta, NULL, &addr_hints_addrinfo_structure,
			&addr_result_addrinfo_structure) != 0) {
		syserr("error in getaddrinfo() method call!");
	}

	switch_address_sockaddr_in_structure.sin_port = htons(
			(uint16_t) atoi(numer_portu));
	switch_address_sockaddr_in_structure.sin_addr.s_addr =
			((struct sockaddr_in*) (addr_result_addrinfo_structure->ai_addr))->sin_addr.s_addr;
	switch_address_sockaddr_in_structure.sin_family = AF_INET;

	if ((gniazdo = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syserr("error in socket() function call!");
	}

	if ((epoll_file_descriptor = epoll_create1(0)) < 0) {
		syserr("error in epoll_create() function call");
	}

	zdarzenie_switcha.events = EPOLLIN;
	zdarzenie_interfacu.events = EPOLLIN;
	zdarzenie_interfacu.data.fd = file_descriptor;
	zdarzenie_switcha.data.fd = gniazdo;
	if ((epol_ctl_result = epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD,
			gniazdo, &zdarzenie_switcha)) < 0) {
		syserr("error in epoll_ctl() function call! - switch event");
	}
	if ((epol_ctl_result = epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD,
			file_descriptor, &zdarzenie_interfacu)) < 0) {
		syserr("error in epoll_ctl() function call! - interface event");
	}
	zdarzenia = calloc(LIMIT_ZDARZEN, sizeof(zdarzenie_switcha));
}
