#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <pthread.h>
#include <math.h> 

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

typedef struct{
	Sensor* reachable_sensors;
	int num_reachable_sensors;
} SensorReachableInfo;

typedef struct{
	BaseStation* reachable_stations;
	int num_reachable_stations;
} StationReachableInfo;

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
SensorReachableInfo* getSensorReachableInfo(Sensor new_sensor);
StationReachableInfo* getStationReachableInfo(Sensor new_sensor);
void freeSensorReachableInfo(SensorReachableInfo* sensor_info);
void freeStationReachableInfo(StationReachableInfo* station_info);
void sendReachableMsg(int fd, int total_num_reachable, int num_reachable_sensors, int num_reachable_stations,
	Sensor* reachable_sensors, BaseStation* reachable_stations);
float getDistance(float x1, float y1, float x2, float y2);
void handleMessageAsBaseStation(DataMessage* dm_struct, char* data_message);
BaseStation* getBaseStation(char* id);
Sensor* getSensor(char* id);
bool stationIsInRange(BaseStation station, Sensor sensor);
bool inHopList(char* id, char** hoplist, int hoplist_len);

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

	//read command line args
	controlPort = atoi(argv[1]);
	const char* baseStationFile = argv[2];

	//create all the base stations
	num_stations = 0;
	generateBaseStations(baseStationFile);

	//initialize the mutex
	if (pthread_mutex_init(&lock, NULL) != 0) { 
	  perror("\n mutex init has failed\n"); 
	  return 1; 
	} 

	//listen for new connections from the client
	num_sensors = 0;
    pthread_t tid;
    pthread_create(&tid, NULL, listenForConnections, NULL);

    //main will block on stdin
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
					//remove possible newline character
					if(token[strlen(token) - 1] == '\n')
						token[strcspn(token, "\n")] = 0;

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

    			//find the sensor
    			bool found_sensor = false;
    			Sensor destination_sensor;
    			for(int i = 0; i < num_sensors; i++){
    				if(strcmp(sensors[i].id, destination_id) == 0){
    					found_sensor = true;
    					destination_sensor = sensors[i];
    				}
    			}

    			if(found_sensor){

    				//find the base station closes to the sensor
    				int min_index = 0;
    				float distance, min_distance = 0;
    				BaseStation station, min_station;
    				for(int i = 0; i < num_stations; i++){
    					station = base_stations[i];
    					printf("station: %s, position: (%d, %d)\n", 
    						station.id, station.x, station.y);
    					distance = getDistance(station.x, station.y, destination_sensor.x, destination_sensor.y);
    					printf("distance to point (%d, %d) is: %f\n", destination_sensor.x, destination_sensor.y,
    						distance);
    					printf("%f\n", distance);
    					if(i == 0 || distance < min_distance){
    						min_distance = distance;
    						min_index = i;
    					}
    				}
    				min_station = base_stations[min_index];

    				printf("base_stations[%d] with id: %s has min_distance: %f to destination %s at position: (%d,%d)\n", 
    					min_index, min_station.id, min_distance, destination_sensor.id, destination_sensor.x,
    					destination_sensor.y);

    				//send data message to closest station
    				char* data_message = calloc(MAX_BUFFER, sizeof(char));
    				sprintf(data_message, "DATAMESSAGE  %s  %s  %s  %d", origin_id, min_station.id, destination_id,
    					0);

    				//initialize data message struct
    				DataMessage* dm_struct = malloc(sizeof(DataMessage));
    				dm_struct->origin_id = calloc(8, sizeof(char));
    				strcpy(dm_struct->origin_id, "CONTROL");
    				dm_struct->next_id = min_station.id;
    				dm_struct->destination_id = destination_id;
    				dm_struct->hoplist_len = 0;

    				handleMessageAsBaseStation(dm_struct, data_message);

    			}else{
    				printf("Sensor not found\n");
    			}

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

				//Find this node and return the node id
				//search in the base stations first
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

				//if not in the base stations, check in the sensors
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

				//parse the rest of the updateposition message
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

				//convert sensor_range, x, and y to ints
				sscanf(sensor_range_str, "%d", &sensor_range);
				sscanf(x_str, "%d", &x_pos); 
				sscanf(y_str, "%d", &y_pos);  
				Sensor new_sensor;

				//Add sensor to array of sensors
				if(num_sensors == 0){

					createSensor(sensor_id, sensor_range, x_pos, y_pos);
					new_sensor = sensors[num_sensors-1];

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

					pthread_mutex_lock(&lock);
					if(found_sensor){ //update sensors position
						sensors[sensor_index].x = x_pos;
						sensors[sensor_index].y = y_pos;
						new_sensor = sensors[sensor_index];
					}else{
						createSensor(sensor_id, sensor_range, x_pos, y_pos);
						new_sensor = sensors[num_sensors-1];
					}
					pthread_mutex_unlock(&lock);
				}

				//Get reachable sensors and stations
				SensorReachableInfo* sensor_info = getSensorReachableInfo(new_sensor);
				Sensor* reachable_sensors = sensor_info->reachable_sensors;
				int num_reachable_sensors = sensor_info->num_reachable_sensors;

				Sensor current_sensor;
				for(int i = 0; i < num_reachable_sensors; i++){
					current_sensor = reachable_sensors[i];
					printf("sensor id: %s, x: %d, y: %d\n", current_sensor.id, current_sensor.x, current_sensor.y);
				}

				StationReachableInfo* station_info = getStationReachableInfo(new_sensor);
				BaseStation* reachable_stations = station_info->reachable_stations;
				int num_reachable_stations = station_info->num_reachable_stations;

				BaseStation current_station;
				for(int i = 0; i < num_reachable_stations; i++){
					current_station = reachable_stations[i];
					printf("station id: %s, x: %d, y: %d\n", current_station.id, current_station.x, current_station.y);
				}

				int total_num_reachable = num_reachable_sensors + num_reachable_stations;
				printf("total_num_reachable: %d, num_reachable_sensors: %d, num_reachable_stations: %d\n", 
					total_num_reachable, num_reachable_sensors, num_reachable_stations);

				sendReachableMsg(fd, total_num_reachable, num_reachable_sensors, num_reachable_stations,
					reachable_sensors, reachable_stations);

				freeSensorReachableInfo(sensor_info);
				freeStationReachableInfo(station_info);

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

SensorReachableInfo* getSensorReachableInfo(Sensor new_sensor){
	//Getting reachable sensors;
	int left_bound = new_sensor.x - new_sensor.range;
	int right_bound = new_sensor.x + new_sensor.range;
	int upper_bound = new_sensor.y + new_sensor.range;
	int lower_bound = new_sensor.y - new_sensor.range;
	int num_reachable_sensors = 0;
	//Allocate max space to avoid reallocation
	Sensor* reachable_sensors = calloc(num_sensors, sizeof(Sensor));
	Sensor current_sensor;
	for(int i = 0; i < num_sensors; i++){
		current_sensor = sensors[i];
		if(strcmp(current_sensor.id, new_sensor.id) != 0){
			//check if sensors is reachable
			if(current_sensor.x >= left_bound && current_sensor.x <= right_bound &&
				current_sensor.y >= lower_bound && current_sensor.y <= upper_bound){
				reachable_sensors[num_reachable_sensors++] = current_sensor;
			}
		}
	}

	SensorReachableInfo* result = malloc(sizeof(SensorReachableInfo));
	result->reachable_sensors = reachable_sensors;
	result->num_reachable_sensors = num_reachable_sensors;
	return result;
}

StationReachableInfo* getStationReachableInfo(Sensor new_sensor){
	//Getting reachable base stations
	BaseStation* reachable_stations = calloc(num_stations, sizeof(BaseStation));
	BaseStation current_station;
	int num_reachable_stations = 0;
	for(int i = 0; i < num_stations; i++){
		current_station = base_stations[i];
		if(strcmp(current_station.id, new_sensor.id) != 0){
			//check if sensors is reachable
			if(stationIsInRange(current_station, new_sensor)){
				reachable_stations[num_reachable_stations++] = current_station;
			}
		}
	}

	StationReachableInfo* result = malloc(sizeof(StationReachableInfo));
	result->reachable_stations = reachable_stations;
	result->num_reachable_stations = num_reachable_stations;
	return result;
}

void freeSensorReachableInfo(SensorReachableInfo* sensor_info){
	free(sensor_info->reachable_sensors);
	free(sensor_info);
}

void freeStationReachableInfo(StationReachableInfo* station_info){
	free(station_info->reachable_stations);
	free(station_info);
}

void sendReachableMsg(int fd, int total_num_reachable, int num_reachable_sensors, int num_reachable_stations, 
	Sensor* reachable_sensors, BaseStation* reachable_stations){
	//format the reachable_list string
	//calculate the length of the string
	int reachable_list_len = 0;
	char* current_id;
	char* current_x_str;
	char* current_y_str;

	int current_x, current_y, num_reachables_traversed = 0;
	char** reachable_ids = calloc(total_num_reachable, sizeof(char*));
	char** reachable_x_strs = calloc(total_num_reachable, sizeof(char*));
	char** reachable_y_strs = calloc(total_num_reachable, sizeof(char*));

	//get all the reachable sensors
	Sensor current_sensor;
	for(int i = 0; i < num_reachable_sensors; i++){
		current_sensor = reachable_sensors[i];
		current_id = current_sensor.id;
		current_x = current_sensor.x;
		current_y = current_sensor.y;
		current_x_str = intToString(current_x);
		current_y_str = intToString(current_y);
		reachable_list_len += strlen(current_id) + 1 + strlen(current_x_str) + 1
			+ strlen(current_y_str);
		reachable_ids[num_reachables_traversed] = current_id;
		reachable_x_strs[num_reachables_traversed] = current_x_str;
		reachable_y_strs[num_reachables_traversed] = current_y_str;
		num_reachables_traversed++;
	}

	//get all the reachable base stations
	BaseStation current_station;
	for(int i = 0; i < num_reachable_stations; i++){
		current_station = reachable_stations[i];
		current_id = current_station.id;
		current_x = current_station.x;
		current_y = current_station.y;
		current_x_str = intToString(current_x);
		current_y_str = intToString(current_y);
		reachable_list_len += strlen(current_id) + 1 + strlen(current_x_str) + 1
			+ strlen(current_y_str);
		reachable_ids[num_reachables_traversed] = current_id;
		reachable_x_strs[num_reachables_traversed] = current_x_str;
		reachable_y_strs[num_reachables_traversed] = current_y_str;
		num_reachables_traversed++;
	}

	//Add room for the spaces between entries
	if(total_num_reachable > 0)
		reachable_list_len += total_num_reachable - 1; 

	//copy ids, and positions into the reachable_list string
	char* reachable_list = calloc(reachable_list_len + 1, sizeof(char));
	for(int i = 0; i < total_num_reachable; i++){
		printf("%s %s %s\n", reachable_ids[i], reachable_x_strs[i], reachable_y_strs[i]);
		strcat(reachable_list, reachable_ids[i]);
		strcat(reachable_list, " ");
		strcat(reachable_list, reachable_x_strs[i]);
		strcat(reachable_list, " ");
		strcat(reachable_list, reachable_y_strs[i]);
		if(i != total_num_reachable - 1)
			strcat(reachable_list, " ");
		free(reachable_x_strs[i]);
		free(reachable_y_strs[i]);
	}
	reachable_list[reachable_list_len] = '\0';

	free(reachable_ids);
	free(reachable_x_strs);
	free(reachable_y_strs);

	//calculate length of reachable msg
	char* total_num_reachable_str = intToString(total_num_reachable);
	int total_num_reachable_strlen = strlen(total_num_reachable_str);
	int reachable_msglen = 9 + 1 + total_num_reachable_strlen + 1 + reachable_list_len;
	char* reachable_msg = calloc(reachable_msglen + 1, sizeof(char));

	//concat the reachable msg
	strcat(reachable_msg, "REACHABLE");
	strcat(reachable_msg, " ");
	strcat(reachable_msg, total_num_reachable_str);
	strcat(reachable_msg, " ");
	strcat(reachable_msg, reachable_list);
	reachable_msg[reachable_msglen] = '\0';

	send(fd, reachable_msg, reachable_msglen, 0);

	free(reachable_list);
	free(reachable_msg);
}

float getDistance(float x1, float y1, float x2, float y2){
	return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

void handleMessageAsBaseStation(DataMessage* dm_struct, char* data_message){
	//get the current base station
	BaseStation* station_ptr = getBaseStation(dm_struct->next_id);
	BaseStation station = *station_ptr;

	//get the links for this base station
	BaseStation* station_links = calloc(station.num_links, sizeof(BaseStation));
	// printf("num links for this station: %d\n", station.num_links);
	for(int i = 0; i < station.num_links; i++){
		// printf("This is id we are about to pass: %s, with len %lu\n", station.links[i],
			// strlen(station.links[i]));
		BaseStation* station_ptr = getBaseStation(station.links[i]);
		station_links[i] = *station_ptr;
	}

	//get the destination (either a sensor or a base station)
	//first check in base stations
	char* destination_id = dm_struct->destination_id;
	bool destination_is_station = false;
	BaseStation* destination_station_ptr = getBaseStation(destination_id);
	Sensor* destination_sensor_ptr = getSensor(destination_id);
	BaseStation destination_station;
	Sensor destination_sensor;
	if(destination_station_ptr == NULL){
		destination_sensor = *destination_sensor_ptr;
	}else{
		destination_station = *destination_station_ptr;
		destination_is_station = true;
	}

	//Figure out next move
	//look at all possible options (base stations and sensors)
	//remove those that would cause a cycle (in hoplist)
	//send to closest thing

	//find the link closest to the destination that is not in the hop list (no cycle)
	char** hoplist = dm_struct->hoplist;
	int hoplist_len = dm_struct->hoplist_len;
	bool valid_link_found = false;
	BaseStation closest_link;
	float closest_link_distance, link_distance;
	BaseStation link;
	for(int i = 0; i < station.num_links; i++){
		link = station_links[i];
		if(!inHopList(link.id, hoplist, hoplist_len)){
			if(destination_is_station)
				link_distance = getDistance(link.x, link.y, destination_station.x, destination_station.y);
			else
				link_distance = getDistance(link.x, link.y, destination_sensor.x, destination_sensor.y);

			if(!valid_link_found || link_distance < closest_link_distance){
				closest_link_distance = link_distance;
				closest_link = link;
			}
			valid_link_found = true;
		}
	}

	if(valid_link_found)
		printf("Valid link was found. Id: %s, distance: %f\n", closest_link.id, closest_link_distance);
	else
		printf("Valid link was not found. Either no links or all links were already in hoplist\n");

	//if the destination is a station would a station ever send a message to a sensor?

	//find closest sensor not in hoplist
	float closest_sensor_distance, sensor_distance;
	bool valid_sensor_found = false;
	Sensor sensor, closest_sensor;
	for(int i = 0; i < num_sensors; i++){
		sensor = sensors[i];
		if(stationIsInRange(station, sensor) && !inHopList(sensor.id, hoplist, hoplist_len)){
			if(destination_is_station)
				sensor_distance = getDistance(sensor.x, sensor.y, destination_station.x, destination_station.y);
			else
				sensor_distance = getDistance(sensor.x, sensor.y, destination_sensor.x, destination_sensor.y);

			if(!valid_sensor_found || sensor_distance < closest_sensor_distance){
				closest_sensor_distance = sensor_distance;
				closest_sensor = sensor;
			}
			valid_sensor_found = true;
		}
	}

	if(valid_sensor_found)
		printf("Valid sensor was found. Id: %s, distance: %f\n", closest_sensor.id, closest_sensor_distance);
	else
		printf("Valid sensor was not found. Either no sensors or all sensors were already in hoplist\n");

}

BaseStation* getBaseStation(char* id){
	BaseStation* station = NULL;
	for(int i = 0; i < num_stations; i++){
		int result = strcmp(base_stations[i].id, id);
		// printf("CURRENT STATION: %s with len :%lu, id: %s with len: %lu result: %d\n", base_stations[i].id,
		// 	strlen(base_stations[i].id), id, strlen(id), result);
		if(result == 0){
			// printf("I SHOULD BE PRINTING THIS, id: %s\n", id);
			station = &base_stations[i];
			break;
		}
	}
	return station;
}

Sensor* getSensor(char* id){
	Sensor* sensor = NULL;
	for(int i = 0; i < num_sensors; i++){
		if(strcmp(sensors[i].id, id) == 0){
			sensor = &sensors[i];
			break;
		}
	}
	return sensor;
}

bool stationIsInRange(BaseStation station, Sensor sensor){
	int left_bound = sensor.x - sensor.range;
	int right_bound = sensor.x + sensor.range;
	int upper_bound = sensor.y + sensor.range;
	int lower_bound = sensor.y - sensor.range;
	return (station.x >= left_bound && station.x <= right_bound
		&& station.y >= lower_bound && station.y <= upper_bound);
}

bool inHopList(char* id, char** hoplist, int hoplist_len){
	for(int i = 0; i < hoplist_len; i++){
		if(strcmp(id, hoplist[i]) == 0){
			return true;
		}
	}
	return false;
}