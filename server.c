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

typedef struct {
	int fd; //fd associated with sensor id
	char* sensor_id; 
} Client;

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

typedef struct{
	char* origin_id;
	char* destination_id;
}OriginAndDestination;

typedef struct {
	char* origin_id;
	char* next_id;
	char* destination_id;
	int hoplist_len;
	char* hoplist;
} DataMessage;

void generateBaseStations(const char* baseStationFile);
void parseBaseStationFromLine(char* line);
void readStdin();
void handleSendData(char stdin_buffer[]);
OriginAndDestination* parseSendDataMsg(char buffer[]);
void freeOriginAndDestination(OriginAndDestination* orig_and_dest);
BaseStation findStationClosestToSensor(Sensor sensor);
void* listenForConnections(void* p);
void* handleSensor(void* p);
void handleWhere(int fd, char buffer[]);
void handleUpdatePosition(int fd, char buffer[], int client_index);
void handleDataMsg(char buffer[]);
char* parseWhereMsg(char buffer[]);
Sensor* parseUpdatePositionMsg(char buffer[]);
int getSensorIndex(char* sensor_id);
Sensor updateSensorPosition(char* sensor_id, int sensor_range, int x, int y);
void createSensor(char* sensor_id, int sensor_range, int x_pos, int y_pos);
char* intToString(int num);
void sendThereMessage(int fd, char* node_id);
SensorReachableInfo* getSensorReachableInfo(Sensor new_sensor);
StationReachableInfo* getStationReachableInfo(Sensor new_sensor);
void freeSensorReachableInfo(SensorReachableInfo* sensor_info);
void freeStationReachableInfo(StationReachableInfo* station_info);
void sendReachableMsg(int fd, int total_num_reachable, int num_reachable_sensors, int num_reachable_stations,
	Sensor* reachable_sensors, BaseStation* reachable_stations);
void addReachableSensorsAndStationsToReachableList(char* reachable_list, int total_num_reachable, 
	int num_reachable_sensors, int num_reachable_stations, Sensor* reachable_sensors,
	 BaseStation* reachable_stations);
float getDistanceToStationOrSensor(float x1, float y1, bool destination_is_station,
	Sensor destination_sensor, BaseStation destination_station);
float getDistance(float x1, float y1, float x2, float y2);
void handleMessageAsBaseStation(DataMessage* dm_struct, char* data_message, bool destination_is_station);
BaseStation** getStationLinks(BaseStation station);
BaseStation* getBaseStation(char* id);
BaseStation* getClosestValidLink(char* hoplist, int hoplist_len, char* destination_id, bool destination_is_station,
	BaseStation station, BaseStation** station_links, BaseStation destination_station, Sensor destination_sensor);
Sensor* getClosestValidSensor(char* hoplist, int hoplist_len, char* destination_id, bool destination_is_station,
	BaseStation station, BaseStation destination_station, Sensor destination_sensor);
Sensor* getSensor(char* id);
bool stationIsInRange(BaseStation station, Sensor sensor);
bool inHopList(char* id, char* hoplist, int hoplist_len);
Client getClientBySensorID(char* sensor_id);
bool destinationIsStation(char* destination_id);

fd_set readfds; //keeps track of the sockets that 'select will listen for'
unsigned short controlPort;
int client_sockets[ MAX_STATIONS ]; /* client socket fd list */
Client clients[ MAX_STATIONS ];
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
	// Try to open file
	FILE *file = fopen(baseStationFile, "r");
	if(file == NULL) {
		fprintf(stderr, "ERROR: File could not be found/opened!\n");
		exit(1);
	}

	//Get the number of stations (number of lines in file)
	char* line = NULL;
	size_t _len = 0;
	ssize_t line_len;
	while ((line_len = getline(&line, &_len, file)) != -1) {
		num_stations++;
	}
	base_stations = calloc(num_stations, sizeof(BaseStation));

	rewind(file); //resets the pointer to beginning of file
	num_stations = 0;
	while((line_len = getline(&line, &_len, file)) != -1) {
		parseBaseStationFromLine(line);
	}

	fclose(file);
}

void parseBaseStationFromLine(char* line){
	// Counter to indicate current value parsed on this line
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
			handleSendData(stdin_buffer);


		}else if(strstr(stdin_buffer,"QUIT") != NULL){

			printf("MAIN: recieved QUIT command\n");

		}

		memset(stdin_buffer, 0, MAX_BUFFER);  
	}
}

void handleSendData(char stdin_buffer[]){
	//Get the origin_id and destination_id
	OriginAndDestination* orig_and_dest = parseSendDataMsg(stdin_buffer);
	char* origin_id = orig_and_dest->origin_id;
	char* destination_id = orig_and_dest->destination_id;

	// DataMessage* msg = malloc(sizeof(DataMessage));
	if(strcmp(origin_id, "CONTROL") == 0){

		Sensor* destination_sensor_ptr = getSensor(destination_id);
		if(destination_sensor_ptr != NULL){
			Sensor destination_sensor = *destination_sensor_ptr;

			BaseStation closest_station = findStationClosestToSensor(destination_sensor);

			//format data message to closest station
			char* data_message = calloc(MAX_BUFFER, sizeof(char));
			sprintf(data_message, "DATAMESSAGE  %s  %s  %s  %d", origin_id, closest_station.id, destination_id,
				0);

			//initialize data message struct
			DataMessage* dm_struct = malloc(sizeof(DataMessage));
			dm_struct->origin_id = calloc(8, sizeof(char));
			strcpy(dm_struct->origin_id, "CONTROL");
			dm_struct->next_id = calloc(MAX_BUFFER, sizeof(char));
			sprintf(dm_struct->next_id, "%s", closest_station.id);
			dm_struct->destination_id = destination_id;
			dm_struct->hoplist_len = 0;
			dm_struct->hoplist = calloc(MAX_BUFFER, sizeof(char));
			bool destination_is_station = destinationIsStation(destination_id);

			//send data message to closest station
			handleMessageAsBaseStation(dm_struct, data_message, destination_is_station);

		}else{
			printf("Sensor not found\n");
		}

	}else{

		//Message came from a base station
		printf("Message came from a base station\n");

	}

	freeOriginAndDestination(orig_and_dest);
}

OriginAndDestination* parseSendDataMsg(char buffer[]){
	OriginAndDestination* orig_and_dest = malloc(sizeof(OriginAndDestination));
	char* token = strtok(buffer, " ");
	int num_reads = 0;
	while(token != NULL) {
		if(num_reads == 1){
			orig_and_dest->origin_id = calloc(strlen(token)+1, sizeof(char));
			strcpy(orig_and_dest->origin_id, token);
		} else if(num_reads == 2){
			orig_and_dest->destination_id = calloc(strlen(token)+1, sizeof(char));
			strcpy(orig_and_dest->destination_id, token);
		}
		num_reads++;
		token = strtok(NULL, " ");
	}
	return orig_and_dest;
}

void freeOriginAndDestination(OriginAndDestination* orig_and_dest){
	free(orig_and_dest->origin_id);
	free(orig_and_dest->destination_id);
}

BaseStation findStationClosestToSensor(Sensor sensor){
	int min_index = 0;
	float distance, min_distance = 0;
	BaseStation station, closest_station;
	for(int i = 0; i < num_stations; i++){
		station = base_stations[i];
		distance = getDistance(station.x, station.y, sensor.x, sensor.y);
		printf("station %s, distance to point (%d, %d) is: %f\n", station.id, sensor.x, sensor.y,
			distance);
		printf("%f\n", distance);
		if(i == 0 || distance < min_distance){
			min_distance = distance;
			min_index = i;
		}
	}
	closest_station = base_stations[min_index];

	printf("base_stations[%d] with id: %s has min_distance: %f to destination %s at position: (%d,%d)\n", 
		min_index, closest_station.id, min_distance, sensor.id, sensor.x,
		sensor.y);
	return closest_station;
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
	// char buffer[ MAX_BUFFER ];
	// int n;

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
	clients[index].fd = fd;
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

			    for(m = k; m < client_socket_index - 1; m++)
			    {
			    	clients[m].fd = clients[m + 1].fd;
			    	clients[m].sensor_id = clients[m + 1].sensor_id;
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
				handleWhere(fd, buffer);

			}else if(strstr(buffer, "UPDATEPOSITION") != NULL){

				printf("recieved UPDATEPOSITION\n");
				handleUpdatePosition(fd, buffer, index);

			}else if(strstr(buffer, "DATAMESSAGE") != NULL){

				printf("Recieved data message\n");
				handleDataMsg(buffer);

			}

			memset(buffer, 0, MAX_BUFFER);  

		}
	}

	// Exit the current connection
	pthread_exit(p);
}

void handleDataMsg(char buffer[]){
	//parse the data message
	DataMessage* dm_struct = malloc(sizeof(DataMessage));
	char* token = strtok(buffer, " ");
	int num_reads = 0;
	while(token != NULL) {
		if(num_reads == 1){
			printf("read: %d, token %s\n", num_reads, token);
			dm_struct->origin_id = calloc(strlen(token)+1, sizeof(char));
			strcpy(dm_struct->origin_id, token);
		}else if(num_reads == 2){
			printf("read: %d, token %s\n", num_reads, token);
			dm_struct->next_id = calloc(strlen(token)+1, sizeof(char));
			strcpy(dm_struct->next_id, token);
		}else if(num_reads == 3){
			printf("read: %d, token %s\n", num_reads, token);
			printf("destination_id: %s\n", token);
			dm_struct->destination_id = calloc(strlen(token)+1, sizeof(char));
			strcpy(dm_struct->destination_id, token);
		}else if(num_reads == 4){
			printf("read: %d, token %s\n", num_reads, token);
			sscanf(token, "%d", &(dm_struct->hoplist_len));
		}else if(num_reads == 5){
			printf("read: %d, token %s\n", num_reads, token);
			dm_struct->hoplist = calloc(MAX_BUFFER, sizeof(char));
			strcpy(dm_struct->hoplist, token);
		}
		num_reads++;
		token = strtok(NULL, " ");
	}
	char* data_message = calloc(MAX_BUFFER, sizeof(char));
	sprintf(data_message, "DATAMESSAGE %s %s %s %d", dm_struct->origin_id, dm_struct->next_id,
		dm_struct->destination_id, dm_struct->hoplist_len);
	if(dm_struct->hoplist_len != 0)
		sprintf(data_message + strlen(data_message), " %s", dm_struct->hoplist);
	printf("DataMessage: %s\n", data_message);
	bool destination_is_station = destinationIsStation(dm_struct->destination_id);
	handleMessageAsBaseStation(dm_struct, data_message, destination_is_station);
}

void handleWhere(int fd, char buffer[]){
	char* node_id = parseWhereMsg(buffer);

	BaseStation* station_ptr = getBaseStation(node_id);
	if(station_ptr != NULL){
		BaseStation station = *station_ptr;
		sendThereMessage(fd, station.id);
		free(node_id);
		return;
	}

	Sensor* sensor_ptr = getSensor(node_id);
	Sensor sensor = *sensor_ptr;
	sendThereMessage(fd, sensor.id);
	free(node_id);
}

void handleUpdatePosition(int fd, char buffer[], int client_index){
	Sensor* sensor_ptr = parseUpdatePositionMsg(buffer);
	char* sensor_id = sensor_ptr->id;
	int sensor_range = sensor_ptr->range;
	int x = sensor_ptr->x;
	int y = sensor_ptr->y;

	Sensor new_sensor;
	//Add sensor to array of sensors
	pthread_mutex_lock(&lock);
	if(num_sensors == 0){

		createSensor(sensor_id, sensor_range, x, y);
		new_sensor = sensors[num_sensors-1];
		clients[client_index].sensor_id = sensors[num_sensors-1].id;

	}else{

		if(getSensor(sensor_id) != NULL){ 
			new_sensor = updateSensorPosition(sensor_id, sensor_range, x, y);
		}else{
			createSensor(sensor_id, sensor_range, x, y);
			new_sensor = sensors[num_sensors-1];
			clients[client_index].sensor_id = sensors[num_sensors-1].id;
		}
	}
	pthread_mutex_unlock(&lock);

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
}

Sensor* parseUpdatePositionMsg(char buffer[]){
	char* sensor_id;
	char* sensor_range_str;
	char* x_str;
	char* y_str;
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
	int sensor_range, x, y;
	sscanf(sensor_range_str, "%d", &sensor_range);
	sscanf(x_str, "%d", &x); 
	sscanf(y_str, "%d", &y);  

	//create new sensor
	Sensor* sensor = malloc(sizeof(Sensor));
	sensor->id = sensor_id;
	sensor->range = sensor_range;
	sensor->x = x;
	sensor->y = y;

	//free temporary memory
	free(sensor_range_str);
	free(x_str);
	free(y_str);

	return sensor;
}

int getSensorIndex(char* sensor_id){
	for(int i = 0; i < num_sensors; i++){
		if(strcmp(sensors[i].id, sensor_id) == 0)
			return i;
	}
	printf("Something went wrong\n");
	return -1;
}

Sensor updateSensorPosition(char* sensor_id, int sensor_range, int x, int y){
	int index = getSensorIndex(sensor_id);
	sensors[index].range = sensor_range;
	sensors[index].x = x;
	sensors[index].y = y;
	return sensors[index];
}

char* parseWhereMsg(char buffer[]){
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
	return node_id;
}

void createSensor(char* sensor_id, int sensor_range, int x_pos, int y_pos){
	if(num_sensors == 0)
		sensors = calloc(1,sizeof(Sensor));
	else
		sensors = realloc(sensors, sizeof(Sensor) * (num_sensors + 1));

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

void sendThereMessage(int fd, char* node_id){
	char* there_msg = calloc(MAX_BUFFER, sizeof(char));

	//find node (either base station or sensor)
	BaseStation* station_ptr = getBaseStation(node_id);
	Sensor* sensor_ptr = getSensor(node_id);
	BaseStation station;
	Sensor sensor;

	if(station_ptr == NULL){
		sensor = *sensor_ptr;
		sprintf(there_msg, "THERE %s  %d  %d", node_id, sensor.x, sensor.y);
	}else{
		station = *station_ptr;
		sprintf(there_msg, "THERE %s  %d  %d", node_id, station.x, station.y);
	}

	send(fd, there_msg, strlen(there_msg), 0);
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

	char* reachable_list = calloc(MAX_BUFFER, sizeof(char));
	addReachableSensorsAndStationsToReachableList(reachable_list, total_num_reachable, num_reachable_sensors,
		num_reachable_stations, reachable_sensors, reachable_stations);

	char* reachable_msg = calloc(MAX_BUFFER, sizeof(char));
	sprintf(reachable_msg, "REACHABLE %d %s", total_num_reachable, reachable_list);

	send(fd, reachable_msg, strlen(reachable_msg), 0);

	free(reachable_list);
	free(reachable_msg);
}

void addReachableSensorsAndStationsToReachableList(char* reachable_list, int total_num_reachable, 
	int num_reachable_sensors, int num_reachable_stations, Sensor* reachable_sensors,
	 BaseStation* reachable_stations){
	//add all reachable sensors
	Sensor current_sensor;
	int num_reachables_traversed = 0;
	for(int i = 0; i < num_reachable_sensors; i++){
		current_sensor = reachable_sensors[i];
		sprintf(reachable_list + strlen(reachable_list), "%s %d %d", 
			current_sensor.id, current_sensor.x, current_sensor.y);

		if(num_reachables_traversed != total_num_reachable - 1)
			sprintf(reachable_list + strlen(reachable_list), " ");

		num_reachables_traversed++;
	}

	//add all the reachable base stations
	BaseStation current_station;
	for(int i = 0; i < num_reachable_stations; i++){
		current_station = reachable_stations[i];
		sprintf(reachable_list + strlen(reachable_list), "%s %d %d", 
			current_station.id, current_station.x, current_station.y);

		if(num_reachables_traversed != total_num_reachable - 1)
			sprintf(reachable_list + strlen(reachable_list), " ");

		num_reachables_traversed++;
	}
}

float getDistanceToStationOrSensor(float x1, float y1, bool destination_is_station,
	Sensor destination_sensor, BaseStation destination_station){
	if(destination_is_station)
		return getDistance(x1, y1, destination_station.x, destination_station.y);
	else
		return getDistance(x1, y1, destination_sensor.x, destination_sensor.y);
}

float getDistance(float x1, float y1, float x2, float y2){
	return sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

void handleMessageAsBaseStation(DataMessage* dm_struct, char* data_message, bool destination_is_station){
	//get the current base station and its links
	BaseStation* station_ptr = getBaseStation(dm_struct->next_id);
	BaseStation station = *station_ptr;
	BaseStation** station_links = getStationLinks(station);
	printf("IN handleMessageAsBaseStation. Station: %s\n", station.id);
	printf("Trying to send message from: %s to: %s current next id is: %s hoplist_len: %d, hoplist: %s\n",
		dm_struct->origin_id, dm_struct->destination_id, dm_struct->next_id, dm_struct->hoplist_len,
		dm_struct->hoplist);

	//check if this is the destination
	char* destination_id = dm_struct->destination_id;
	if(strcmp(station.id, destination_id) == 0){
		printf("Message from %s to %s successfully received.\n", dm_struct->origin_id, destination_id);
		return;
	}

	BaseStation destination_station;
	Sensor destination_sensor;
	if(destination_is_station)
		destination_station = *(getBaseStation(destination_id));
	else
		destination_sensor = *(getSensor(destination_id));

	//get closest valid link or sensor (link or sensor not in the hoplist)
	//if the destination is a station would a station ever send a message to a sensor?
	char* hoplist = dm_struct->hoplist;
	int hoplist_len = dm_struct->hoplist_len;
	if(hoplist_len == 0)
		dm_struct->hoplist = calloc(MAX_BUFFER, sizeof(char));

	BaseStation* closest_valid_link_ptr = getClosestValidLink(hoplist, hoplist_len, destination_id,
		destination_is_station, station, station_links, destination_station, destination_sensor);
	BaseStation closest_valid_link;
	if(closest_valid_link_ptr != NULL){
		printf("closest valid link id: %s\n", closest_valid_link_ptr->id);
		closest_valid_link = *closest_valid_link_ptr;
		printf("Valid link was found. Id: %s\n", closest_valid_link.id);
	}else{
		printf("Valid link was not found. Either no links or all links were already in hoplist\n");	
	}

	Sensor* closest_valid_sensor_ptr = getClosestValidSensor(hoplist, hoplist_len, destination_id,
		destination_is_station, station, destination_station, destination_sensor);
	Sensor closest_valid_sensor;
	if(closest_valid_sensor_ptr != NULL){
		closest_valid_sensor = *closest_valid_sensor_ptr;
		printf("Valid sensor was found. Id: %s\n", closest_valid_sensor.id);
	}else{
		printf("Valid sensor was not found. Either no sensors or all sensors were already in hoplist\n");
	}

	//figure out which is closer (closest link or closest sensor)
	float closest_link_distance, closest_sensor_distance;
	if(closest_valid_link_ptr != NULL && closest_valid_sensor_ptr != NULL){

		closest_link_distance = getDistanceToStationOrSensor(closest_valid_link.x, closest_valid_link.y,
			destination_is_station, destination_sensor, destination_station);
		closest_sensor_distance = getDistanceToStationOrSensor(closest_valid_sensor.x, closest_valid_sensor.y,
			destination_is_station, destination_sensor, destination_station);

		if(closest_link_distance < closest_sensor_distance){

			printf("link id: %s with closest_link_distance: %f < sensor id: %s with closest_sensor_distance: %f\n", 
				closest_valid_link.id, closest_link_distance, closest_valid_sensor.id, closest_sensor_distance);

			strcpy(dm_struct->next_id, closest_valid_link.id);
			dm_struct->hoplist_len++;
			sprintf(dm_struct->hoplist + strlen(dm_struct->hoplist), "%s ", station.id);
			sprintf(data_message, "DATAMESSAGE  %s  %s  %s  %d  %s", dm_struct->origin_id, 
				dm_struct->next_id, dm_struct->destination_id, dm_struct->hoplist_len,
				dm_struct->hoplist);
			handleMessageAsBaseStation(dm_struct, data_message, destination_is_station);

		}else if(closest_sensor_distance < closest_link_distance){

			printf("link id: %s with closest_link_distance: %f > sensor id: %s with closest_sensor_distance: %f\n", 
				closest_valid_link.id, closest_link_distance, closest_valid_sensor.id, closest_sensor_distance);

			strcpy(dm_struct->next_id, closest_valid_sensor.id);
			dm_struct->hoplist_len++;
			printf("This is the length of the hoplist %lu\n", strlen(dm_struct->hoplist));
			sprintf(dm_struct->hoplist + strlen(dm_struct->hoplist), "%s ", station.id);
			sprintf(data_message, "DATAMESSAGE %s %s %s %d %s", dm_struct->origin_id, 
				dm_struct->next_id, dm_struct->destination_id, dm_struct->hoplist_len,
				dm_struct->hoplist);
			Client target_cli = getClientBySensorID(dm_struct->next_id);
			printf("Client with fd %d is associated with sensor id: %s\n",
				target_cli.fd, target_cli.sensor_id );
			printf("Sending data message %s\n", data_message);
			send(target_cli.fd, data_message, strlen(data_message), 0);

		}else{
			printf("They are equal distance away. What should I do?\n");
		}

	}else if(closest_valid_link_ptr == NULL && closest_valid_sensor_ptr != NULL){

		strcpy(dm_struct->next_id, closest_valid_sensor.id);
		dm_struct->hoplist_len++;
		sprintf(dm_struct->hoplist + strlen(dm_struct->hoplist), "%s ", station.id);
		sprintf(data_message, "DATAMESSAGE %s %s %s %d %s", dm_struct->origin_id, 
			dm_struct->next_id, dm_struct->destination_id, dm_struct->hoplist_len,
			dm_struct->hoplist);
		Client target_cli = getClientBySensorID(dm_struct->next_id);
		printf("Client with fd %d is associated with sensor id: %s\n",
			target_cli.fd, target_cli.sensor_id );
		printf("Sending data message %s\n", data_message);
		send(target_cli.fd, data_message, strlen(data_message), 0);

	}else if(closest_valid_link_ptr != NULL && closest_valid_sensor_ptr == NULL){

		strcpy(dm_struct->next_id, closest_valid_link.id);
		dm_struct->hoplist_len++;
		sprintf(dm_struct->hoplist + strlen(dm_struct->hoplist), "%s ", station.id);
		sprintf(data_message, "DATAMESSAGE  %s  %s  %s  %d  %s", dm_struct->origin_id, 
			dm_struct->next_id, dm_struct->destination_id, dm_struct->hoplist_len,
			dm_struct->hoplist);
		handleMessageAsBaseStation(dm_struct, data_message, destination_is_station);		

	}else{
		printf("%s: Message from %s to %s could not be delivered.\n", station.id, 
			dm_struct->origin_id, dm_struct->destination_id);
	}

}

BaseStation** getStationLinks(BaseStation station){
	BaseStation** station_links = calloc(station.num_links, sizeof(BaseStation*));
	for(int i = 0; i < station.num_links; i++){
		BaseStation* station_ptr = getBaseStation(station.links[i]);
		station_links[i] = station_ptr;
	}
	return station_links;
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
	printf("This is the station id: %s with len: %lu\n", id, strlen(id));
	if(station == NULL)
		printf("Station was null\n");
	else
		printf("About to return station with id: %s\n", station->id);
	return station;
}

//Returns the closest link to the provided station that is not the hoplist
BaseStation* getClosestValidLink(char* hoplist, int hoplist_len, char* destination_id, bool destination_is_station,
	BaseStation station, BaseStation** station_links, BaseStation destination_station, Sensor destination_sensor){
	BaseStation* closest_link = NULL;
	BaseStation link;
	float closest_link_distance, link_distance;
	for(int i = 0; i < station.num_links; i++){
		link = *(station_links[i]);
		if(hoplist_len == 0 || !inHopList(link.id, hoplist, hoplist_len)){
			link_distance = getDistanceToStationOrSensor(link.x, link.y, destination_is_station,
				destination_sensor, destination_station);

			if(closest_link == NULL || link_distance < closest_link_distance){
				closest_link_distance = link_distance;
				closest_link = &link;
			}
		}
	}
	if(closest_link != NULL)
		return getBaseStation(closest_link->id);
	return NULL;
}

//Returns the closest sensor to the provided station where the provided station is in 
//the range of the sensor and the sensor is not in the hoplist
Sensor* getClosestValidSensor(char* hoplist, int hoplist_len, char* destination_id, bool destination_is_station,
	BaseStation station, BaseStation destination_station, Sensor destination_sensor){
	float closest_sensor_distance, sensor_distance;
	Sensor* closest_sensor = NULL;
	Sensor sensor;
	for(int i = 0; i < num_sensors; i++){
		sensor = sensors[i];
		printf("Iteration for sensor: %s\n", sensor.id);
		if(stationIsInRange(station, sensor) && (hoplist_len == 0 || !inHopList(sensor.id, hoplist, hoplist_len))){
			sensor_distance = getDistanceToStationOrSensor(sensor.x, sensor.y, destination_is_station,
				destination_sensor, destination_station);
			printf("hoplist_len: %d, inHopList returned %d for sensor %s\n",
			 hoplist_len, inHopList(sensor.id, hoplist, hoplist_len), sensor.id);

			if(closest_sensor == NULL || sensor_distance < closest_sensor_distance){
				closest_sensor_distance = sensor_distance;
				closest_sensor = &(sensors[i]);
		 		printf("Just found new closest sensor with id: %s\n", closest_sensor->id);
			}
		}
	}
	if(closest_sensor != NULL){
		printf("About to return closest sensor with id: %s\n", closest_sensor->id);
		return getSensor(closest_sensor->id);
	}
	return NULL;
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

bool inHopList(char* id, char* hoplist, int hoplist_len){
	//add each id from hoplist into array
	// char* token = strtok(hoplist, " ");
	// char** hoplist_ids = calloc(hoplist_len, sizeof(char*));
	// int num_ids_read = 0;
	// while(token != NULL) {
	// 	hoplist_ids[num_ids_read] = calloc(MAX_BUFFER, sizeof(char));
	// 	strcpy(hoplist_ids[num_ids_read], token);
	// 	num_ids_read++;
	// 	token = strtok(NULL, " ");
	// }
	// //check if id is in array of hoplist ids
	// for(int i = 0; i < hoplist_len; i++){
	// 	printf("ID: %s\n", id);
	// 	printf("hoplist_ids[%d]: %s\n", i, hoplist_ids[i]);
	// 	if(strcmp(id, hoplist_ids[i]) == 0){
	// 		return true;
	// 	}
	// }
	// return false;
	return (strstr(hoplist, id) != NULL);
}

Client getClientBySensorID(char* sensor_id){
	for(int i = 0; i < client_socket_index; i++){
		if(strcmp(clients[i].sensor_id, sensor_id) == 0)
			return clients[i];
	}
	fprintf(stderr, "Client was not found for some reason\n");
	exit(1);
}

bool destinationIsStation(char* destination_id){
	return (getBaseStation(destination_id) != NULL);
}