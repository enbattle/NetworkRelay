#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_BUFFER 512
#define MAX_STATIONS 256
#define MAX_NUM_LENGTH 128
#define MAX_LINKS 512
#define MAX_STATION_LENGTH 512

fd_set readfds; //keeps track of the sockets that 'select will listen for'

int main(int argc, char* argv[]) {
	// Check for correct number of command line arguments
	if(argc != 3) {
		fprintf(stderr, "ERROR: Invalid Arguments/Invalid Number of Arguments\n");
		return EXIT_FAILURE;
	}

	// setvbuf for the buffer
	setvbuf(stdout, NULL, _IONBF, 0);

	int i;
	int j;
	int k;

	// Control port argument and base station file argument
	unsigned short controlPort = atoi(argv[1]);
	const char* baseStationFile = argv[2];

	//PARSING BASE STATION FILE-------------------------------------------

	// Try to open file and extract necessary information
	FILE *file = fopen(baseStationFile, "r");
	if(file == NULL) {
		fprintf(stderr, "ERROR: File could not be found/opened!\n");
		return EXIT_FAILURE;
	}

	// Print base station found
	printf("Found base station file: %s\n", baseStationFile);

	// Initialize the base station characteristic arrays
		// for easier access of data
	char** baseStations = (char**)calloc(MAX_STATIONS, sizeof(char*));
	for(i=0; i<MAX_STATIONS; i++) {
		baseStations[i] = (char*)calloc(MAX_STATION_LENGTH, sizeof(char));
	}

	int** stationCoordinates = (int**)calloc(MAX_STATIONS, sizeof(int*));
	for(i=0; i<MAX_STATIONS; i++) {
		stationCoordinates[i] = (int*)calloc(2, sizeof(int));
	}

	int* stationNumLinks = (int*)calloc(MAX_STATIONS, sizeof(int));

	char*** stationLinks = (char***)calloc(MAX_STATIONS, sizeof(char**));
	for(i=0; i<MAX_STATIONS; i++) {
		stationLinks[i] = (char**)calloc(MAX_LINKS, sizeof(char*));
		for(j=0; j<MAX_LINKS; j++) {
			stationLinks[i][j] = (char*)calloc(MAX_LINKS, sizeof(char));
		}
	}

	// Keep track of the number of stations
	int numOfStations = 0;

	// Counters for each of the station characteristic arrays
	int stationsCounter = 0;
	int coordinatesCounter = 0;
	int numLinksCounter = 0;
	int linksCounter = 0;

	char line[MAX_BUFFER];
	while(fgets(line, MAX_BUFFER, file) != NULL) {
		printf("The line is: %s\n", line);

		numOfStations++;

		// Counter to indicate current value parsed in the file
		// 0 --- Base ID
		// 1 --- X-Coordinate
		// 2 --- Y-Coordinate
		// 3 --- Number of Links
		// 4 --- List of Stations
		int value = 0;

		// Run through each line from file, splitting by a space delimiter
		char* token = strtok(line, " ");
		while(token != NULL) {
			printf("%s\n", token);
			if(value == 0) {
				strcpy(baseStations[stationsCounter++], token);
				value = 1;
			}
			else if(value == 1) {
				char x_pos[MAX_NUM_LENGTH];
				strcpy(x_pos, token);
				stationCoordinates[coordinatesCounter][0] = atoi(x_pos);
				value = 2;
			}
			else if(value == 2) {
				char y_pos[MAX_NUM_LENGTH];
				strcpy(y_pos, token);
				stationCoordinates[coordinatesCounter++][1] = atoi(y_pos);
				value = 3;
			}
			else if(value == 3) {
				char links[MAX_NUM_LENGTH];
				strcpy(links, token);
				stationNumLinks[numLinksCounter++] = atoi(links);
				value = 4;
			}
			else if(value == 4) {
				int temporaryLinkCounter = 0;
				while(token != NULL) {
					strcpy(stationLinks[linksCounter][temporaryLinkCounter++], token);
					token = strtok(NULL, " ");
				}
				linksCounter++;
			}
			token = strtok(NULL, " ");
		}		
	}

	// Debugging statement to make sure file was read in correctly
	for(i=0; i<numOfStations; i++) {
		printf("Station: %s\n", baseStations[i]);
		printf("Station Coordinates: (%d, %d)\n", stationCoordinates[i][0], 
			stationCoordinates[i][1]);
		printf("Number of links: %d\n", stationNumLinks[i]);
		printf("Links:\n");
		for(j=0; j<stationNumLinks[i]; j++) {
			printf("\t %s\n", stationLinks[i][j]);
		}
		printf("\n");
	}

	fclose(file);

	//SETTTING UP TCP SERVER----------------------------------------------

	//Creating listener socket
  	int sd = socket( PF_INET, SOCK_STREAM, 0 );
  	if ( sd < 0 )
  	{
  	  perror( "ERROR: tcp socket() failed" );
  	  exit( EXIT_FAILURE );
  	}

  	struct sockaddr_in server;
  	struct sockaddr_in client;
	server.sin_family = PF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( controlPort );
	int len = sizeof( server );

	//bind the server to the port
	if ( bind( sd, (struct sockaddr *)&server, len ) < 0 )
	{
	  perror( "ERROR: tcp bind() failed" );
	  exit( EXIT_FAILURE );
	}

	//listen on port
	if ((listen(sd, 5)) != 0) { 
	    printf("Listen failed...\n"); 
	    exit(0); 
	} 
	else
	    printf("Server: listening for TCP connections on port: %d\n", controlPort); 

	int fromlen = sizeof( client );
	char buffer[ MAX_BUFFER ];
	int n;

	while(1){
		FD_ZERO( &readfds ); //initializes the file descriptor set
		FD_SET( sd, &readfds ); //select() will check tcp socket for activity

		//If there is activity on the socket
		if ( FD_ISSET( sd, &readfds ) ) {
      		int newsock = accept( sd, (struct sockaddr *)&client, (socklen_t *)&fromlen );
      		printf("MAIN: Rcvd incoming TCP connection from: %s\n", inet_ntoa( (struct in_addr)client.sin_addr ));

		}
	}





	// Freeing the allocated memory
	for(i=0; i<MAX_STATIONS; i++) {
		free(baseStations[i]);
		free(stationCoordinates[i]);
		for(j=0; j<MAX_LINKS; j++) {
			free(stationLinks[i][j]);
		}
		free(stationLinks[i]);
	}
	free(baseStations);
	free(stationCoordinates);
	free(stationNumLinks);
	free(stationLinks);

	return EXIT_SUCCESS;
}