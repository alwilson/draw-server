/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>
#include <sys/poll.h>

#include "ws_core.h"

#define MAX_CONN 256
typedef struct thread_table
{
	int id;
	int clientfd;
	char position[20];
	pthread_t thread;
} thread_table_t;

static thread_table_t thread_table[MAX_CONN];
static int num_threads;
static pthread_mutex_t num_threads_l = PTHREAD_MUTEX_INITIALIZER;

void error(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void *client_thread(void *thread_data_ptr)
{
	thread_table_t *thread_data = (thread_table_t*)thread_data_ptr;
	int sockfd = thread_data->clientfd;
	int n;
	char *buffer = (char*)calloc(25600, sizeof(char));
	bzero(buffer, 25600);
	n = read(sockfd,buffer,25600);
	if (n <= 0) {perror("ERROR writing to socket"); goto error0;}
	//printf("=Message Start=\n%s\n=Message End=\n", buffer);
	char *substring = strstr(buffer, "Sec-WebSocket-Key: ");
	char client_key[25600];
	char handshake[25600];
	int client_key_size = 0;
	if (substring != NULL) {
		substring += sizeof("Sec-WebSocket-Key: ")-1;
		//printf("Sec-WebSocket-Key is ");
		while (*substring != '\0' && *substring != '\n' && 
		       *substring != '\r') {
			client_key[client_key_size++] = *substring;
			substring++;
		}
		client_key[client_key_size] = '\0';
		//printf("%s\n", client_key);
	}

	char special[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	strcat(client_key, special);
	client_key_size += strlen(special);
	//printf("%s\n", client_key);
	SHA1(client_key, client_key_size, handshake);
	char *handshake64 = base64(handshake, 20);
	//printf("Base64: *%s*\n", handshake64);

	const char *response = "HTTP/1.1 101 Switching Protocols\r\n"
			       "Upgrade: websocket\r\n"
			       "Connection: Upgrade\r\n"
			       "Sec-WebSocket-Accept: ";
	n = write(sockfd,response,strlen(response));
	if (n <= 0) {perror("ERROR writing to socket"); goto error0;}
	n = write(sockfd,handshake64,strlen(handshake64));
	if (n <= 0) {perror("ERROR writing to socket"); goto error0;}
	n = write(sockfd,"\r\n\r\n",strlen("\r\n\r\n"));
	if (n <= 0) {perror("ERROR writing to socket"); goto error0;}
	free(handshake64);
	free(buffer);

	struct pollfd poll_fd[1];
	poll_fd[0].fd = sockfd;
	poll_fd[0].events = POLLIN;
	int rv;

	char *recv_buf = (char*)calloc(1024, sizeof(char));
	char *send_buf = (char*)calloc(1024, sizeof(char));
	int stillalive = 1;
	while (stillalive) {
		//printf("waiting for message\n");
		bzero(recv_buf, 1024);
		rv = poll(poll_fd, 1, 1);
		if (rv > 0 && poll_fd[0].revents & POLLIN) {
			n = read(sockfd,recv_buf,1024);
			//printf("%i bytes read\n", n);
			if (n <= 0) {perror("ERROR writing to socket"); goto error1;}

			int opcode = 0x0F & recv_buf[0];
			int mask_b = 0x80 & recv_buf[1];
			int msg_size = 0x7F & recv_buf[1];
			char *mask = &recv_buf[2];
			char *data = &recv_buf[6];
			// Decode masked data
			int i;
			for (i = 0; i < msg_size; i++)
				data[i] ^= mask[i%4];
			for (i = 0; i < msg_size; i++)
				thread_data->position[i] = data[i];
			thread_data->position[i] = '\0';
		}

		int x;
		for (x = 0; x < MAX_CONN; x++) {
			if (thread_table[x].id != -1 && thread_table[x].id != thread_data->id) {
				bzero(send_buf, 1024);
				send_buf[0] = 0x81;
				send_buf[1] = 0x7F & strlen(thread_table[x].position);
				strcpy(&send_buf[2], thread_table[x].position);
				n = write(sockfd, send_buf, send_buf[1]+2);
				if (n <= 0) {perror("ERROR writing to socket"); goto error1;}
				
			}
		}
	}

error1:
	free(recv_buf);
	free(send_buf);

error0:
	close(sockfd);
	
	pthread_mutex_lock(&num_threads_l);
	num_threads--;
	pthread_mutex_unlock(&num_threads_l);

	printf("Thread #%d disconnecting\n", thread_data->id);
	printf("There are %d threads active\n", num_threads);
	thread_data->id = -1;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr,"usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Initialize socket on port
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
		sizeof(serv_addr)) < 0)  {
		close(sockfd);
		error("ERROR on binding");	
	}
	listen(sockfd,5);
	clilen = sizeof(cli_addr);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	num_threads = 0;

	int n;
	for (n = 0; n < MAX_CONN; n++)
		thread_table[n].id = -1;

	int ret_th;
	int next_thread;
	int stillalive = 1;
	while(stillalive) {
		printf("Waiting for client...\n");
		// TODO: Need way to find next thread_id
		for (n = 0; n < MAX_CONN; n++) {
			if (thread_table[n].id == -1) {
				next_thread = n;
				break;
			}
		}

		if (n == MAX_CONN) {
			fprintf(stderr,"Too many connections somehow!?\n");
			exit(EXIT_FAILURE);
		}

		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, 
		                   &clilen);
		if (newsockfd < 0) {
			perror("ERROR on accept");
			goto error0;
		}
		thread_table[next_thread].clientfd = newsockfd;
		thread_table[next_thread].id = next_thread;
		strcpy(thread_table[next_thread].position, "0,0");
		
		ret_th = pthread_create(&thread_table[next_thread].thread, 
					    &attr, client_thread, 
					    (void*)&thread_table[next_thread]);
		if (ret_th) {
			fprintf(stderr, "pthread_create failed with %d\n",
				ret_th);
			goto error1;
		}

		pthread_mutex_lock(&num_threads_l);
		num_threads++;
		pthread_mutex_unlock(&num_threads_l);
		printf("Thread #%d is up\n", next_thread);
		printf("There are %d threads active\n", num_threads);

		while (num_threads >= MAX_CONN) {
			printf("Hit MAX_CONN!!!\n");
			sleep(1);
		}
	}
	exit(EXIT_SUCCESS);
error1:
	close(newsockfd);
error0:
	close(sockfd);
	exit(EXIT_FAILURE);
}

