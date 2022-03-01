#include "helpers.h"

int main(int argc, char **argv) {
    // Dezactivez buffering-ul la afisare
    setvbuf(stdout , NULL , _IONBF , BUFSIZ);
    setvbuf(stdin , NULL , _IONBF , BUFSIZ);
    
    // Verific numarul de argumente
    if (argc != 2) {
        usage_server();
    }

    // Declaratii variabile
    char buffer[BUFLEN];
    int socket_udp, socket_tcp, new_client_socket;
    int flag_ngl = 1;
    int port_number, max_fd;
    
    socklen_t socklen_udp = sizeof(sockaddr);
    socklen_t socklen_client = sizeof(sockaddr);

    fd_set read_fds, tmp_fds;

    tcp_message tcp_msg;
    udp_packet aux;
    msg_client_to_sv* client_msg;

    ////// Data structures used ///////
    // Mapeaza id-ul unic al clientului la informatii despre acesta
    unordered_map<string, client_info> clients_info;

    // Mapeaza socket-ul cu care se conecteaza clientul la id-ul sau unic
    unordered_map<int, string> connected_clients;
    //////////////////////////////////

    // Initializez socket-ul udp
    socket_udp = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(socket_udp < 0, "Failure: UDP socket!\n");

    // Initializez socket-ul tcp
    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socket_tcp < 0, "Failure: TCP socket!\n");

    // Verific port-ul
    port_number = atoi(argv[1]);
    DIE(port_number <= 1024, "Port number is not available!\n");

    sockaddr_in addr_udp, addr_tcp, addr_client;

    // Informatiile pentru socketul UDP
    addr_udp.sin_family = AF_INET;
    addr_udp.sin_port = htons(port_number);
    inet_aton("127.0.0.1", &addr_udp.sin_addr);

    // Informatiile pentru socketul TCP
    addr_tcp.sin_family = AF_INET;
    addr_tcp.sin_port = htons(port_number);
    inet_aton("127.0.0.1", &addr_tcp.sin_addr);

    int ret;

    ret = bind(socket_udp, (struct sockaddr *) &addr_udp, sizeof(struct sockaddr_in));
    DIE(ret < 0, "Failure: UDP bind.\n");

    ret = bind(socket_tcp, (struct sockaddr *) &addr_tcp, sizeof(struct sockaddr_in));
    DIE(ret < 0, "Failure: TCP bind.\n");
    // Pasivizez socketul pentru a asculta pentru conexiuni noi
    ret = listen(socket_tcp, MAX_CLIENTS);
    DIE(ret < 0, "Failure: TCP listen.\n");


    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(socket_tcp, &read_fds);
    FD_SET(socket_udp, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    max_fd = socket_tcp;

    bool flag = false;

    while (!flag) {
        tmp_fds = read_fds;
        memset(buffer, 0, BUFLEN);

        ret = select(max_fd + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "Select error.\n");

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                // Comanda de la tastatura
                if (i == 0) {
                    fgets(buffer, BUFLEN - 1, stdin);

                    if (!strncmp(buffer, "exit\n", 5)) {
                        flag = true;
                        break;
                    } else {
                        printf("Wrong command\n");
                    }
                }
                // Se primeste ceva pe socket-ul UDP
                else if (i == socket_udp) {
                    // Primit mesaj de la clienti UDP
                    struct sockaddr_in cli_addr;
                    
                    memset(&aux, 0, BUFLEN);
                    memset(&tcp_msg, 0, sizeof(tcp_msg));
                    ret = recvfrom(socket_udp, &aux, sizeof(aux), 0, (sockaddr *) &cli_addr, &socklen_udp);
                    DIE(ret < 0, "UDP socket error. No info.\n");

                    tcp_msg.port = ntohs(cli_addr.sin_port);
                    strcpy(tcp_msg.ip, inet_ntoa(cli_addr.sin_addr));
                    // Convertesc mesajul dupa specificatia protocolului
                    ret = convert_message(&aux, &tcp_msg);
                    if (ret) {
                        // Trimit mesajul sau il bufferez, in functie de sf si daca clientul e conectat sau nu
                        send_and_buffer(clients_info, connected_clients, tcp_msg);
                    } else {
                        DIE(true, "Failure at converting message\n");
                    }
                }
                // Se primeste ceva pe socket-ul TCP
                else if (i == socket_tcp) {
                    // Ii creez un socket nou
                    new_client_socket = accept(i, (sockaddr *) &addr_client, &socklen_client);
                    DIE(new_client_socket < 0, "Accept failure.\n");

                    // Dezactivam algoritmul lui Neagle
                    setsockopt(new_client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag_ngl, sizeof(int));

                    FD_SET(new_client_socket, &read_fds);
                    // Updatam socketul maxim
                    if (new_client_socket > max_fd) {
                        max_fd = new_client_socket;
                    }

                    // Primesc o cerere de conectare
                    memset(buffer, 0, BUFLEN);

                    ret = recv(new_client_socket, buffer, BUFLEN - 1, 0);
                    DIE(ret < 0, "Failed to receive ID\n");

                    string aux_id(buffer);

                    if (clients_info.find(aux_id) != clients_info.end()) {
                        // Verificam daca clientul nu e deja conectat
                        if (clients_info.at(aux_id).is_connected) {
                            printf("Client %s already connected.\n", buffer);
                            shutdown(new_client_socket, SHUT_RDWR);
                            close(new_client_socket);
                            FD_CLR(new_client_socket, &read_fds);
                        } else {
                            clients_info.at(aux_id).is_connected = true;
                            connected_clients.insert({new_client_socket, aux_id});
                            printf("New client %s connected from %s:%hu.\n", buffer,
                                 inet_ntoa(addr_client.sin_addr), ntohs(addr_client.sin_port));
                            // Trimitem mesajele din coada
                            if (!clients_info.at(aux_id).messages.empty()) {
                                memset(&tcp_msg, 0, sizeof(tcp_msg));
                                while (!clients_info.at(aux_id).messages.empty()) {
                                    tcp_msg = clients_info.at(aux_id).messages.front();

                                    ret = send_package((char *) &tcp_msg, new_client_socket);
                                    DIE(ret < 0, "Unable to transmit from buffer.\n");
                                    clients_info.at(aux_id).messages.pop();
                                }
                            }
                        }
                    } else {
                        // Client nou, il inseram in map-ul de conectari si in cel de informatii
                        connected_clients.insert({new_client_socket, aux_id});
                        client_info aux_client_info;

                        aux_client_info.is_connected = true;
                        clients_info.insert({aux_id, aux_client_info});
                        printf("New client %s connected from %s:%hu.\n", buffer,
                            inet_ntoa(addr_client.sin_addr), ntohs(addr_client.sin_port));
                    }
                }
                // Primesc o comanda de la clientii TCP 
                else {
                    char buffer_aux[53];
                    memset(buffer_aux, 0, BUFLEN);
                    ret = recv(i, buffer_aux, 53, 0);

                    DIE(ret < 0, "Nothing received.\n");
                    // Verific daca primesc ceva de la clienti
                    if (ret == 0) {
                        // Nu primesc nimic, clientul s-a deconectat
                        string aux = connected_clients.at(i);
                        cout << "Client " << aux << " disconnected." << endl;

                        clients_info.at(connected_clients.at(i)).is_connected = false;
                        connected_clients.erase(i);

                        FD_CLR(i, &read_fds);
                        close(i);                       
                    } else {
                        // Primesc un mesaj de tip subscribe/unsubscribe
                        client_msg = (msg_client_to_sv *)buffer_aux; 
                        string aux_topic(client_msg->topic);
                        int socket_primit = i;

                        if (client_msg->cmd_type == 'u') {
                            // Unsubscribe logic
                            string aux_id = connected_clients.at(socket_primit);
                            client_info aux_info = clients_info.at(aux_id);

                            auto it = topic_present(aux_info.topics, aux_topic);

                            if (it == -1) { 
                                printf("Cannot unsubscribe from inexistent topic.\n");
                            } else {
                                int position = find_index_of_topic(aux_info.topics, aux_topic);
                                
                                clients_info.at(aux_id).topics.erase(
                                    clients_info.at(aux_id).topics.begin() + position);
                            }
                        } else {
                            // Subscribe logic
                            string aux_id = connected_clients.at(socket_primit);
                            client_info aux_info = clients_info.at(aux_id);

                            // Already subscribed to that topic
                            if (topic_present(aux_info.topics, aux_topic) == 1) {
                                printf("Topic already exists\n");
                            } else {
                                // Creating a new topic for that user
                                topic_info new_topic;
                                if (client_msg->sf == '1') {
                                    new_topic.sf = true;
                                } else {
                                    new_topic.sf = false;
                                }
                                
                                string new_string(client_msg->topic);
                                new_topic.topic.assign(new_string);

                                clients_info.at(aux_id).topics.push_back(new_topic);
                            }
                        }
                    }
                }
            } 
        }
    }

    close_all_sockets(&read_fds, max_fd);
    close(socket_udp);
    shutdown(socket_tcp, SHUT_RDWR);
    close(socket_tcp);
    return 0;
}