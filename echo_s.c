#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <netinet/in.h>
#include "logging.h"
#define BUFFER_SIZE 1024
#define ATTEMPTS 10

static int PORT = 9876;
int LOG_LEVEL = 3;

enum http_request_type {
	GET = 0,
	POST = 1,
	UNDEFINED = -1
};

enum http_response_status {
	INVALID_REQ = 400,
	INVALID_URI = 404, 
	OK_REQ = 200, 
};

struct http_request {
	enum http_request_type request_type;
	char* request_uri;
	char* request_protocol;
};

struct sockaddr_in getSocketAddr() {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT);
	return addr;
};

enum http_request_type getRequestType(char* str) {
	if (strcmp(str, "GET") == 0) return GET;
	if (strcmp(str, "POST") == 0) return POST;
	return UNDEFINED;
}

struct http_request arrToRequest(
	char* arr[], 
	size_t arr_len
) {
	struct http_request request;
	
	if (arr_len != 3) {
		free(arr[0]);
		return request;
	}

	request.request_type = getRequestType(arr[0]);
	request.request_uri = arr[1];
	request.request_protocol = arr[2];
	
	free(arr[0]);

	return request;
}

struct http_request bufToRequest(
	int buf_len, 
	char buf[],
	int buf_size
) {
	int part_count = 1;

	for (int i = 0; i < buf_size; i++) {
		if (buf[i] == ' ' || buf[i] == '\0') {
			part_count++;
		}
	}

	if (part_count != 3) {
		struct http_request empty = {
			UNDEFINED,
			NULL,
			NULL
		};
		return empty;
	}

	char* parts[part_count];
	int part_index = 0;
	int marker = 0;

	for (int i = 0; i < buf_size; i++) {
		if (buf[i] == ' ' || buf[i] == '\0' || i == buf_size - 1) {
			int part_size = i - marker;
			char* part = malloc(part_size + 1);

			if (part == NULL) {
				perror("malloc failed");
				exit(EXIT_FAILURE);
			}

    			strncpy(part, buf + marker, part_size);
			part[part_size] = '\0';
			parts[part_index++] = part;
			marker = i + 1;
		}
	} 

	return arrToRequest(parts, part_count);
}

int sanitizeUri(char* loc) {
	regex_t regex;
	if (regcomp(&regex, "[a-zA-Z]+[1-9]\\.(jpg|html)", REG_EXTENDED) != 0) {
		return 0;
	}
	int regex_status = regexec(&regex, (const char*)loc, 0, NULL, 0);
	regfree(&regex);
	return regex_status == 0;
}

FILE* getFile(
	char* loc
) {
	return sanitizeUri(loc) == 0 
		? NULL 
		: fopen(loc, "r");
}

enum http_response_status handleRequest(
	struct http_request request,
	int connFd
) {
	if (request.request_type != GET || strcmp(request.request_protocol, "HTTP/1.0") != 0) {
		dprintf(connFd, "HTTP/1.0 400 Bad Request\r\n\r\n");
		free(request.request_uri);
		free(request.request_protocol);
		return INVALID_REQ;
	}

	FILE* file = getFile(request.request_uri);
	if (!file) {
		dprintf(connFd, "HTTP/1.0 404 Not Found\r\n\r\n");
		free(request.request_uri);
		free(request.request_protocol);
		return INVALID_URI;
	}

	char* file_ext = strrchr(request.request_uri, '.');
	char* content_type = file_ext && strcmp(file_ext, ".jpg") == 0
		? "image/jpeg"
		: "text/html";
	fseek(file, 0, SEEK_END);
	size_t content_len = ftell(file);
	fseek(file, 0, SEEK_SET);

	dprintf(connFd, "HTTP/1.1 200 OK\r\n");
    	dprintf(connFd, "Content-Length: %zu\r\n", content_len);
    	dprintf(connFd, "Content-Type: %s\r\n", content_type);
    	dprintf(connFd, "\r\n");

	char buf[BUFFER_SIZE];
	size_t bytesRead = 0;


	while ((bytesRead = fread(buf, 1, BUFFER_SIZE, file)) > 0) {
		if (write(connFd, buf, bytesRead) < 0) {
			fclose(file);
			free(request.request_uri);
			free(request.request_protocol);
			return INVALID_REQ;
		}
	}

	fclose(file);
	free(request.request_uri);
	free(request.request_protocol);

	return OK_REQ;
}

int processConnection(
	int connFd
) {
	char buf[BUFFER_SIZE];
	while (1) {
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
		
		struct http_request request = bufToRequest(BUFFER_SIZE, buf, bRead);
		if (request.request_type == UNDEFINED) {
			char *badRequestMsg = "HTTP/1.0 400 Bad Request\r\n\r\n";
			write(connFd, badRequestMsg, strlen(badRequestMsg));
			continue;
		}

		enum http_response_status response = handleRequest(request, connFd);

	}
	return 0;
}

void setLogLevel(
	int argc, 
	char* argv[]
) {
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
	int bindAttempt = 0;
	struct sockaddr_in addr = getSocketAddr();
	while (!binded && bindAttempt < ATTEMPTS) {
		listenFd = bind(socketFd, (struct sockaddr*)&addr, sizeof(addr));
		if (listenFd < 0) {
			ERROR "Bind Failed... Trying new port." ENDL;
			addr.sin_port = htons(++PORT);
			bindAttempt++;
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
