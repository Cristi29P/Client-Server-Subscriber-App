#include "helpers.h"


void usage_server()
{
	fprintf(stderr, "Usage: ./server <PORT>\n");
	exit(0);
}

void usage_client()
{
	fprintf(stderr, "Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n");
	exit(0);
}

int receive_package(char *recv_buffer, int socket) {
    // recv_buffer must be initialised in the main program with MAXRECV size char recv_buffer[MAXRECV]
    memset(recv_buffer, '\0', MAXRECV);

    int ret = 0;
    int offset = 0;
    int size_left = 0;

    // ret contine cati bytes s-au primit
    ret = recv(socket, recv_buffer, MAXRECV, 0);
    
    if (ret < 0) {
        perror("Error receving.\n");
        return -1;
    }

    if (ret == 0) {
        perror("Connection closed.\n");
        return 0;
    }

    if (ret > 0 && ret < (int)MAXRECV) {
        // Mai trebuie primiti size_left bytes
        size_left = MAXRECV - ret;
        offset = ret;
        while (size_left > 0) {
            ret = recv(socket, recv_buffer + offset, size_left, 0);
            if (ret < 0) {
                perror("Error receving.\n");
                return -1; 
            }
            offset += ret;
            size_left -= ret;
        }
    }
    return 1;
}

int topic_present(vector<topic_info> &topics, string topic) {
    for (auto& it : topics) {
        if (it.topic.compare(topic) == 0) {
            return 1;
        }
    }
    return -1;
}

bool get_sf(vector<topic_info>& topics, string topic) {
    for (auto& it : topics) {
        if (it.topic.compare(topic) == 0) {
            return it.sf;
        }
    }
    return false;
}

int send_package(char *send_buffer, int socket) {
    // send_buffer must be initialised in the main program with MAXRECV size char send_buffer[MAXRECV]
    int ret = 0;
    int length = MAXRECV;
    while (length > 0) {
        ret = send(socket, send_buffer, length, 0);
        if (ret <= 0) {
            return -1;
        }
        send_buffer += ret;
        length -= ret;
    }
    return 0;

}

void send_and_buffer(unordered_map<string, client_info>& clients_info,
                    unordered_map<int, string>& connected_clients,
                    tcp_message msg) {
    // caut prin lista de online ---> va id-urile celor online ca sa le trimit direct lor,
    // dar trebuie sa vad daca topicul este si pt ei
    string aux_topic(msg.topic);
    // Iteram peste mapa de clienti online
    for (auto& it1 : connected_clients) {
        int temp_socket = it1.first;
        string aux_string = it1.second;

        // Am gasit un id care corespunde unui utilizator online
        if (clients_info.find(aux_string) != clients_info.end()) {
            if (topic_present(clients_info[aux_string].topics, aux_topic) == 1) {
                int ret = send_package((char *) &msg, temp_socket);
                DIE(ret < 0, "Unable to transmit.\n");
            }
        }
    }
    // Iteram peste mapa de informatii client ca sa adaugam mesajele in buffer pt cei cu sf = 1
    for (auto& it1 : clients_info) {
        if (it1.second.is_connected == false && topic_present(it1.second.topics, aux_topic) == 1) {
            if (get_sf(it1.second.topics, aux_topic) == true) {
                it1.second.messages.push(msg);
            }
        }
    }
}


int find_index_of_topic(vector<topic_info>& topics, string topic) {
    for(long unsigned int i=0; i < topics.size(); i++){
        if (topics[i].topic.compare(topic) == 0) {
            return i;
            break;
        }
    }
    return -1;
}

int convert_message(udp_packet *package, tcp_message *to_send) {
    double real = 0;
    memset(to_send->topic, 0, sizeof(to_send->topic));
    strncpy(to_send->topic, package->topic, 50);

    if (package->data_type == 0) {
        long long numar;
        numar = ntohl(*(uint32_t *)(package->data + 1));
        if (package->data[0]) {
            numar *= -1;
        }    
        sprintf(to_send->data_payload, "%lld", numar);
        strcpy(to_send->data_type, "INT");

    } else if (package->data_type == 1) {
        real = ntohs(*(uint16_t*)(package->data));
        real /= 100;
        sprintf(to_send->data_payload, "%.2f", real);
        strcpy(to_send->data_type, "SHORT_REAL");
    } else if (package->data_type == 2) {
        real = ntohl(*(uint32_t *)(package->data + 1));
        real /= pow(10, package->data[5]);

        if (package->data[0]) {
            real *= -1;
        }

        char real_string[20];
        memset(real_string, 0, 20);
        sprintf(real_string, "%lf", real);

        
        int len = strlen(real_string);
        int poz = 0;

        for (int i = len - 1; i>=0; i--) {
            if (real_string[i] != '0') {
                if (real_string[i] == '.') {
                    poz = i - 1;
                } else {
                    poz = i;
                }
                break;
            }
        }

        int to_print = poz + 2;
        snprintf(to_send->data_payload, to_print, "%s", real_string);
        strcpy(to_send->data_type, "FLOAT");
    } else {
        strcpy(to_send->data_payload, package->data);
        strcpy(to_send->data_type, "STRING");
    } 
    return 1;
}

void close_all_sockets(fd_set *fds, int max_fd) {
    for (int i = 2; i <= max_fd; ++i) {
        if (FD_ISSET(i, fds)) {
            close(i);
        }
    }
}



