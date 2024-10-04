#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include "logging.h"
#define BUFFER_SIZE 1024

static int PORT = 9876;
int LOG_LEVEL = 3;

struct sockaddr_in getSocketAddr() {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT);
	return addr;
}

int read_prefix(int buf_len, char buf[]) {
	char* result = malloc(5);
	int status = 0;
	if (buf_len) {
		strncpy(result, buf, 4);
		if (strncmp(result, "QUIT", 4) == 0) {
			status = -1;
			TRACE "QUIT Command Executed" ENDL;
		}
		else {
			strncpy(result, buf, 5);
			if (strncmp(result, "CLOSE", 5) == 0) {
				status = 1;
				TRACE "CLOSE Command Executed" ENDL;
			}
		}
	}
	free(result);
	return status;
}

int processConnection(int connFd) {
	char buf[BUFFER_SIZE];
	int process = 1;
	while (process) {
		TRACE "Awaiting Block" ENDL;
		int bRead = read(connFd, buf, BUFFER_SIZE);
		if (bRead < 1) {
			if (bRead < 0) {
				printf("read() failed: %s\n", strerror(errno));
				exit(-1);
			}
			TRACE "Connection closed unexpectedly" ENDL;
			return 1;
		}
		TRACE "Block Received" ENDL;
		int prefix_status = read_prefix(BUFFER_SIZE, buf);
		if (prefix_status == -1) return 1;
		if (prefix_status == 1) break;
		write(connFd, buf, bRead);
	}
	return 0;
}

void setLogLevel(int argc, char* argv[]) {
	int opt = 0;
	while ((opt = getopt(argc, argv, "d: ")) != -1) {
		switch (opt) {
			case 'd':
				LOG_LEVEL = atoi(optarg);
				break;
			case ':': case '?': default:
			       printf("usage: %s -d <num>\n", argv[0]);
			       exit(-1);
			       break;
		}
	}
}

int bindSocket(int socketFd) {
	int binded = 0;
	int listenFd = -1;
	struct sockaddr_in addr = getSocketAddr();
	while (!binded) {
		listenFd = bind(socketFd, (struct sockaddr*)&addr, sizeof(addr));
		if (listenFd < 0) {
			ERROR "Bind Failed... Trying new port." ENDL;
			addr.sin_port = htons(++PORT);
			continue;
		}
		binded = 1;
		TRACE "Socket Bound" ENDL;
	}
	return listenFd;
}

int createSocket() {
	int socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketFd < 0) {
		printf("socket() failed: %s\n", strerror(errno));
		exit(-1);
	} 
	TRACE "Socket Open" ENDL;
	return socketFd;
}

int listenSocket(int socketFd) {
	int listenQueueCount = 1;
	int listen_res = listen(socketFd, listenQueueCount);
	if (listen_res < 0) {
		printf("listen() failed: %s\n", strerror(errno));
		exit(-1);
	}
	return listenQueueCount;
}

void runProgram(int socketFd, int listenQueueCount) {
	int quit = 0; 
	while (!quit) {
		TRACE "Awaiting Connection" ENDL;
		int connectionFd = accept(socketFd, (struct sockaddr*)NULL, NULL);
		if (connectionFd < 0) {
			printf("accept() failed: %s", strerror(errno));
			exit(-1);
		}
		quit = processConnection(connectionFd);
		close(connectionFd);
	}
}	

int main(int argc, char* argv[]) {
	setLogLevel(argc, argv);
	int socketFd = createSocket(PORT);
	int listenFd = bindSocket(socketFd);
	printf("Using port: %d\n", PORT);
	int listenQueueCount = listenSocket(socketFd);
	runProgram(socketFd, listenQueueCount);
	close(listenFd);
	return 0;
}
