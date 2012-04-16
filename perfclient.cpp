/**
 *File: perfclient.cpp
 *Description: Network Bandwdith Performance Monitoring client
 *Author: Bob Danani
*/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <ctime>

using namespace std;

int datasize = 1472; // ETHERNET MTU
ofstream out;
int sockfd;
bool stopLoop = false;
char *sendbuff; // holds bytes to send
char *recvbuff; // holds bytes to receive


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void my_handler(int s){
    stopLoop = true;
    out.close();
    close(sockfd);
}

int main(int argc, char *argv[])
{
    out.open("perfoutput.txt", ios::out);
    
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL); // handle interrupt (Ctrl-C)
    
	int numbytes;  
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
    
    int numberOfLoop = 10000; 
    
    double timeInMilliSecond = 0; // one way average transmission time
    long starttime = 0, stoptime = 0;
    
    double megabitsPerSecond = 0.0; // bandwidth in megabits/sec
    double bytesPerMilliSecond = 0.0; // bandwitdh in bytes/milisec
    double megabits = 0.0; //total of bits sent/received
    double timeInSecond = 0;
    
    double runningBandwidthAverage;
    double runningBandwidthTotal;
    
    int iterationCounter = 1;
    
	if (argc  < 3) {
	    fprintf(stderr,"usage: client <hostname> <portnum>  OR client <hostname> <portnum> <datasize>\n");
	    exit(1);
	}
    
    if (argc == 4){
        datasize = atoi (argv[3]);
    }

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	fprintf(stderr, "host=%s, port=%s\n", argv[1], argv[2]);
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);
    printf("client: Type 'ctrl+c (or cmd+c for mac)' to disconnect from server\n");

	freeaddrinfo(servinfo); // all done with this structure
    
    sendbuff = (char*) malloc (  ((datasize * 2) + 1) * sizeof (char)); 
    recvbuff = (char*) malloc (  ((datasize * 2) + 1) * sizeof (char));
    
    while (!stopLoop){       
        cout << "Iteration : " << (iterationCounter) << "\n";
        out << "Iteration : " << (iterationCounter) << "\n";
        out << "Bytes Length\t1-way transmission time (ms)\tBandwidth (Mbps)\tRunning Bandwidth Average (Mbps)\n";
        
        starttime = clock(); // start timer
        //do send/recv data for "numberOfLoop" times
        for (int i = 0; i < numberOfLoop; ++i){
            //this is a 2-way transmission (send, and recv)
            if ( (numbytes = send(sockfd, sendbuff, datasize, 0)) <=0) { // send bytes
                stopLoop = true;
                break;
            }
            if ( (numbytes = recv(sockfd, recvbuff, datasize, 0)) <= 0){ // receive bytes
                stopLoop = true;
                break;
            }
        }
        stoptime = clock(); // stop timer
        if (stopLoop) 
            break;
        
        timeInSecond =  (double) (stoptime - starttime) / (double) CLOCKS_PER_SEC;
        
        //divide the time by 2, to calculate one way transmission time
        timeInSecond /=2;
        //divide the time by numberOfLoop, to calculate the average 1-way transmission time 
        timeInSecond /= numberOfLoop;
        
        timeInMilliSecond = timeInSecond * 1000.0;
        bytesPerMilliSecond = datasize / timeInMilliSecond; // bandwidth in bytes / milliseconds
        
        megabits = (datasize * 8) / (double)(1024 * 1024); // number of bits sent (in Megabits)
        megabitsPerSecond = megabits / timeInSecond; // bandwitdh in megabits per second
        
        runningBandwidthTotal += megabitsPerSecond;
        runningBandwidthAverage = runningBandwidthTotal / (iterationCounter);
        
        out << datasize;
        out << "\t\t";
        out << timeInMilliSecond;
        out << "\t\t\t";
        out << megabitsPerSecond;
        out << "\t\t\t";
        out << runningBandwidthAverage;
        out << "\n";

        iterationCounter++;    
    }
    
    free(sendbuff);
    free(recvbuff);
    out.close();
    close(sockfd);
    
    printf("\nclient: Done...\n");

	return 0;
}

