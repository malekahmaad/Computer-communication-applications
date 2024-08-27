#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

void dividing(char* link, char*** path, char* port, int* path_num) {
    int j = 7;
    int count = 0;
    int dir_counter = 0;
    int port_entered=0;
    *path_num += 1;
    (*path)[0] = (char *) malloc(sizeof(char) * strlen(link));
    strcpy((*path)[(*path_num)-1],"\0");
    for (int i = j; i < strlen(link); i++) {
        if (count == 0) {
            if (link[i] == ':' && port_entered==0) {
                dir_counter=0;
                count = 1;
            } else if (link[i] == '/' && i!= strlen(link)-1) {
                *path_num += 1;
                *path = (char**)realloc(*path, sizeof(char*) * (*path_num));
                (*path)[(*path_num) - 1] = (char *) malloc(sizeof(char) * strlen(link));
                dir_counter=0;
                strcpy((*path)[(*path_num)-1],"\0");
            } else if(link[i]!='/'){
                (*path)[(*path_num) -1][dir_counter] = link[i];
                dir_counter+=1;
                (*path)[(*path_num) -1][dir_counter]='\0';
            }
        }else if(count==1){
            if(link[i] =='/' && i!= strlen(link)-1){
                *path_num += 1;
                *path = (char**)realloc(*path, sizeof(char*) * (*path_num));
                (*path)[*path_num - 1] = (char *) malloc(sizeof(char) * strlen(link));
                strcpy((*path)[(*path_num)-1],"\0");
                dir_counter=0;
                count=0;
                port_entered=1;
            }else if(link[i]!='/'){
                port[dir_counter]=link[i];
                dir_counter+=1;
                port[dir_counter]='\0';
            }
        }
    }
    if(*path_num ==1 | link[strlen(link)-1]=='/'){
        *path_num += 1;
        *path = (char**)realloc(*path, sizeof(char*) * (*path_num));
        (*path)[*path_num - 1] = (char *) malloc(sizeof(char) * strlen(link));
        strcpy((*path)[(*path_num)-1],"index.html");
    }
}



int main(int argc, char *argv[]) {
    if(argc > 3 | argc == 1){
        printf("Usage: cproxy <URL> [-s]\n");
        exit(0);
    }
    if(argc ==3) {
        if (strcmp(argv[2],"-s")){
            printf("Usage: cproxy <URL> [-s]\n");
            exit(0);
        }
    }
    if(strncmp(argv[1],"http://",7)){
        printf("Usage: cproxy <URL> [-s]\n");
        exit(0);
    }
    char** path = (char**) malloc(sizeof (char*));
    char* port = (char*) malloc(sizeof (char)* strlen(argv[1]));
    strcpy(port,"\0");
    char* all_path = (char*) malloc(sizeof (char)* strlen(argv[1])* strlen(argv[1]));
    int path_num = 0;
    FILE *file;
    dividing(argv[1], &path, port, &path_num);
    if(strcmp(port, "\0")==0){
        strcpy(port,"80");
    }
    strcpy(all_path,"\0");
    for(int i=0;i<path_num;i++){
        if(i!=0) {
            strcat(all_path, "/");
        }
        strcat(all_path,path[i]);
    }
    file = fopen(all_path, "r");
    if(file == NULL){
        FILE *file2 = NULL;
        int fd=0;
        struct sockaddr_in saddr;
        struct hostent* hp = NULL;
        int length=512+ strlen(argv[1]);
        char request[length];
        unsigned char response[4096];
        int bytes=0;
        unsigned char* responseBody;
        int responseLength=0;
        int body_initialized=0;
        int len=0;
        int okResponse=0;
        char d[strlen(argv[1]) + 128];
        if((fd = socket(PF_INET, SOCK_STREAM, 0))<0){
            perror("socket");
            free(port);
            for(int j=0; j<path_num; j++){
                free(path[j]);
            }
            free(path);
            free(all_path);
            exit(0);
        }
        saddr.sin_family=AF_INET;
        hp= gethostbyname(path[0]);
        if(hp == NULL){
            herror("gethostbyname");
            close(fd);
            free(port);
            for(int j=0; j<path_num; j++){
                free(path[j]);
            }
            free(path);
            free(all_path);
            exit(0);
        }
        strcpy(request,"\0");
        strcpy(request, "GET /");
        for(int i=1;i<path_num-1;i++){
            strcat(request, path[i]);
            strcat(request,"/");
        }
        strcat(request, path[path_num-1]);
        strcat(request, " HTTP/1.0\r\nHost: ");
        strcat(request, path[0]);
        strcat(request, "\r\n\r\n");
        request[strlen(request)]='\0';
        printf("HTTP request =\n%s\nLEN = %d\n",request,(int)strlen(request));
        saddr.sin_addr.s_addr=((struct in_addr*)(hp->h_addr))->s_addr;
        saddr.sin_port = htons(atoi(port));
        if(connect(fd, (struct sockaddr*) &saddr, sizeof (saddr)) < 0){
            perror("connect");
            close(fd);
            free(port);
            for(int j=0; j<path_num; j++){
                free(path[j]);
            }
            free(path);
            free(all_path);
            exit(0);
        }
        if(write(fd, request, strlen(request)) < 0){
            perror("write");
            close(fd);
            free(port);
            for(int j=0; j<path_num; j++){
                free(path[j]);
            }
            free(path);
            free(all_path);
            exit(0);
        }
        if((bytes = read(fd, response,4095))<=0){
            perror("read");
            close(fd);
            free(port);
            for(int j=0; j<path_num; j++){
                free(path[j]);
            }
            free(path);
            free(all_path);
            exit(0);
        }
        response[bytes]='\0';
        printf("%s",response);
        responseLength+= bytes;
        if(strstr(response,"200 OK") != NULL){
            okResponse = 1;
        }
        if(okResponse ==1) {
            responseBody = (unsigned char *) strstr(response, "\r\n\r\n");
            strcpy(d, path[0]);
            for (int i = 0; i < path_num - 1; i++) {
                if (mkdir(d, 0777) != 0) {
                    if (errno != EEXIST) {
                        perror("error making the directory");
                        close(fd);
                        free(port);
                        for (int j = 0; j < path_num; j++) {
                            free(path[j]);
                        }
                        free(path);
                        free(all_path);
                        exit(0);
                    }
                }
                strcat(d, "/");
                strcat(d, path[i + 1]);
            }
            file2 = fopen(d, "w");
            if (file2 == NULL) {
                perror("error opening the file");
                close(fd);
                free(port);
                for (int j = 0; j < path_num; j++) {
                    free(path[j]);
                }
                free(path);
                free(all_path);
                exit(0);
            }
            if (responseBody != NULL) {
                responseBody += 4;
                body_initialized = 1;
                len = responseBody - response;
                if (fwrite(responseBody, sizeof(char), bytes - len, file2) != bytes - len) {
                    perror("error writing in the file");
                    close(fd);
                    free(port);
                    for (int j = 0; j < path_num; j++) {
                        free(path[j]);
                    }
                    free(path);
                    fclose(file2);
                    free(all_path);
                    exit(0);
                }
            }
        }
        for(int i=0;i< 4095;i++){
            response[i]='\0';
        }
        while((bytes = read(fd, response,4095))>0){
            response[bytes]='\0';
            printf("%s",response);
            responseLength+= bytes;
            if(okResponse == 1) {
                if (body_initialized == 1) {
                    if (fwrite(response, sizeof(char), bytes, file2) != bytes) {
                        perror("error opening the file");
                        close(fd);
                        free(port);
                        for (int j = 0; j < path_num; j++) {
                            free(path[j]);
                        }
                        free(path);
                        fclose(file2);
                        free(all_path);
                        exit(0);
                    }
                } else {
                    responseBody = (unsigned char *) strstr(response, "\r\n\r\n");
                    if (responseBody != NULL) {
                        responseBody += 4;
                        body_initialized = 1;
                        len = responseBody - response;
                        if (fwrite(responseBody, sizeof(char), bytes - len, file2) != bytes - len) {
                            perror("error writing in the file");
                            close(fd);
                            free(port);
                            for (int j = 0; j < path_num; j++) {
                                free(path[j]);
                            }
                            free(path);
                            fclose(file2);
                            free(all_path);
                            exit(0);
                        }
                    }
                }
            }
            for(int i=0;i< 4095;i++){
                response[i]='\0';
            }
        }
        printf("\n Total response bytes: %d\n", responseLength);
        if(okResponse == 1) {
            fclose(file2);
            if(argc == 3){
                char x[strlen(all_path)+128];
                strcpy(x,"firefox ");
                strcat(x, all_path);
                if(system(x)<0){
                    perror("system");
                }
            }
        }
        close(fd);
    }else{
        printf("File is given from local filesystem\n");
        int fileSize;
        int size;
        unsigned char* y = (char*) malloc(sizeof (char)*128);
        strcpy(y,"\0");
        fseek(file,0,SEEK_END);
        fileSize = ftell(file);
        fseek(file,0,SEEK_SET);
        unsigned char* body = (char*) malloc(sizeof (char)*fileSize+1);
        fread(body, 1, fileSize, file);
        body[fileSize]= '\0';
        unsigned char* x = (char*) malloc(sizeof (char)*fileSize+128);
        strcpy(x, "HTTP/1.0 200 OK\r\nContent-Length: ");
		x[strlen(x)]='\0';
        sprintf(y,"%d",fileSize);
        y[strlen(y)]='\0';
        strcat(x, y);
        strcat(x,"\r\n\r\n");
        size = strlen(x)+fileSize;
        strcat(x,body);
        printf("%s",x);
        printf("\n Total response bytes: %d\n", size);
        if(argc == 3){
            char x[strlen(all_path)+128];
            strcpy(x,"firefox ");
            strcat(x,all_path);
            if(system(x)<0){
                perror("system");
            }
        }
        free(x);
        free(y);
        fclose(file);
        free(body);
    }
    free(port);
    for(int j=0; j<path_num; j++){
        free(path[j]);
    }
    free(path);
    free(all_path);
}




