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
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <math.h>

#define BUFFER 512

typedef struct {
	char reachableID[BUFFER];
	int xPosition;
	int yPosition;
	float distance;
} ReachableList;

int numReachable = 0;
ReachableList* reachables;
int reachableCounter = 0;

int clientsd = 0;
char clientSensorID[BUFFER];
int clientSensorRange = 0;
int clientXPosition = 0;
int clientYPosition = 0;

pthread_mutex_t lock;

float getDistance(float x1, float y1, float x2, float y2);
void updatePosition(int sd, char* sensorID, int sensoryRange, int xPosition, int yPosition);
void* childThread(void* someArgument);

float getDistance(float x1, float y1, float x2, float y2){
	return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

// Function that updates the CONTROL server on the new position of the SENSOR
void updatePosition(int sd, char* sensorID, int sensorRange, int xPosition, int yPosition) {

	// Lock the mutex so that there is no conflict between multiple updatePosition calls
	pthread_mutex_lock(&lock);

	char message[BUFFER];
	char buffer[BUFFER];

	int i;

	// Create the message that needs to be sent to the control server
	sprintf(message, "UPDATEPOSITION %s %d %d %d", sensorID, sensorRange, 
		xPosition, yPosition);

	// Send the UPDATEDPOSITION to the control server
	int bytes = send(sd, message, strlen(message), 0);
	if(bytes < strlen(message)) {
		fprintf(stderr, "ERROR: Could not send UPDATEPOSITION message to server!\n");
		return exit(1);
	}

	// Should receive a REACHABLE message from the server
	bytes = recv(sd, buffer, BUFFER, 0);

	if(bytes < 0) {
		fprintf(stderr, "ERROR: Could not receive UPDATEPOSITION response from server!\n");
		return exit(1);
	}
	else if(bytes == 0) {
		printf("Received no data. Server socket seems to have closed!\n");
	}
	else {
		buffer[bytes] = '\0';
		printf("Received from server: %s\n", buffer);

		// Receive the REACHABLE message from the server after UPDATEPOSITION message was sent
		// Store all reachable points in global list of structs
		char* token = strtok(buffer, " ");
		if(strcmp(token, "REACHABLE") == 0) {
			token = strtok(NULL, " ");

			numReachable = atoi(token);
			int value = 0;
			ReachableList newEntry;

			reachables = (ReachableList*)calloc(numReachable, sizeof(ReachableList));

			token = strtok(NULL, " ");
			while(token != NULL) {
				if(value == 0) {
					strcpy(newEntry.reachableID, token);
					value = 1;
				}
				else if(value == 1) {
					newEntry.xPosition = atoi(token);
					value = 2;
				}
				else {
					newEntry.yPosition = atoi(token);
					value = 3;
				}
				if(value == 3) {
					// Find the distance between the current client and the sensor/base
					float distance = getDistance(xPosition, yPosition, newEntry.xPosition, 
						newEntry.yPosition);

					newEntry.distance = distance;
					reachables[reachableCounter++] = newEntry;
					value = 0;
				}
				token = strtok(NULL, " ");
			}

			// Reset the reachable list counter
			reachableCounter = 0;

			// Debugging print statement
			printf("Reachables:\n");
			for(i=0; i<numReachable; i++) {
				printf("\t%s %d %d\n", reachables[i].reachableID, reachables[i].xPosition,
					reachables[i].yPosition);
			}
		}
		else {
			fprintf(stderr, "ERROR: Did not receive REACHABLE message from server!\n");
			exit(1);
		}
	}

	// Unlock the mutex
	pthread_mutex_unlock(&lock);
}

void* childThread(void* someArgument) {

	int i;
	int j;

	while(1) {
		char buffer[BUFFER];

		// Wait on DATAMESSAGE from the server
		int bytes = recv(clientsd, buffer, BUFFER, 0);
		if(bytes < 0) {
			fprintf(stderr, "ERROR: Could not receive UPDATEPOSITION response from server!\n");
			exit(1);
		}
		else if(bytes == 0) {
			printf("Received no data. Server socket seems to have closed!\n");
			exit(1);
		}
		else {
			buffer[bytes] = '\0';
			printf("Received from server: %s\n", buffer);
		}

		// Create the originID string and the destinationID string
		char originID[BUFFER];
		char nextID[BUFFER];
		char destinationID[BUFFER];
		int hopReachable = 0;
		ReachableList* hopList;
		int hopListCounter = 0;

		char newHopList[BUFFER];
		strcpy(newHopList, "");

		char* token = strtok(buffer, " ");
		if(strcmp(token, "DATAMESSAGE") == 0) {
			token = strtok(NULL, " ");
			int value = 0;

			while(token != NULL) {
				if(value == 0) {
					strcpy(originID, token);
					value = 1;
				}
				else if(value == 1) {
					strcpy(nextID, token);
					value = 2;
				}
				else if(value == 2) {
					strcpy(destinationID, token);
					value = 3;
				}
				else if(value == 3) {
					hopReachable = atoi(token);
					hopList = (ReachableList *)calloc(hopReachable, sizeof(ReachableList));
					value = 4;
				}
				else {
					token = strtok(NULL, " ");
					int newValue = 0;

					ReachableList newHopEntry;

					while(token != NULL){
						if(newValue == 0) {
							strcpy(newHopEntry.reachableID, token);
							strcat(newHopList, token);
							newValue = 1;
						}
						else if(newValue == 1) {
							newHopEntry.xPosition = atoi(token);
							strcat(newHopList, " ");
							strcat(newHopList, token);
							newValue = 2;
						}
						else {
							newHopEntry.yPosition = atoi(token);
							strcat(newHopList, " ");
							strcat(newHopList, token);
							newValue = 3;
						}
						if(newValue == 3) {
							strcat(newHopList, " ");
							hopList[hopListCounter++] = newHopEntry;
							newValue = 0;
						}
						token = strtok(NULL, " ");
					}	
					value = 5;
				}
				token = strtok(NULL, " ");
			}

			if(strcmp(destinationID, clientSensorID) == 0) {
				printf("%s: Message from %s to %s successfully received.\n", clientSensorID, 
					originID, destinationID);
			}

			else {
				// Check if all reachable sensors/base stations are already in hop list
				int allReachable = 0;
				for(i=0; i<numReachable; i++) {
					for(j=0; j<hopReachable; j++) {
						if(strcmp(reachables[i].reachableID, hopList[j].reachableID) == 0) {
							allReachable++;
							break;
						}
					}
				}

				// If all sensors/base stations are in hop list, message could not be delivered
				// Else, deliver to the NextID
				if(allReachable == numReachable) {
					printf("%s: Message from %s to %s could not be delivered.\n", clientSensorID, originID, destinationID);
				}
				else {
					updatePosition(clientsd, clientSensorID, clientSensorRange, clientXPosition, clientYPosition);

					// Implementing the WHERE message
					char someID[BUFFER];
					char message[BUFFER];
					strcpy(someID, destinationID);

					sprintf(message, "WHERE %s", someID);

					// Send the WHERE message to the control server
					int bytes = send(clientsd, message, strlen(message), 0);
					if(bytes < strlen(message)) {
						fprintf(stderr, "ERROR: Could not send update position message to server!\n");
						exit(1);
					}

					// Should receive a THERE message from the server
					bytes = recv(clientsd, buffer, BUFFER, 0);

					if(bytes < 0) {
						fprintf(stderr, "ERROR: Could not receive update position response from server!\n");
						exit(1);
					}
					else if(bytes == 0) {
						printf("Received no data. Server socket seems to have closed!\n");
					}
					else {
						buffer[bytes] = '\0';
						printf("Received from server: %s\n", buffer);
					}

					// Find the distances for all of the reachable bases/sensors
					// Make sure that the sensor/base is not already part of the hoplist
					// To prevent infinite loops
					float minDistance = INFINITY;
					int nextXPosition = 0;
					int nextYPosition = 0;
					char closest[BUFFER];
					for(i=0; i<numReachable; i++) {
						if(reachables[i].distance < minDistance) {
							int found = 0;

							for(j=0; j<hopReachable; j++) {
								if(strcmp(reachables[i].reachableID, hopList[j].reachableID) == 0) {
									found = 1;
								}
							}

							if(!found) {
								minDistance = reachables[i].distance;
								strcpy(closest, reachables[i].reachableID);
								nextXPosition = reachables[i].xPosition;
								nextYPosition = reachables[i].yPosition;
							}
							else {
								continue;
							}
						}
					}

					hopReachable++;
					strcat(newHopList, closest);
					sprintf(newHopList, "%s %d", newHopList, nextXPosition);
					sprintf(newHopList, "%s %d", newHopList, nextYPosition);

					printf("%s: Message from %s to %s being forwarded through %s\n", clientSensorID, 
						originID, destinationID, closest);

					// Send message to the CONTROL server
					// Create the message that needs to be sent to the control server
					sprintf(message, "DATAMESSAGE %s %s %s %d %s", originID, closest, destinationID, 
						hopReachable, newHopList);

					// Send the DATAMESSAGE to the server
					bytes = send(clientsd, message, strlen(message), 0);
					if(bytes < strlen(message)) {
						fprintf(stderr, "ERROR: Could not send update position message to server!\n");
						exit(1);
					}

				}
			}

			// Free the hop list
			free(hopList);
		}
	}
}

int main(int argc, char* argv[]) {
	// Check for valid arguments
	if(argc != 7) {
		fprintf(stderr, "ERROR: Invalid Arguments/Invalid Number of Arguments\n");
		return EXIT_FAILURE;
	}

	setvbuf(stdout, NULL, _IONBF, 0);
	int i;

	if(pthread_mutex_init(&lock, NULL) != 0) {
		fprintf(stderr, "ERROR: Could initiate pthread mutex!\n");
		return EXIT_FAILURE;
	}

	// Assigning arguments to variables
	char controlHost[BUFFER];
	strcpy(controlHost, argv[1]);
	unsigned short controlPort = atoi(argv[2]);
	char sensorID[BUFFER];
	strcpy(sensorID, argv[3]);
	int sensorRange = atoi(argv[4]);
	int xPosition = atoi(argv[5]);
	int yPosition = atoi(argv[6]);

	// Client-side initialization
	int sd = socket(PF_INET, SOCK_STREAM, 0);
	if(sd < 0) {
		fprintf(stderr, "ERROR: socket creation failed!\n");
		return EXIT_FAILURE;
	}

	// Save information as global variables
	clientsd = sd;
	strcpy(clientSensorID, sensorID);
	clientSensorRange = sensorRange;
	clientXPosition = xPosition;
	clientYPosition = yPosition;

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

	// The initial UPDATEPOSITION message sent to the CONTROL SERVER
	updatePosition(sd, sensorID, sensorRange, xPosition, yPosition);

	// Use fork to create a child process
	// Parent --- handles the input commands from the user
	// Child --- handles the receiving of messages from the server
	pthread_t tid;

	int status = pthread_create(&tid, NULL, childThread, NULL);
	if(status != 0) {
		fprintf(stderr, "ERROR: Could not create pthread!\n");
		return EXIT_FAILURE;
	}

	while(1) {
		char command[BUFFER];
		char message[BUFFER];
		char buffer[BUFFER];

		// Wait for user to enter a command
		printf("Please enter a command: ");
		fgets(command, BUFFER, stdin);
		command[strlen(command)-1] = '\0';

		// If command is MOVE
		// -- send update position message from client to server, and client waits for response
		// If command is SENDDATA
		// -- send new message for indicated client to the server and wait for response
		// If command is WHERE
		// -- send a message to the server and ask for information of a sensor or base
		// IF command is QUIT
		// -- clean out memory and exit the program
		char* token = strtok(command, " ");
		if(strcmp(token, "MOVE") == 0) {

			// Update the X coordinate position and Y coordinate position
			int value = 0;
			token = strtok(NULL, " ");
			while(token != NULL) {
				if(value == 0) {
					xPosition = atoi(token);
					value = 1;
				}
				else {
					yPosition = atoi(token);
				}
				token = strtok(NULL, " ");
			}

			// Change global variables for position;
			clientXPosition = xPosition;
			clientYPosition = yPosition;

			updatePosition(sd, sensorID, sensorRange, xPosition, yPosition);
		}

		else if(strcmp(token, "SENDDATA") == 0) {
			token = strtok(NULL, " ");
			char destination[BUFFER];

			updatePosition(sd, sensorID, sensorRange, xPosition, yPosition);

			// Generate a new DATAMESSAGE with destination of DestinationID
			strcpy(destination, token);

			// Find the distances for all of the reachable bases/sensors
			float minDistance = INFINITY;
			char closest[BUFFER];
			for(i=0; i<numReachable; i++) {
				if(reachables[i].distance < minDistance) {
					minDistance = reachables[i].distance;
					strcpy(closest, reachables[i].reachableID);
				}
			}

			printf("Sent a new message bound for %s\n", destination);

			// Send message to the CONTROL server
			// Create the message that needs to be sent to the control server
			sprintf(message, "DATAMESSAGE %s %s %s 0", sensorID, closest, destination);

			// Send the DATAMESSAGE to the server
			int bytes = send(sd, message, strlen(message), 0);
			if(bytes < strlen(message)) {
				fprintf(stderr, "ERROR: Could not send UPDATEPOSITION message to server!\n");
				return EXIT_FAILURE;
			}
		}

		else if(strcmp(token, "WHERE") == 0) {
			token = strtok(NULL, " ");

			char someID[BUFFER];
			strcpy(someID, token);

			sprintf(message, "WHERE %s", someID);

			// Send the WHERE message to the control server
			int bytes = send(sd, message, strlen(message), 0);
			if(bytes < strlen(message)) {
				fprintf(stderr, "ERROR: Could not send UPDATEPOSITION message to server!\n");
				return EXIT_FAILURE;
			}

			// Should receive a THERE message from the server
			bytes = recv(sd, buffer, BUFFER, 0);

			if(bytes < 0) {
				fprintf(stderr, "ERROR: Could not receive THERE response from server!\n");
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

		else if (strcmp(token, "UPDATEPOSITION") == 0) {
			int value = 0;
			token = strtok(NULL, " ");
			while(token != NULL) {
				if(value == 0) {
					strcpy(sensorID, token);
					value = 1;
				}
				else if(value == 1) {
					sensorRange = atoi(token);
					value = 2;
				}
				else if(value == 2) {
					xPosition = atoi(token);
					value = 3;
				}
				else {
					yPosition = atoi(token);
					value = 4;
				}
				token = strtok(NULL, " ");
			}

			// Change global variables for position;
			strcpy(clientSensorID, sensorID);
			clientSensorRange = sensorRange;
			clientXPosition = xPosition;
			clientYPosition = yPosition;

			updatePosition(sd, sensorID, sensorRange, xPosition, yPosition);
		}

		else if (strcmp(token, "QUIT") == 0) {
			// Close the sd
			close(sd);

			// Free the reachable list
			free(reachables);

			// Destroy the mutex lock
			pthread_mutex_destroy(&lock);
			return EXIT_SUCCESS;
		}

		else {
			fprintf(stderr, "ERROR: Invalid Command. Please try again!\n");
		}
	}

	pthread_mutex_destroy(&lock);
	return EXIT_SUCCESS;
}