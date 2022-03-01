#ifndef _HELPERS_H
#define _HELPERS_H

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <iostream>


#define BUFLEN 1551
#define MAX_CLIENTS 10000
#define MAXRECV (sizeof(tcp_message))

using namespace std;


struct __attribute__((packed)) tcp_message {
    uint16_t port;
    char ip[16];
    char topic[51];
    char data_type[11];
    char data_payload[1501];
};

struct __attribute__((packed)) msg_client_to_sv {
    char cmd_type;
    char topic[51];
    char sf;
};

struct __attribute__((packed)) udp_packet {
    char topic[50];
    uint8_t data_type;
    char data[1501];
};

struct topic_info {
    string topic;
    bool sf;
};

struct client_info {
    bool is_connected;
    vector<topic_info> topics;
    queue<tcp_message> messages;
};



#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)


void usage_client();

void usage_server();

int receive_package(char *recv_buffer, int socket);

int topic_present(vector<topic_info> &topics, string topic);

bool get_sf(vector<topic_info>& topics, string topic);

int send_package(char *send_buffer, int socket);

void send_and_buffer(unordered_map<string, client_info>& clients_info,
                    unordered_map<int, string>& connected_clients,
                    tcp_message msg);

int find_index_of_topic(vector<topic_info>& topics, string topic);

int convert_message(udp_packet *package, tcp_message *to_send);

void close_all_sockets(fd_set *fds, int max_fd);


#endif
