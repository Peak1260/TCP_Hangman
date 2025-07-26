#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>

int sockfd;

void error(const char *msg){
    perror(msg);
    exit(0);
}

struct hangman_data_client{ // struct to send data to server
    int msg_length;
    char data[64];
};


struct hangman_data_from_server{ // struct to send data to server
    int msg_flag;
    int word_length;
    int num_incorrect;
    char data[64];
};



void handle_sigint(int sig){
    // printf("Caught signal %d\n", sig);

    // send a message to the server to close the connection
    struct hangman_data_client client_data;
    client_data.msg_length = htonl(-5);
    bzero(client_data.data, 64);
    send(sockfd, &client_data, sizeof(client_data), 0);

    // shutdown(sockfd, SHUT_RDWR);  
    close(sockfd);
    exit(0);
}

int main(int argc, char *argv[]){
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[64];
    if (argc < 3){
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }

    signal(SIGINT, handle_sigint);


    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)error("ERROR opening socket");
    server = gethostbyname(argv[1]);

    if (server == NULL){
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);


    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR connecting");

    
    
    struct hangman_data_from_server server_data;
    bzero(&server_data, sizeof(server_data));
    recv(sockfd, &server_data, sizeof(server_data), 0);

    if (server_data.msg_flag == -1){ // server is full
        printf(">>>server-overloaded\n");
        close(sockfd);
        return 0;
    }



    // SERVER IS NOT FULL
    printf(">>>Ready to start game? (y/n): ");
    bzero(buffer, 64);

    // fgets(buffer, 64, stdin);
    if (fgets(buffer, 64, stdin) == NULL){ // USER presses ctrl + d
        printf("\n");

        // send the server a message to close the connection
        struct hangman_data_client client_data;
        client_data.msg_length = htonl(-5);
        bzero(client_data.data, 64);
        send(sockfd, &client_data, sizeof(client_data), 0);



        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return 0;
    }

    // make the user input lowercase
    buffer[0] = tolower(buffer[0]);

    struct hangman_data_client client_data;
    // if the user enters 'n' then terminate the client
    if (strcmp(buffer, "n\n") == 0){ // CLIENT SAID NO 


        // send a message to the server to close the connection
        struct hangman_data_client client_data;
        client_data.msg_length = htonl(-5);
        bzero(client_data.data, 64);
        send(sockfd, &client_data, sizeof(client_data), 0);

        close(sockfd);
        // printf(">>>Goodbye!\n");
        return 0;
    }

    else if (strcmp(buffer, "y\n") != 0){ // INVALID INPUT

        // send a message to the server to close the connection
        struct hangman_data_client client_data;
        client_data.msg_length = htonl(-5);
        bzero(client_data.data, 64);
        send(sockfd, &client_data, sizeof(client_data), 0);

        close(sockfd);
        // printf(">>>Error! Please Input y/n\n");
        return 0;
    }

    else{  // CLIENT SAID YES
        //if the user enters 'y' then send an empty message to the server with msg_length = 0
        client_data.msg_length = htonl(0);
        bzero(client_data.data, 64);
        send(sockfd, &client_data, sizeof(client_data), 0);
    }
     


    while(1){
        struct hangman_data_from_server server_data;
        bzero(&server_data, sizeof(server_data));
        recv(sockfd, &server_data, sizeof(server_data), 0);

        if (server_data.msg_flag == -1){ // server is full
            printf(">>>server-overloaded\n");
            close(sockfd);
            return 0;
        }
        else if (server_data.msg_flag == 100){ // server sends the word at end of game
            printf(">>>The word was %s\n", server_data.data);
            continue;
        }
        else if (server_data.msg_flag > 0){ // server sends final message (GAME OVER)
            printf(">>>%s\n", server_data.data);
            printf(">>>Game Over!\n");
            close(sockfd);
            return 0;
        } 
        // printf("Word Length: %d\n", server_data.word_length);  // for debugging
        // printf("Length of server data: %ld\n", strlen(server_data.data)); // for debugging

        printf(">>>");
        for (int i = 0; i < server_data.word_length ; i++){

            // on the last letter, don't print a space
            if (i == server_data.word_length - 1){
                printf("%c", server_data.data[i]);
            } else {
                printf("%c ", server_data.data[i]);
            }
            
        }

        printf("\n");
        printf(">>>Incorrect Guesses: ");
        


        if (server_data.num_incorrect > 0) {
            for (int i = 0; i < server_data.num_incorrect; i++) {
                // on the last incorrect guess, don't print a space
                if (i == server_data.num_incorrect - 1){
                    printf("%c", server_data.data[server_data.word_length + i]); 
                } else {
                    printf("%c ", server_data.data[server_data.word_length + i]); 
                }
            }
            
        }

        printf("\n");
        printf(">>>\n");

        printf(">>>Letter to guess: ");
        bzero(buffer, 64);
        // fgets(buffer, 64, stdin);
        if (fgets(buffer, 64, stdin) == NULL){ // USER presses ctrl + d

            // send the server a message to close the connection
            struct hangman_data_client client_data;
            client_data.msg_length = htonl(-5);
            bzero(client_data.data, 64);
            send(sockfd, &client_data, sizeof(client_data), 0);

            printf("\n");
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            return 0;
        }

        // If users inputs more than 1 character or a non-alphabet character, ask for input again
        while (strlen(buffer) != 2 || !isalpha(buffer[0])){
            printf(">>>Error! Please guess one letter.\n");
            printf(">>>Letter to guess: ");
            bzero(buffer, 64);
            // fgets(buffer, 64, stdin);
            if (fgets(buffer, 64, stdin) == NULL){ // USER presses ctrl + d

                // send the server a message to close the connection
                struct hangman_data_client client_data;
                client_data.msg_length = htonl(-5);
                bzero(client_data.data, 64);
                send(sockfd, &client_data, sizeof(client_data), 0);


                printf("\n");
                shutdown(sockfd, SHUT_RDWR);
                close(sockfd);
                return 0;
            }
        }

        buffer[0] = tolower(buffer[0]);

        client_data.msg_length = htonl(1);
        client_data.data[0] = buffer[0];

        // print message length and message
        // printf("Message length: %d\n", ntohl(client_data.msg_length)); // for debugging
        // printf("Message: %s\n", client_data.data); // for debugging
        send(sockfd, &client_data, sizeof(client_data), 0);

    }
    close(sockfd);
    return 0;
}