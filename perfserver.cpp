/**
 *File: perfserver.cpp
 *Description: Network Bandwidth Performance Monitoring server
 *Author: Bob Danani
 */

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

#define MAXDATASIZE 65536 // maximum datasize to receive
#define BACKLOG 10	 // how many pending connections the queue will hold

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int reuseAddress=1;
	char clientAddress[INET6_ADDRSTRLEN];
    
	char buff[ (MAXDATASIZE + 1)];
	int rv;
	int numbytes;
    
	if (argc != 2) {
		fprintf(stderr, "usage: server <portnum>\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP address

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(true) {
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
        
		inet_ntop(their_addr.ss_family,
        get_in_addr((struct sockaddr *)&their_addr), clientAddress, sizeof (clientAddress));
		printf("server: got connection from %s\n", clientAddress);
        
		if (!fork()) { // child process
			close(sockfd); // child process doesn't need the listener
            //just reflect the package back to the client so the client program can calculate latency time and network performance bandwidth
            while (true) {
                if ( (numbytes = recv(new_fd, buff, MAXDATASIZE, 0)) <= 0){
                    cout << "Numbytes received : " << numbytes << endl;
                    break;
                }
                if ( (numbytes = send(new_fd, buff, numbytes, 0)) <= 0){
                    cout << "Numbytes sent : " << numbytes << endl;
                    break;
                }
            }
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent process doesn't need this
	}

	return 0;
}

