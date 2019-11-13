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

	// Get the host
	struct hostent * host = gethostbyname(controlHost);
	if(host == NULL) {
		fprintf(stderr, "ERROR: gethostbyname() failed!\n");
		return EXIT_FAILURE;
	}

	// Initialize server settings
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(controlPort);
	server.sin_addr = *((struct in_addr *)host->h_addr);
	bzero(&(server.sin_zero), 8);

	// Connect to the server
	if(connect(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "ERROR: Could not connect to server!\n");
		return EXIT_FAILURE;
	}

	while(1) {
		char command[BUFFER];
		char message[BUFFER];
		char buffer[BUFFER];

		// Wait for user to enter a command
		printf("Please enter a command: ");
		scanf("%s", command);

		// If command is MOVE
		// -- send update position message from client to server, and client waits for response
		// If command is SENDDATA
		// -- send new message for indicated client to the server and wait for response
		// IF command is QUIT
		// -- clean out memory and exit the program
		char* token = strtok(command, " ");
		if(strcmp(token, "MOVE")) {

			// Update the X coordinate position and Y coordinate position
			int value = 0;
			while(token != NULL) {
				if(value = 0) {
					xPosition = atoi(token);
					value = 1;
				}
				else {
					yPosition = atoi(token);
				}
				token = strtok(NULL, " ");
			}

			// Create the message that needs to be sent to the control server
			sprintf(message, "UPDATEPOSITION %d %d %d %d", sensorID, sensorRange, 
				xPosition, yPosition);

			// Send the updated position to the control server
			int bytes = send(sd, message, strlen(message), 0);
			if(bytes < strlen(message)) {
				fprintf(stderr, "ERROR: Could not send update position message to server!\n");
				return EXIT_FAILURE;
			}

			bytes = recv(sd, buffer, BUFFER, 0);

			if(bytes < 0) {
				fprintf(stderr, "ERROR: Could not receive update position response from server!\n");
				return EXIT_FAILURE;
			}
			else if(bytes == 0) {
				printf("Received no data. Server socket seems to have closed!\n");
			}
			else {
				buffer[bytes] = '\0';
				printf("Received from server: %s\n", buffer);
			}
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