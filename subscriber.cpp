#include "helpers.h"

#define BUFLEN 1551
#define MAXRECV (sizeof(tcp_message))

int main(int argc, char **argv) {
    // Dezactivez buffering-ul la afisare
    setvbuf(stdout , NULL , _IONBF , BUFSIZ);

    // Verific numarul de parametri
    if (argc != 4) {
        usage_client();
    }

    // Verific lungimea ID-ului
    if (strlen(argv[1]) > 10) {
        fprintf(stderr, "Client ID should not be greater than 10 characters!\n");
        exit(0);
    }

    int sockfd, ret, flag = 1;
    struct sockaddr_in serv_addr;
    char buffer[BUFLEN];
    tcp_message* received_msg;
    msg_client_to_sv sent_msg;

    fd_set read_fds;
    fd_set tmp_fds;

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    // Creez socket-ul de comunicatie
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "Socket could not be created.\n");

    // Completez datele pentru socket-ul TCP
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));

    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton\n");

    // Ma conectez la server
    ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "Connection could not be established.\n");

    // Trimit catre server id-ul cu care ma conectez la el
    ret = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    DIE(ret < 0, "ID could not be sent.\n");

    // Dezactivez algoritmul lui Neagle
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(ret < 0, "Error deactivating Neagle's algorithm\n");

    FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);


    while (1) {
        tmp_fds = read_fds;

        ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "Select error.\n");

        // Verific daca am primit mesaj de la tastatura (stdin)
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);
            
            // Verific daca am primit "exit"
            if (!strncmp(buffer, "exit\n", 5)) { // posibila sursa de eroare?
                break;
            }

            // Am primit ceva diferit de exit, procesez stringul
            buffer[strlen(buffer) - 1] = '\0';
            char *tokenizer = strtok(buffer, " ");
            DIE(tokenizer == NULL, "Wrong command.\n");

            // Verific primul cuvant din string
            if (!strncmp(tokenizer, "subscribe", 9)) {
                sent_msg.cmd_type = 's';
            } else if (!strncmp(tokenizer, "unsubscribe", 11)) {
                sent_msg.cmd_type = 'u';
            } else {
                DIE(true, "Unrecognized command.\n");
            }

            // Verific al doilea cuvant din string
            tokenizer = strtok(NULL, " ");
            DIE(tokenizer == NULL, "Wrong command.\n");

            strcpy(sent_msg.topic, tokenizer);

            sent_msg.sf = '0';
            // Verific si al treilea parametru, daca e cmd de subscribe
            if (sent_msg.cmd_type == 's') {
                tokenizer = strtok(NULL, " ");
                DIE(tokenizer == NULL, "Wrong command.\n");

                sent_msg.sf = tokenizer[0];
            }

            ret = send(sockfd, (char *) &sent_msg, sizeof(sent_msg), 0);
            DIE(ret < 0, "Failure at sending to the server.\n");

            if (sent_msg.cmd_type == 's') {
                printf("Subscribed to topic.\n");
            } else {
                printf("Unsubscribed from topic.\n");
            }
        }
        
        // Verific daca am primit mesaj de la server
        if (FD_ISSET(sockfd, &tmp_fds)) {
            char buffer2[MAXRECV];
            memset(buffer2, 0, MAXRECV);

            // Fac recv la mesajul de la sv
            ret = receive_package(buffer2, sockfd);
            // Sv s-a inchis, inchid si clientul
            if (ret == 0) {
                break;
            }
            DIE(ret < 0, "Error receiving from server.\n");

            received_msg = (tcp_message *)buffer2;
            printf("%s:%hu - %s - %s - %s\n", received_msg->ip, received_msg->port,
                   received_msg->topic, received_msg->data_type, received_msg->data_payload);
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;
}