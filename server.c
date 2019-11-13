#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <pthread.h> 

#define MAX_BUFFER 512
#define MAX_STATIONS 256
#define MAX_NUM_LENGTH 128
#define MAX_LINKS 512
#define MAX_STATION_LENGTH 512

typedef int bool;
#define true 1
#define false 0

//keeps track of the index of the client socket
typedef struct {
	int i;
} Index;

typedef struct{
	char* id;
	int x;
	int y;
	int num_links;
	char** links;
} BaseStation;

typedef struct{
	char* id;
	int range;
	int x;
	int y;
} Sensor;

typedef struct {
	char* origin_id;
	char* next_id;
	char* destination_id;
	int hoplist_len;
	char** hoplist;
} DataMessage;

void generateBaseStations(const char* baseStationFile);
void readStdin();
void* listenForConnections(void* p);
void* handleSensor(void* p);
void createSensor(char* sensor_id, int sensor_range, int x_pos, int y_pos);
char* intToString(int num);
void sendThereMessage(int fd, char* node_id, char* x_pos_str, char* y_pos_str);

fd_set readfds; //keeps track of the sockets that 'select will listen for'
unsigned short controlPort;
int client_sockets[ MAX_STATIONS ]; /* client socket fd list */
int client_socket_index = 0;  /* next free spot */
BaseStation* base_stations; //keeps track of all the base stations
Sensor* sensors;
int num_stations, num_sensors;
pthread_mutex_t lock;

int main(int argc, char* argv[]) {
	// setvbuf for the buffer (for submitty)
	setvbuf(stdout, NULL, _IONBF, 0);

	// Check for correct number of command line arguments
	if(argc != 3) {
		fprintf(stderr, "ERROR: Invalid Arguments/Invalid Number of Arguments\n");
		return EXIT_FAILURE;
	}

	controlPort = atoi(argv[1]);
	const char* baseStationFile = argv[2];

	num_stations = 0;
	generateBaseStations(baseStationFile);

	//initialize the mutex
	if (pthread_mutex_init(&lock, NULL) != 0) { 
	  perror("\n mutex init has failed\n"); 
	  return 1; 
	} 

	num_sensors = 0;
    pthread_t tid;
    pthread_create(&tid, NULL, listenForConnections, NULL);

    readStdin();

	return EXIT_SUCCESS;
}

void generateBaseStations(const char* baseStationFile){
	//PARSING BASE STATION FILE-------------------------------------------

	// Try to open file and extract necessary information
	FILE *file = fopen(baseStationFile, "r");
	if(file == NULL) {
		fprintf(stderr, "ERROR: File could not be found/opened!\n");
		exit(1);
	}

	// Print base station found
	printf("Found base station file: %s\n", baseStationFile);

	//Get the number of stations (number of lines in file)
	char* line = NULL;
	size_t _len = 0;
	ssize_t line_len;

	while ((line_len = getline(&line, &_len, file)) != -1) {
		num_stations++;
	}
	printf("%d stations\n", num_stations);
	base_stations = calloc(num_stations, sizeof(BaseStation));

	rewind(file); //resets the pointer to beginning of file
	num_stations = 0;
	while((line_len = getline(&line, &_len, file)) != -1) {

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
			if(value == 0) {
				base_stations[num_stations].id = calloc(strlen(token) + 1, sizeof(char));
				strcpy(base_stations[num_stations].id, token);
			}
			else if(value == 1) {
				sscanf(token, "%d", &(base_stations[num_stations].x));  
			}
			else if(value == 2) {
				sscanf(token, "%d", &(base_stations[num_stations].y));  
			}
			else if(value == 3) {
				sscanf(token, "%d", &(base_stations[num_stations].num_links)); 
				base_stations[num_stations].links = calloc(base_stations[num_stations].num_links, sizeof(char*)); 
			}
			else if(value == 4) {
				for(int i = 0; i < base_stations[num_stations].num_links; i++){
					base_stations[num_stations].links[i] = calloc(strlen(token) + 1, sizeof(char));
					strcpy(base_stations[num_stations].links[i], token);
					token = strtok(NULL, " ");
				}
			}
			value++;
			token = strtok(NULL, " ");
		}
		num_stations++;		
	}

	// //Print out all the base stations for debugging
	// for(int i = 0; i < num_stations; i++){
	// 	BaseStation b = base_stations[i];
	// 	printf("id: %s\n", b.id);
	// 	printf("x: %d\n", b.x);
	// 	printf("y: %d\n", b.y);
	// 	printf("num_links: %d\n", b.num_links);
	// 	for(int j = 0; j < b.num_links; j++){
	// 		printf(" %s\n", b.links[j]);
	// 	}
	// 	printf("\n");
	// }

	fclose(file);
}

void readStdin(){
    char stdin_buffer[MAX_BUFFER];
    int num_bytes = 0;

	//Block on stdin
	while(fgets(stdin_buffer, MAX_BUFFER, stdin)){
		stdin_buffer[strcspn(stdin_buffer, "\n")] = 0; //remove newline from stdin_buffer
		num_bytes = strlen(stdin_buffer);
		printf("Read %s from stdin, %d bytes\n", stdin_buffer, num_bytes);

		if(strstr(stdin_buffer,"SENDDATA") != NULL){

			printf("MAIN: recieved SENDDATA command\n");

			//Get the origin_id and destination_id
			char* origin_id;
			char* destination_id;

			char* token = strtok(stdin_buffer, " ");
			int num_reads = 0;
			while(token != NULL) {
				if(num_reads == 1){
					origin_id = calloc(strlen(token)+1, sizeof(char));
					strcpy(origin_id, token);
				} else if(num_reads == 2){
					destination_id = calloc(strlen(token)+1, sizeof(char));
					strcpy(destination_id, token);
				}
				num_reads++;
				token = strtok(NULL, " ");
			}

			DataMessage* msg = malloc(sizeof(DataMessage));
    		if(strcmp(origin_id, "CONTROL") == 0){

    			printf("I think this is the first message\n");
    			//pick the base station closes to the sensor.
    			//where is the sensor?

    		}else{

    			//Message came from a base station
    			printf("Message came from a base station\n");

    		}



		}else if(strstr(stdin_buffer,"QUIT") != NULL){

			printf("MAIN: recieved QUIT command\n");

		}

		memset(stdin_buffer, 0, MAX_BUFFER);  
	}
}

void* listenForConnections(void* p){
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

    	select( FD_SETSIZE, &readfds, NULL, NULL, NULL ); //blocking

		//If there is a new connection being made
		if ( FD_ISSET( sd, &readfds ) ) {
      		int newsock = accept( sd, (struct sockaddr *)&client, (socklen_t *)&fromlen );
      		printf("MAIN: Rcvd incoming TCP connection from: %s\n", inet_ntoa( (struct in_addr)client.sin_addr ));
      		client_sockets[ client_socket_index++ ] = newsock;
      		Index* ind = malloc(sizeof(Index));
      		ind->i = client_socket_index-1;
      		pthread_t tid;
      		pthread_create(&tid, NULL, handleSensor, (void*) ind);
		}
	}
}

void* handleSensor(void* p){
	//Get the fd this child socket should be listening on 
	Index* ind = (Index*) p;
	int index = ind->i;
	int fd = client_sockets[ index ];
  	char buffer[ MAX_BUFFER ];


	while(1){
		printf("CHILD %lu: Blocking on recv\n", pthread_self());
		int num_bytes = recv( fd, buffer, MAX_BUFFER - 1, 0 );
		printf("CHILD %lu: recieved %d bytes, %s\n", pthread_self(), num_bytes, buffer);
		if(num_bytes < 0){

			perror("recv()");

		}else if ( num_bytes == 0 ){

			printf("CHILD %lu: Client disconnected\n", pthread_self());
			pthread_mutex_lock(&lock);
			int k;
			close( fd );

			// if(is_logged_in){
			//   deleteUser(current_user.userid);
			//   is_logged_in = false;  
			// }   

			/* remove fd from client_sockets[] array: */
			for ( k = 0 ; k < client_socket_index ; k++ )
			{
			  if ( fd == client_sockets[ k ] )
			  {
			    /* found it -- copy remaining elements over fd */
			    int m;
			    for ( m = k ; m < client_socket_index - 1 ; m++ )
			    {
			      client_sockets[ m ] = client_sockets[ m + 1 ];
			    }
			    client_socket_index--;
			    break;  /* all done */
			  }
			}
			pthread_mutex_unlock(&lock);
			// printf("NUMBER OF CONNECTIONS: %d\n", client_socket_index);
			break;

		} else {

			buffer[strcspn(buffer, "\n")] = 0; //remove newline from buffer

			if(strstr(buffer,"WHERE") != NULL){

				printf("recieved where\n");

				//get the id
				char* node_id;
				char* token = strtok(buffer, " ");
				int num_reads = 0;
				while(token != NULL) {
					if(num_reads == 1){
						node_id = calloc(strlen(token)+1, sizeof(char));
						strcpy(node_id, token);
					}
					num_reads++;
					token = strtok(NULL, " ");
				}

				//find this node and return the node id
				bool found_node = false;
				for(int i = 0; i < num_stations; i++){
					if(strcmp(base_stations[i].id, node_id) == 0){
						int x_pos = base_stations[i].x;
						int y_pos = base_stations[i].y;
						char* x_pos_str = intToString(x_pos);
						char* y_pos_str = intToString(y_pos);
						sendThereMessage(fd, node_id, x_pos_str, y_pos_str);
						free(x_pos_str);
						free(y_pos_str);
						found_node = true;
						break;
					}
				}

				if(found_node) continue;

				for(int i = 0; i < num_sensors; i++){
					if(strcmp(sensors[i].id, node_id) == 0){
						int x_pos = sensors[i].x;
						int y_pos = sensors[i].y;
						char* x_pos_str = intToString(x_pos);
						char* y_pos_str = intToString(y_pos);
						sendThereMessage(fd, node_id, x_pos_str, y_pos_str);
						free(x_pos_str);
						free(y_pos_str);
						found_node = true;
						break;
					}
				}

			}else if(strstr(buffer, "UPDATEPOSITION") != NULL){

				printf("recieved UPDATEPOSITION\n");

				char* sensor_id;
				char* sensor_range_str;
				char* x_str;
				char* y_str;
				int sensor_range, x_pos, y_pos;

				char* token = strtok(buffer, " ");
				int num_reads = 0;
				while(token != NULL) {
					if(num_reads == 1){
						sensor_id = calloc(strlen(token)+1, sizeof(char));
						strcpy(sensor_id, token);
					}else if(num_reads == 2){
						sensor_range_str = calloc(strlen(token)+1, sizeof(char));
						strcpy(sensor_range_str, token);
					}else if(num_reads == 3){
						x_str = calloc(strlen(token)+1, sizeof(char));
						strcpy(x_str, token);
					}else if(num_reads == 4){
						y_str = calloc(strlen(token)+1, sizeof(char));
						strcpy(y_str, token);
					}
					num_reads++;
					token = strtok(NULL, " ");
				}

				sscanf(sensor_range_str, "%d", &sensor_range);
				sscanf(x_str, "%d", &x_pos); 
				sscanf(y_str, "%d", &y_pos);  

				//Add sensor to array of sensors
				if(num_sensors == 0){

					createSensor(sensor_id, sensor_range, x_pos, y_pos);

				}else{

					//check if sensor is in sensors array
					bool found_sensor = false;
					int sensor_index;
					for(int i = 0; i < num_sensors; i++){
						if(strcmp(sensors[i].id, sensor_id) == 0){
							found_sensor = true;
							sensor_index = i;
							break;
						}
					}

					if(found_sensor){ //update sensors position
						sensors[sensor_index].x = x_pos;
						sensors[sensor_index].y = y_pos;
					}else{
						createSensor(sensor_id, sensor_range, x_pos, y_pos);
					}

				}

			}else if(strstr(buffer, "DATAMESSAGE") != NULL){

				printf("Recieved data message\n");

			}

			memset(buffer, 0, MAX_BUFFER);  

		}
	}
}

void createSensor(char* sensor_id, int sensor_range, int x_pos, int y_pos){
	if(num_sensors == 0)
		sensors = calloc(1,sizeof(Sensor));
	else
		sensors = reallocarray(sensors, num_sensors + 1, sizeof(Sensor));

	sensors[num_sensors].id = calloc(strlen(sensor_id) + 1, sizeof(char));
	strcpy(sensors[num_sensors].id, sensor_id);
	sensors[num_sensors].range = sensor_range;
	sensors[num_sensors].x = x_pos;
	sensors[num_sensors].y = y_pos;

	printf("Just created new sensor:\n");
	printf(" id: %s\n", sensors[num_sensors].id);
	printf(" range: %d\n", sensors[num_sensors].range);
	printf(" x: %d\n", sensors[num_sensors].x);
	printf(" y: %d\n", sensors[num_sensors].y);

	num_sensors++;
}

char* intToString(int num){
	char* str = calloc(10, sizeof(char));
	sprintf(str, "%d", num);
	return str;
}

void sendThereMessage(int fd, char* node_id, char* x_pos_str, char* y_pos_str){
	//format there message
	int there_msg_len = 5 + 1 + strlen(node_id) + 1 + 
		strlen(x_pos_str) + 1 + strlen(y_pos_str);
	char* there_msg = calloc(there_msg_len + 1, sizeof(char));
	strcat(there_msg, "THERE ");
	strcat(there_msg, node_id);
	strcat(there_msg, " ");
	strcat(there_msg, x_pos_str);
	strcat(there_msg, " ");
	strcat(there_msg, y_pos_str);
	there_msg[there_msg_len] = '\0';
	send(fd, there_msg, there_msg_len, 0);
	free(there_msg);
}