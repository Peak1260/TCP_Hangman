#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CLIENTS 5
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;

struct hangman_data_server{ // struct to send data to server
    int msg_flag;
    int word_length;
    int num_incorrect;
    char data[64];
};


struct hangman_data_from_client{ // struct to send data to server
    int msg_length;
    char data[64];
};


void error(const char *msg){
    perror(msg);
    exit(1);
}



void setup_signal_handler(){
    signal(SIGPIPE, SIG_IGN);

}

/*
Description:
A function that checks if the letter is in the word and 
if it is then update the word with the letter filled in and 
the rest of the word with underscores
*/
void update_display_word(char *word, char *display_word, char letter, char *wrong_guesses){
    int found = 0;
    for (int i = 0; i < strlen(word); i++){
        if (word[i] == letter){
            display_word[i] = letter;
            found = 1;
        }
    }
    if (found == 0){
        wrong_guesses[strlen(wrong_guesses)] = letter;
    }
    return;
}


// Client Handler
void *client_handler(void *arg){
    int newsockfd = *((int *)arg);
    free(arg);
    pthread_detach(pthread_self());
    srand(time(NULL) + getpid());
    
    FILE *fp;
    char **words = NULL;
    char wrong_guesses[26] = "";
    char my_word[64] = ""; 
    int num_words = 0;

    // Open the file for reading
    fp = fopen("hangman_words.txt", "r");
    if (fp == NULL) {
        printf("Could not open file hangman_words.txt\n");
        // return NULL;
    }

    // Read words into a dynamically allocated array of strings
    char line[64];
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Strip \r and \n characters
        line[strcspn(line, "\r\n")] = '\0';

        // Reallocate memory for the words array (reallocating as more words are read)
        words = realloc(words, sizeof(char *) * (num_words + 1));
        words[num_words] = strdup(line); // Duplicate the word into the array
        num_words++;
    }
    fclose(fp);

    if (num_words == 0) {
        printf("No words were loaded from the file.\n");
        // return NULL;
    }

    // Generate a random index within the bounds of the word list
    int random_number = rand() % num_words;

    // Copy the selected word to my_word
    strcpy(my_word, words[random_number]);

    // Print the selected word and its length
    printf("Random word: %s\n", my_word);
    // printf("Length of my_word: %ld\n", strlen(my_word));


    // Game Start Packet
    struct hangman_data_server game_start;
    game_start.msg_flag = -2;
    game_start.word_length = -2;
    game_start.num_incorrect = -2;
    bzero(game_start.data, 64);
    send(newsockfd, &game_start, sizeof(game_start), 0);


    // regular packet
    struct hangman_data_server server_data;
    bzero(server_data.data, 64); // zero out all of the .data 

    // Game Control Packet
    server_data.msg_flag = 0;
    server_data.word_length = strlen(words[random_number]); 
    server_data.num_incorrect = 0;
    
    // for the length of the word, send underscores. For example: if the word is "hello" then send "_____"
    for (int i = 0; i < server_data.word_length; i++) {
        server_data.data[i] = '_';
    }

    while (1){
        struct hangman_data_from_client client_data;
        recv(newsockfd, &client_data, sizeof(client_data), 0);
        client_data.msg_length = ntohl(client_data.msg_length);
        // printf("Received message length: %d\n", client_data.msg_length); // for debugging
        // printf("Received message: %s\n", client_data.data);// for debugging

        if (client_data.msg_length == -5){ // client disconnected
            //printf("Client disconnected\n");
            break;
        }

        if (client_data.msg_length == 0){ // GAME START!
            send(newsockfd, &server_data, sizeof(server_data), 0);

        } else { // GAME IN PROGRESS !
            
            // printf("Client %d , Received message: %c\n", getpid(), client_data.data[0]); // for debugging
            update_display_word(my_word, server_data.data, client_data.data[0], wrong_guesses);

            // Game Control Packet
            server_data.msg_flag = 0;
            server_data.word_length = strlen(words[random_number]); 
            server_data.num_incorrect = strlen(wrong_guesses);

            // printf("Updated display word: %s\n", server_data.data); // for debugging
            // printf("Word Length: %d\n", server_data.word_length);  // for debugging
            // printf("Number of incorrect guesses: %d\n", server_data.num_incorrect);  // for debugging
            // printf("Wrong guesses: %s\n", wrong_guesses); // for debugging


            // Append wrong guesses to server_data.data
            for (int i = 0; i < strlen(wrong_guesses); i++){
                server_data.data[server_data.word_length + i] = wrong_guesses[i];
            }
            // printf("Updated display word with wrong guesses: %s\n", server_data.data); // for debugging


            // Check if the entire word has been guessed (i.e. no more underscores)
            int word_guessed = 1;
            for (int i = 0; i < server_data.word_length; i++){
                if (server_data.data[i] == '_'){
                    word_guessed = 0;
                }
            }

            
            if (word_guessed == 1){ // YOU WIN

                // ----------Sending the word-------------------------
                server_data.msg_flag = 100;
                strcpy(server_data.data, my_word);
                send(newsockfd, &server_data, sizeof(server_data), 0);
                // ----------Sending the word-------------------------

                // Sending "You Win!" message
                bzero(server_data.data, 64);
                strcpy(server_data.data, "You Win!");
                // printf("Server message: %s\n", server_data.data); // for debugging
                server_data.msg_flag = strlen(server_data.data);
                send(newsockfd, &server_data, sizeof(server_data), 0);
                break;
                
            } else if (server_data.num_incorrect == 6) { // YOU LOSE

                // ----------Sending the word-------------------------
                server_data.msg_flag = 100;
                strcpy(server_data.data, my_word);
                send(newsockfd, &server_data, sizeof(server_data), 0);
                // ----------Sending the word-------------------------

                // Sending "You Lose!" message
                bzero(server_data.data, 64);
                strcpy(server_data.data, "You Lose!");
                // printf("Server message: %s\n", server_data.data); // for debugging
                server_data.msg_flag = strlen(server_data.data);
                send(newsockfd, &server_data, sizeof(server_data), 0);
                // printf("Count of clients (You Lose): %d\n", client_count);

                break;

            } else { // GAME CONTINUES
                send(newsockfd, &server_data, sizeof(server_data), 0); // send the updated display word to the client
            }  
        }
    }

    close(newsockfd);

    // Decrement the client count
    pthread_mutex_lock(&client_mutex);
    client_count--;
    //printf("Count of clients after subtraction: %d\n", client_count);
    pthread_mutex_unlock(&client_mutex);
    pthread_exit(NULL);

    // return NULL;
}





int main(int argc, char *argv[]){
    setup_signal_handler();


    // SETUP SERVER ------------------
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[64];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    if (argc < 2){
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
    
    listen(sockfd, MAX_CLIENTS); // listen for up to 3 clients
    clilen = sizeof(cli_addr);

    // SETUP SERVER ------------------ 

    while(1){
        int *newsockfd = malloc(sizeof(int));
        *newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (*newsockfd < 0) error("ERROR on accept");

        pthread_mutex_lock(&client_mutex);
        client_count++;

        if (client_count > MAX_CLIENTS){
            printf("Server is full\n");
            client_count--;
            //printf("Count of clients after full server: %d\n", client_count);

            pthread_mutex_unlock(&client_mutex);

            struct hangman_data_server server_overloaded;
            server_overloaded.msg_flag = -1;
            server_overloaded.word_length = -1;
            server_overloaded.num_incorrect = -1;
            bzero(server_overloaded.data, 64);
            send(*newsockfd, &server_overloaded, sizeof(server_overloaded), 0);
            close(*newsockfd);
            free(newsockfd);
            continue;
        }

        
        //printf("Count of clients: %d\n", client_count);
        pthread_mutex_unlock(&client_mutex);

        pthread_t thread;
        int ret = pthread_create(&thread, NULL, client_handler, (void *)newsockfd);
        if (ret != 0) {
            printf("Error creating thread: %s\n", strerror(ret));
            close (*newsockfd);
            free(newsockfd);
            return 1;
        }
          
    }

    printf("Leaving while loop in server");

    close(sockfd);
    return 0;
}