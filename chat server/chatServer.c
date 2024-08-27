#include "chatServer.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/ioctl.h>

static int end_server = 0;

int fd = -1;

void clear_messages(conn_t* connection);

void intHandler(int SIG_INT) {
	/* use a flag to end_server to break the main loop */
	end_server = 1;
}

int main (int argc, char *argv[])
{
	
	signal(SIGINT, intHandler);
   
	conn_pool_t* pool = malloc(sizeof(conn_pool_t));
	initPool(pool);
	
	if(argc != 2){
		printf("Usage: server <port>\n");
		free(pool);
		exit(0);
	}
	for(int i=0 ; i < strlen(argv[1]); i++){
		if(!isdigit(argv[1][i])){
			printf("Usage: server <port>\n");
			free(pool);
			exit(0);
		}
	}
	int port = atoi(argv[1]);
	if(port < 1 || port > 65536){
		printf("Usage: server <port>\n");
		free(pool);
		exit(0);
	}
	/*************************************************************/
	/* Create an AF_INET stream socket to receive incoming      */
	/* connections on                                            */
	/*************************************************************/
	
	if((fd = socket(AF_INET, SOCK_STREAM, 0))<0){
		perror("socket");
		free(pool);
		exit(0);
	}
	pool->maxfd = fd;
   
	/*************************************************************/
	/* Set socket to be nonblocking. All of the sockets for      */
	/* the incoming connections will also be nonblocking since   */
	/* they will inherit that state from the listening socket.   */
	/*************************************************************/
	//ioctl(...);
	int on = 1; 
	int rc = ioctl(fd, (int)FIONBIO, (char *)&on);
	if(rc < 0){
		perror("ioctl");
		close(fd);
		free(pool);
		exit(0);
	}

	/*************************************************************/
	/* Bind the socket                                           */
	/*************************************************************/
	struct sockaddr_in srv;
	memset(&srv, 0, sizeof(srv));
	srv.sin_family = AF_INET;
	srv.sin_port = htons(port);
	srv.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(fd, (struct sockaddr*) &srv, sizeof(srv))<0){
		perror("bind");
		free(pool);
		close(fd);
		exit(0);
	}

	/*************************************************************/
	/* Set the listen back log                                   */
	/*************************************************************/
	if(listen(fd, 5)<0){
		perror("bind");
		free(pool);
		close(fd);
		exit(0);
	}

	/*************************************************************/
	/* Initialize fd_sets  			                             */
	/*************************************************************/
	FD_SET(fd, &(pool->ready_read_set));
	/*************************************************************/
	/* Loop waiting for incoming connects, for incoming data or  */
	/* to write data, on any of the connected sockets.           */
	/*************************************************************/
	char buffer[BUFFER_SIZE];
	do
	{
		/**********************************************************/
		/* Copy the master fd_set over to the working fd_set.     */
		/**********************************************************/
		pool->read_set = pool->ready_read_set;
		pool->write_set = pool->ready_write_set;
		/**********************************************************/
		/* Call select() 										  */
		/**********************************************************/
		printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
		pool->nready = select(pool->maxfd+1, &(pool->read_set), &(pool->write_set), 0, 0);
		if(pool->nready<=0){
			continue;
			//perror("select");
			//free(pool);
			//exit(0);
		}

		/**********************************************************/
		/* One or more descriptors are readable or writable.      */
		/* Need to determine which ones they are.                 */
		/**********************************************************/
		int max = pool->maxfd;
		for (int socketfd = 0; socketfd < max+1; socketfd++)
		{
			/* Each time a ready descriptor is found, one less has  */
			/* to be looked for.  This is being done so that we     */
			/* can stop looking at the working set once we have     */
			/* found all of the descriptors that were ready         */
				
			/*******************************************************/
			/* Check to see if this descriptor is ready for read   */
			/*******************************************************/
			if (FD_ISSET(socketfd, &(pool->read_set)))
			{
				pool->nready -=1;
				/***************************************************/
				/* A descriptor was found that was readable		   */
				/* if this is the listening socket, accept one      */
				/* incoming connection that is queued up on the     */
				/*  listening socket before we loop back and call   */
				/* select again. 						            */
				/****************************************************/
                //accept(...)
				if(fd == socketfd){
					struct sockaddr_in cli;
					int cli_len = sizeof(cli);	
					int fdNew = accept(socketfd, (struct sockaddr*) &cli, &cli_len);
					if(fdNew < 0){
						continue;
					}
					if(addConn(fdNew, pool)<0){
						continue;
					}
					printf("New incoming connection on sd %d\n", fdNew); 
				}

				/****************************************************/
				/* If this is not the listening socket, an 			*/
				/* existing connection must be readable				*/
				/* Receive incoming data his socket             */
				/****************************************************/
				//read(...)
				else{
					printf("Descriptor %d is readable\n", socketfd);
					int bytes = read(socketfd, buffer, BUFFER_SIZE-1);
					buffer[bytes] = '\0';	
					printf("%d bytes received from sd %d\n", (int)strlen(buffer), socketfd); 

				/* If the connection has been closed by client 		*/
                /* remove the connection (removeConn(...))    		*/
							
					if(bytes == 0){
						printf("Connection closed for sd %d\n",socketfd);
						removeConn(socketfd, pool); 
						printf("removing connection with sd %d \n", socketfd);
					}

				/**********************************************/
				/* Data was received, add msg to all other    */
				/* connectios					  			  */
				/**********************************************/
                	if(bytes > 0){
						if(addMsg(socketfd, buffer, bytes, pool)==-1){
							continue;
						}
					}
				}          
				
			} /* End of if (FD_ISSET()) */
			/*******************************************************/
			/* Check to see if this descriptor is ready for write  */
			/*******************************************************/
			if (FD_ISSET(socketfd, &(pool->write_set))) {
				pool->nready -= 1;
				/* try to write all msgs in queue to sd */
				if(writeToClient(socketfd, pool)<0){
					continue;
				}
		 	}
			if(pool->nready == 0){
				break;
			}

		 /*******************************************************/
		 
		 
      } /* End of loop through selectable descriptors */

   } while (end_server == 0);

	/*************************************************************/
	/* If we are here, Control-C was typed,						 */
	/* clean up all open connections					         */
	/*************************************************************/
	for(int i=0;i<pool->nr_conns;i++){
		conn_t* connections = pool->conn_head;
		pool->conn_head = connections->next;
		printf("removing connection with sd %d \n", connections->fd);
		clear_messages(connections);
		close(connections->fd);
		free(connections);
	}
	close(fd);
	FD_ZERO(&(pool->read_set));
	FD_ZERO(&(pool->ready_read_set));
	FD_ZERO(&(pool->ready_write_set));
	FD_ZERO(&(pool->write_set));
	free(pool);
	return 0;
}


int initPool(conn_pool_t* pool) {
	//initialized all fields
	pool->maxfd = 0;
	pool->nr_conns = 0;
	pool->nready = 0;
	pool->conn_head = NULL;
	FD_ZERO(&(pool->ready_write_set));
	FD_ZERO(&(pool->write_set));
	FD_ZERO(&(pool->read_set));
	FD_ZERO(&(pool->ready_read_set));
	return 0;
}

int addConn(int sd, conn_pool_t* pool) {
	/*
	 * 1. allocate connection and init fields
	 * 2. add connection to pool
	 * */
	conn_t* client = (conn_t*)malloc(sizeof(conn_t));
	if(client == NULL){
		return -1;
	}
	client->fd = sd;
	client->write_msg_head = NULL;
	client->write_msg_tail = NULL;
	client->prev = NULL;
	if(pool->conn_head == NULL){
		client->next = NULL;
		pool->conn_head = client;
	}else{
		pool->conn_head->prev = client;
		client->next = pool->conn_head;
		pool->conn_head = client;	
	}
	pool->nr_conns += 1;
	FD_CLR(sd, &(pool->read_set));
	FD_CLR(sd, &(pool->write_set));
	FD_CLR(sd, &(pool->ready_write_set));
	FD_SET(sd, &(pool->ready_read_set));
	if(pool->maxfd < sd){
		pool->maxfd = sd;
	}
	return 0;
}

int maxFd(conn_pool_t* pool){
	conn_t* connections = pool->conn_head;
	int max = 0;
	if(pool->nr_conns == 0){
		max = fd;
	}
	for(int i=0; i<pool->nr_conns; i++){
		if(connections->fd > max){
			max = connections->fd;
		}
		connections = connections->next;
	}
	return max;
}

void clear_messages(conn_t* connection){
	if(connection->write_msg_tail == NULL){
		return;
	}
	msg_t* message = connection->write_msg_tail;
	msg_t* previous_message = connection->write_msg_tail;
	while(1){
		if(message == NULL){
			break;
		}
		previous_message = message;
		message = message->next;
		free(previous_message->message);
		free(previous_message);
	}
}

int removeConn(int sd, conn_pool_t* pool) {
	/*
	* 1. remove connection from pool 
	* 2. deallocate connection 
	* 3. remove from sets 
	* 4. update max_fd if needed 
	*/
	conn_t* connections = pool->conn_head;
	for(int i=0; i<pool->nr_conns; i++){
		if(connections->fd != sd){
			connections = connections->next;
			continue;
		}
		if(connections->prev != NULL){
			connections->prev->next = connections->next;
		}else{
			pool->conn_head = connections->next;
		}
		FD_CLR(sd, &(pool->ready_read_set));
		FD_CLR(sd, &(pool->ready_write_set));
		clear_messages(connections);
		free(connections);
		close(sd);
		break;
	}
	pool->nr_conns -= 1;
	if(pool->maxfd == sd){
		pool->maxfd = maxFd(pool);
	} 
	return 0;
}

int addMsg(int sd,char* buffer,int len,conn_pool_t* pool) {
	
	/*
	 * 1. add msg_t to write queue of all other connections 
	 * 2. set each fd to check if ready to write 
	 */
	conn_t* connections = pool->conn_head; 
	for(int i=0; i<pool->nr_conns; i++){
		if(connections->fd != sd){
			msg_t* newMessage = (msg_t*)malloc(sizeof(msg_t));
			if(newMessage == NULL){
				return -1;
			}
			newMessage->message = (char*)malloc(sizeof (char)*len+1);
			strcpy(newMessage->message, "\0");
			strcpy(newMessage->message, buffer);
			newMessage->size = len;
			newMessage->prev = NULL;
			if(connections->write_msg_head == NULL){
				connections->write_msg_head = newMessage;
				connections->write_msg_tail = newMessage;
				newMessage->next = NULL;
			}else{
				connections->write_msg_head->prev = newMessage;
				newMessage->next = connections->write_msg_head;	
				connections->write_msg_head = newMessage;
			}
			FD_SET(connections->fd, &(pool->ready_write_set));
		}
		connections = connections->next;
	}
	return 0;
}

char* toCapital(msg_t* messages){
	char* capital = (char*)malloc(sizeof(char)*messages->size);
	strcpy(capital, "\0");
	for(int i=0;i<messages->size;i++){
		if(messages->message[i] >= 97 && messages->message[i] <= 122){
			capital[i] = (messages->message[i])-32;
		}else{
			capital[i] = messages->message[i];
		}
	}
	return capital;
}

int writeToClient(int sd,conn_pool_t* pool) {
	
	/*
	 * 1. write all msgs in queue 
	 * 2. deallocate each writen msg 
	 * 3. if all msgs were writen successfully, there is nothing else to write to this fd... */
	conn_t* connections = pool->conn_head;
	for(int i=0; i<pool->nr_conns; i++){
		if(connections->fd != sd){
			connections = connections->next;
			continue;
		}
		msg_t* messages = connections->write_msg_tail;
		msg_t* prevMessages = connections->write_msg_tail;
		char* capital;
		while(1){
			if(messages == NULL){
				FD_CLR(sd, &(pool->ready_write_set));
				connections->write_msg_head = NULL;
				connections->write_msg_tail = NULL;
				return 0; 
			}
			capital = toCapital(messages);
			if(write(sd, capital, messages->size)<0){
				return  -1;
			}
			prevMessages = messages;
			messages = messages->prev;
//			free(prevMessages->message);
			free(prevMessages->message);
			free(capital);
			free(prevMessages);
		}
	}
	return 0;
}


