#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<fcntl.h>
#include <pthread.h>
#include <regex.h>
#define BUF_SIZE 8192
#define max_conn 10
#define TRUE 1
#define FALSE 0


const char *HTTP_200_STRING = "OK";
const char *HTTP_404_STRING = "Not Found";
const char *HTTP_501_STRING = "Not Implemented";
const  char  *HTTP_404_CONTENT  =  "<html><head><title>404 Not Found</title></head><body><h1>Error 404, requested page does not exist!</h1></body></html>";
const  char  *HTTP_501_CONTENT  =  "<html><head><title>501 Not Implemented</title></head><body><h1>Error 501, server can not handle request</h1></body></html>";
char *cjpg="Content-Type: image/jpeg\r\n";
char *cpng="Content-Type: image/png\r\n";
char *cgif="Content-Type: image/gif\r\n";
char *chtml="Content-Type: text/html\r\n";
char *ccss="Content-Type: text/css\r\n";
char *cplain="Content-Type: text/plain\r\n";

pthread_t connections[max_conn];

void *worker(void *a) {
    //buffers
    char *bufferA = malloc(BUF_SIZE);
    char *bufferB = malloc(BUF_SIZE);
    size_t buffer_len;
    int connfd = *((int*)a);

    while(1){
        //flag for keeping connection
        int flag = FALSE;
        int flag_501 = TRUE;
        int flag_404 = FALSE;

        memset(bufferA, 0, sizeof(bufferA));
        memset(bufferB, 0, sizeof(bufferB));

        size_t bytes;
        while((bytes = recv(connfd, bufferB, sizeof(bufferB)+1, 0)) > 0) {
            if(bytes != 0){
                bufferB[bytes] = '\0';
                strcat(bufferA, bufferB);
            } else {
                break;
            }
            if(strcmp(bufferB, "/r/n/") == 0 || strstr(bufferA, "\r\n\r\n") != NULL) {
                break;
            }
        }

        if(bytes == 0)
            break;

        //tokentime!
        char* token = strtok(bufferA, " \r\n");
        int total = 0;
        char** token_list = malloc(sizeof(char*));
        token_list[0] = strdup(token);
        while( token != NULL ) {
            ++total;
            token_list = realloc(token_list, sizeof(char*)*(total+1));
            token = strtok(NULL, " \r\n");
            if(token != NULL) {
                token_list[total] = strdup(token);
            }
        }

        //always wash your dishes after you finish eating
        memset(bufferA, 0, sizeof(bufferA));
        memset(bufferB, 0, sizeof(bufferB));

        int fileIndex = -1, connectionIndex = -1;
        char* filename = malloc(1);
        char* content_type = chtml;
        char* HTTP_TYPE;
        for(int i = 0; i < total; i++) {
            if(strstr(token_list[i], "HTTP/")) {
                HTTP_TYPE = malloc(sizeof(token_list[i]));
                strcpy(HTTP_TYPE, token_list[i]);
            }

            if(strcasecmp(token_list[i], "GET") == 0) {
                flag_501 = FALSE;

                //reading tokens
                fileIndex = i+1;
                if(strlen(token_list[fileIndex]) > 1 && token_list[fileIndex][strlen(token_list[fileIndex])-1] != '/') {
                    filename = realloc(filename, strlen("web")+strlen(token_list[fileIndex])+1);
                    strcpy(filename, "web");
                    strcat(filename, token_list[fileIndex]);
                    if(strstr(filename, ".html") != NULL || strstr(filename, ".htm") != NULL)
                        content_type = chtml;
                    else if(strstr(filename, ".css") != NULL)
                        content_type = chtml;
                    else if(strstr(filename, ".jpg") != NULL)
                        content_type = cjpg;
                    else if(strstr(filename, ".png") != NULL)
                        content_type = cpng;
                    else if(strstr(filename, ".gif") != NULL)
                        content_type = cgif;
                    else
                        content_type = cplain;
                }
                else {
                    if(token_list[fileIndex][strlen(token_list[fileIndex])-1] == '/') {
                        filename = realloc(filename, strlen("web")+strlen(token_list[fileIndex])+strlen("index.html")+1);
                        strcpy(filename, "web");
                        strcat(filename, token_list[fileIndex]);
                        strcat(filename, "index.html");
                    }
                    else {
                        filename = realloc(filename, strlen("web")+strlen(token_list[fileIndex])+1);
                        strcpy(filename, "web");
                        strcat(filename, token_list[fileIndex]);
                    }
                }
                //Opens file or sets 404 flag
                FILE *file;
                file = fopen(filename, "rb");
                if (file) {
                    fseek(file,0,SEEK_END);
                    buffer_len = ftell(file)+1;
                    fseek(file,0,SEEK_SET);
                    bufferA = realloc(bufferA, buffer_len+1);
                    memset(bufferA,0,buffer_len+1);
                    fread(bufferA, sizeof(char), buffer_len, file);
                    fclose(file);
                }
                else {
                    printf("File '%s' does NOT exist. Raising 404 flag\n", filename);
                    flag_404 = TRUE;
                }
                continue;
            }
            else if(strcasecmp(token_list[i], "Connection:") == 0) {
                connectionIndex = i+1;
                if(strcasecmp(token_list[connectionIndex], "keep-alive") == 0) {
                    flag = TRUE;
                }
                else
                    flag = FALSE;
                continue;
            }
            else if(strcasecmp(token_list[i], "HEADER") == 0) {
                flag_501 = TRUE;
                break;
            }
            else if(strcasecmp(token_list[i], "POST") == 0) {
                flag_501 = TRUE;
                break;
            }
        }
        //those tokens are so got


        if(flag_404 == TRUE) {
            strcpy(bufferB, HTTP_TYPE);
            strcat(bufferB, " 404 ");
            strcat(bufferB, HTTP_404_STRING);
            strcat(bufferB, "\r\n");
            if(connectionIndex != -1) {
                strcat(bufferB, "Connection: ");
                strcat(bufferB, token_list[connectionIndex]);
                strcat(bufferB, "\r\n");
            }
            else {
                strcat(bufferB, "Connection: close\r\n");
            }
            strcat(bufferB, "Content-Length: ");
            char content_length[2048];
            sprintf(content_length, "%lu", strlen(HTTP_404_CONTENT));
            strcat(bufferB, content_length);
            strcat(bufferB, "\r\n");
            strcat(bufferB, "Content-Type: ");
            strcat(bufferB, chtml);
            strcat(bufferB, "\r\n");
            write(connfd, bufferB, strlen(bufferB));
            write(connfd, HTTP_404_CONTENT, strlen(HTTP_404_CONTENT));
        }
        else if(flag_501 == TRUE) {
            strcpy(bufferB, "HTTP/1.1 501 ");
            strcat(bufferB, HTTP_501_STRING);
            strcat(bufferB, "\r\n");
            if(connectionIndex != -1) {
                strcat(bufferB, "Connection: close\r\n");
            }
            else {
                strcat(bufferB, "Connection: close\r\n");
            }
            strcat(bufferB, "Content-Length: ");
            char content_length[2048];
            sprintf(content_length, "%lu", strlen(HTTP_501_CONTENT));
            strcat(bufferB, content_length);
            strcat(bufferB, "\r\n");
            strcat(bufferB, "Content-Type: ");
            strcat(bufferB, chtml);
            strcat(bufferB, "\r\n");
            write(connfd, bufferB, strlen(bufferB));
            write(connfd, HTTP_501_CONTENT, strlen(HTTP_501_CONTENT));
        }
        else {
            //200
            strcpy(bufferB, HTTP_TYPE);
            strcat(bufferB, " 200 ");
            strcat(bufferB, HTTP_200_STRING);
            strcat(bufferB, "\r\n");
            if(connectionIndex != -1) {
                strcat(bufferB, "Connection: close\r\n");
            }
            else {
                strcat(bufferB, "Connection: close\r\n");
            }
            strcat(bufferB, "Content-Length: ");
            char content_length[2048];
            sprintf(content_length, "%lu", buffer_len);
            strcat(bufferB, content_length);
            strcat(bufferB, "\r\n");
            strcat(bufferB, "Content-Type: ");
            strcat(bufferB, content_type);
            strcat(bufferB, "\r\n");
            write(connfd, bufferB, strlen(bufferB));
            send(connfd, bufferA, buffer_len, 0);
        }

        for(int i = 0; i < total; i++) {
            free(token_list[i]);
        }
        free(token_list);
        free(filename);
        free(HTTP_TYPE);
        if(flag == FALSE) {
            //Close connection and exit thread
            printf("Closing Connection...\n");
            close(connfd);
            pthread_exit(NULL);
            break;
        }
    }


}

int main(int argc, char *argv[]) {
    //Checking arguments to read port number
    int portNum;
    if(argc != 2){
        printf("%i incorrect amount of arguments, provide one argument as port number\n", argc);
        return -1;
    }

    portNum = atoi(argv[1]);

    //Confirm port is valid
    if(portNum < 1024){
        printf("invalid port number, %d and others below 1024 are reserved\n", portNum);
        return -1;
    }else if(portNum > 65535){
        printf("invalid port number, %d is above the max port number, 65535\n", portNum);
        return -1;
    }

    //Create Server socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0))==-1){
        perror("socket");
        exit(1);
    }else{
        printf("successfully created socket\n");
    }

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    memset(&addr, '0', addrlen);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portNum);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((bind(sockfd, (struct sockaddr*)&addr, addrlen)) != 0) {
        perror("bind");
        exit(1);
    }else{
        printf("successfully binded socket\n");
    }

    if (listen(sockfd, max_conn) != 0) {
        perror("listen");
        exit(1);
    }else{
        printf("successfully began listening\n");
    }

    int client;
    for(;/*infinite loop*/;){
        pthread_t thread;
        client = accept(sockfd, (struct sockaddr*)NULL, NULL);
        if(client == -1) {
            continue;
        }
        pthread_create(&thread, NULL, worker, (void*)&client);
        pthread_join(thread, NULL);
        sleep(1);
    }

    return 0;
}