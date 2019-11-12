#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#define BUFFER 512

int main(int argc, char* argv[]) {
	// Check for valid arguments
	if(argc != 7) {
		fprintf(stderr, "ERROR: Invalid Arguments/Invalid Number of Arguments\n");
		return EXIT_FAILURE;
	}

	setvbuf(stdout, NULL, _IONBF, 0);
	int i;
	int j;
	int k;

	// Assigning arguments to variables
	char controlHost[BUFFER];
	strcpy(controlHost, argv[1]);
	unsigned short controlPort = atoi(argv[2]);
	int sensorID = atoi(argv[3]);
	int sensorRange = atoi(argv[4]);
	int xPosition = atoi(argv[5]);
	int yPosition = atoi(argv[6]);

	// Client-side initialization
	int sd = socket(PF_INET, SOCK_STREAM, 0);
	if(sd < 0) {
		fprintf(stderr, "ERROR: socket creation failed!\n");
		return EXIT_FAILURE;
	}

	struct hostent * host = gethostbyname(controlHost);
	if(host == NULL) {
		fprintf(stderr, "ERROR: gethostbyname() failed!\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(controlPort);
	server.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server.sin_zero), 8);

	if(connect(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "ERROR: Could not connect to server!\n");
		return EXIT_FAILURE;
	}

	while(1) {
		char command[BUFFER];

		//
		printf("Please enter a command: ");
		scanf("%s", command);

		char* token = strtok(command, " ");
		if(strcmp(token, "MOVE")) {

		}

		else if(strcmp(token, "SENDDATA")) {

		}

		else if (strcmp(token, "QUIT")) {
			close(sd);
			break;
		}
	}

	return EXIT_SUCCESS;
}