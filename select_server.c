#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <pthread.h> 
#include <ctype.h>
#include <math.h>
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100      /* <===== */
typedef int bool;
#define true 1
#define false 0



//keeps track of the index of the client socket
//that will be passsed into the child thread
typedef struct {
  int i;
} Index;

//represents a logged in user
typedef struct{
  int fd;
  char* userid;
} User;

void* handleChild(void* p);
bool isAlphaNumStr(char* str, int len);
// Function to sort the array 
void sort(const char** arr, int n); 
static int myCompare(const void* a, const void* b);
bool isActiveUser(char* user_id);
User* createUser(char* userid, int fd);
void deleteUser(char* userid);

int client_sockets[ MAX_CLIENTS ]; /* client socket fd list */
int client_socket_index = 0;  /* next free spot */
fd_set readfds; //keeps track of the sockets that 'select will listen for'
User** active_users; //Keeps track of users that are logged in
int num_active_users = 0;

pthread_mutex_t lock; 

int z = 0;
int main(int argc, char const *argv[])
{
  //for submitty
  setvbuf( stdout, NULL, _IONBF, 0 );

  //Reading arguments
  if(argc != 2){
    perror("ERROR: Wrong number of arguments\n");
    exit( EXIT_FAILURE );
  }
  unsigned short port = atoi(argv[1]);

  //Creating listener sockets
  int tcp_sock = socket( PF_INET, SOCK_STREAM, 0 ); //TCP
  int udp_sock = socket( AF_INET, SOCK_DGRAM, 0 ); //UDP

  if ( tcp_sock < 0 )
  {
    perror( "ERROR: tcp socket() failed" );
    exit( EXIT_FAILURE );
  }
  if ( udp_sock < 0 )
  {
    perror( "ERROR: udp socket() failed" );
    exit( EXIT_FAILURE );
  }

  /* socket structures from /usr/include/sys/socket.h */
  //setting up servers
  struct sockaddr_in tcp_server;
  struct sockaddr_in client;
  struct sockaddr_in udp_server;
  tcp_server.sin_family = PF_INET;
  tcp_server.sin_addr.s_addr = INADDR_ANY;
  tcp_server.sin_port = htons( port );
  udp_server.sin_family = AF_INET;  /* IPv4 */
  udp_server.sin_addr.s_addr = htonl( INADDR_ANY );
  udp_server.sin_port = htons( port );
  int tcp_len = sizeof( tcp_server );
  int udp_len = sizeof( udp_server );

  //bind the servers
  if ( bind( tcp_sock, (struct sockaddr *)&tcp_server, tcp_len ) < 0 )
  {
    perror( "ERROR: tcp bind() failed" );
    exit( EXIT_FAILURE );
  }
  if ( bind( udp_sock, (struct sockaddr *)&udp_server, udp_len ) < 0 )
  {
    perror( "ERROR: udp bind() failed" );
    exit( EXIT_FAILURE );
  }

  //Identify this port a tcp listener port
  listen( tcp_sock, 32 );
  printf("MAIN: Started server\n");
  printf("MAIN: Listening for TCP connections on port: %d\n", port);
  printf("MAIN: Listening for UDP datagrams on port: %d\n", port);

  int fromlen = sizeof( client );
  char buffer[ BUFFER_SIZE ];
  int udp_num_bytes;

  //initialize the mutex
  if (pthread_mutex_init(&lock, NULL) != 0) 
    { 
        perror("\n mutex init has failed\n"); 
        return 1; 
    } 

  // active_users = calloc(32,sizeof(char*)); //max # of active users
  active_users = calloc(32,sizeof(User*));

  // pthread_t* child_thread_ids = calloc(32,sizeof(pthread_t)); //max # of child threads
  // int num_child_threads = 0;
  while ( 1 ){
    FD_ZERO( &readfds ); //initializes the file descriptor set
    FD_SET( tcp_sock, &readfds ); //select() will check tcp socket for activity
    FD_SET( udp_sock, &readfds ); //select() will check udp socket for activity
    // printf( "Set FD_SET to include listener fd %d for TCP\n", tcp_sock );
    // printf( "Set FD_SET to include listener fd %d for UDP\n", udp_sock );

    /* initially, this for loop does nothing; but once we have 
       client connections, we will add each client connection's fd 
       to the readfds (the FD set) 
       this will make select() check for activity on these sockets*/
    // for ( i = 0 ; i < client_socket_index ; i++ )
    // {
    //   FD_SET( client_sockets[ i ], &readfds );
    //   printf( "Set FD_SET to include client socket fd %d\n",
    //           client_sockets[ i ] );
    // }

    // printf("Blocking on select()\n");
    // int ready = select( FD_SETSIZE, &readfds, NULL, NULL, NULL ); //blocking
    select( FD_SETSIZE, &readfds, NULL, NULL, NULL ); //blocking
    /* ready is the number of ready file descriptors */
    // printf( "select() identified %d descriptor(s) with activity\n", ready );


    /* if there is a new connection being made on the tcp socket */
    if ( FD_ISSET( tcp_sock, &readfds ) ) {
      int newsock = accept( tcp_sock, (struct sockaddr *)&client, (socklen_t *)&fromlen );
      printf("MAIN: Rcvd incoming TCP connection from: %s\n", inet_ntoa( (struct in_addr)client.sin_addr ));
      // printf( "Accepted client connection\n" );
      client_sockets[ client_socket_index++ ] = newsock;
      //Create a new therad and pass the index of the new descriptor to the thread
      Index* ind = malloc(sizeof(Index));
      ind->i = client_socket_index-1;
      pthread_t tid;
      pthread_create(&tid, NULL, handleChild, (void*) ind);
      //add this thread to the list of child threads to be freed later
      // child_thread_ids[num_child_threads] = tid;
      // num_child_threads++;
    } 

    /* if there is a udp datagram that was sent */
    if( FD_ISSET(udp_sock, &readfds) ) { 
      // printf("UDP DATAGRAM WAS SENT\n");
      udp_num_bytes = recvfrom( udp_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &client,
                    (socklen_t *)&fromlen );
      printf("MAIN: Rcvd incoming UDP datagram from: %s\n", inet_ntoa( (struct in_addr)client.sin_addr ));
  
      if ( udp_num_bytes == -1 ){

        perror( "ERROR: recvfrom() failed" );

      } else {

        buffer[udp_num_bytes] = '\0';    

        if(strstr(buffer,"WHO") != NULL){

          printf("MAIN: Rcvd WHO request\n");

          //temporarily place all userids in a 2d array
          char** tmp = calloc(num_active_users, sizeof(char*));
          int len = 4; //OK!\n
          for(int i = 0; i < num_active_users; i++){
            char* id = active_users[i]->userid;
            tmp[i] = calloc(strlen(id)+1,sizeof(char));
            strcpy(tmp[i],id);
            len += (strlen(id) + 1);
          }
          sort((const char**)tmp,num_active_users);

           //send the string back to the user
           char* str = calloc(len+1, sizeof(char));
           strcat(str, "OK!\n");
           for(int i = 0; i < num_active_users; i++){
             strcat(str, tmp[i]);
             strcat(str, "\n");
           }
           udp_num_bytes = sendto(udp_sock, str, len, 0, (struct sockaddr *) &client, fromlen );
           if(udp_num_bytes != len)
             perror("send() failed");

          //free the temporarily allocated memory
          free(str);
          for(int i = 0; i < num_active_users; i++){
            free(tmp[i]);
          }
          free(tmp);
          /* to do: check the return code of sendto() */    

        } else if(strstr(buffer,"SEND") != NULL){

          printf("MAIN: Rcvd SEND request\n");

          //may have to send not just print
          udp_num_bytes = sendto(udp_sock, "ERROR SEND not supported over UDP", 33, 
            0, (struct sockaddr *) &client, fromlen);          
          // perror("ERROR SEND not supported over UDP");
          continue;

        } else if(strstr(buffer,"BROADCAST") != NULL){

          printf("MAIN: Rcvd BROADCAST request\n");

          //make sure broadcast is in the right format
          if(strlen(buffer) < 13){
            sendto(udp_sock, "ERROR Invalid BROADCAST format\n", 31, 0, (struct sockaddr *) &client, fromlen);
            printf("SENT ERROR (Invalid BROADCAST format)\n");
            continue;
          }
          int num_spaces = 0, i = 0;
          int space_index;
          //read until the newline character
          while(1){
            if(buffer[i] == '\n')
              break;
            if(buffer[i] == ' '){
              if(num_spaces < 1)
                space_index = i; 
              num_spaces++;
            }
            i++;
          }
          if(num_spaces != 1){
            sendto(udp_sock, "ERROR Invalid BROADCAST format\n", 31, 0, (struct sockaddr *) &client, fromlen);
            printf("SENT ERROR (Invalid BROADCAST format)\n");
            continue;
          }

          int newline_index = i;

          //get the message length
          int msglen_str_len = newline_index - (space_index+1);
          char* msglen_str = calloc(msglen_str_len+1, sizeof(char));
          strncpy(msglen_str, buffer+space_index+1, msglen_str_len);
          msglen_str[msglen_str_len] = '\0';
          int msglen = (int) strtol(msglen_str, (char **)NULL, 10); // convert to int
          if(msglen < 1 || msglen > 990){
            free(msglen_str);
            sendto(udp_sock, "ERROR Invalid msglen\n", 21, 0, (struct sockaddr *) &client, fromlen);
            printf("SENT ERROR (Invalid msglen)\n");
            continue;
          }

          //get the message
          char* msg = calloc(msglen+1, sizeof(char));
          strncpy(msg, buffer+newline_index+1, msglen);
          msg[msglen] = '\0';

          //send the message to all users
          int whole_msg_len = 5 + 10 + 1 + strlen(msglen_str) + 1 + msglen + 1;
          char* whole_msg = calloc(whole_msg_len+1, sizeof(char));
          strcat(whole_msg, "FROM ");
          strcat(whole_msg, "UDP-client");
          strcat(whole_msg, " ");
          strcat(whole_msg, msglen_str);
          strcat(whole_msg, " ");
          strcat(whole_msg, msg);
          strcat(whole_msg, "\n");
          // printf("Whole msg len: %d\n", whole_msg_len);
          int msg_fd;
          for(int i = 0; i < num_active_users; i++){
            msg_fd = active_users[i]->fd;
            udp_num_bytes = sendto(msg_fd, whole_msg, whole_msg_len, 0, (struct sockaddr *) &client, fromlen);
          }
          free(msglen_str);
          free(msg);
          free(whole_msg);          

        } else if(strstr(buffer,"SHARE") != NULL){

          printf("MAIN: Rcvd SHARE request\n");

          udp_num_bytes = sendto(udp_sock, "ERROR SHARE not supported over UDP", 34, 
            0, (struct sockaddr *) &client, fromlen);
          // printf("ERROR Share not supported over UDP\n");
          continue;

        }

      }
    }
  }
  pthread_mutex_destroy(&lock); 
  // for(int i = 0; i < num_child_threads; i++){
  //   if(child_thread_ids[i] == NULL)
  //     break;
  //   else
  //     pthread_join(child_thread_ids[i], NULL);
  // }
  // free(child_thread_ids);

  //TODO--------FREE ALL ACTIVE USERS

  return EXIT_SUCCESS; /* we never get here */
}

void* handleChild(void* p){
  //Get the index of this socket
  Index* ind = (Index*) p;
  int index = ind->i;
  int fd = client_sockets[ index ]; //Get the file descriptor at this index
  int tcp_num_bytes;
  char buffer[ BUFFER_SIZE ];

  /* is there activity on any of the established connections? */
  // printf("\nJUST ENTERED NEW THREAD\n");
  bool is_logged_in = false;
  // printf("Set is logged in bool\n");
  User* current_user;
  while(1){
    // printf("Blocking on recv in child thread %lu\n", pthread_self());
    // printf("Name before recv: %s\n", name);
    tcp_num_bytes = recv( fd, buffer, BUFFER_SIZE - 1, 0 );
    // printf("Name after recv: %s\n", name);

    if ( tcp_num_bytes < 0 ){

      perror( "recv()" );

    }else if ( tcp_num_bytes == 0 ){

      pthread_mutex_lock(&lock);
      int k;
      close( fd );

      if(is_logged_in){
        deleteUser(current_user->userid);
        is_logged_in = false;  
      }   

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
      printf("CHILD %lu: Client disconnected\n", pthread_self());
      // printf("NUMBER OF CONNECTIONS: %d\n", client_socket_index);
      break;

    }else{

      // printf( "Received message: %s\n", buffer );


      buffer[tcp_num_bytes] = '\0';
      if(!is_logged_in){

        if(strstr(buffer,"LOGIN") != NULL){ //login command

          char* name = strstr(buffer,"LOGIN") + 6;
          printf("CHILD %lu: Rcvd LOGIN request for userid %s", pthread_self(), name);
          //remove newline char added by strstr
          name[strcspn(name,"\n")] = 0;
          int name_length = strlen(name);
          if((name_length < 4 || name_length > 16) || !isAlphaNumStr(name, name_length)){
            send(fd, "ERROR Invalid userid\n", 21,0);
            printf("CHILD %lu: Sent ERROR (Invalid userid)\n", pthread_self());
            continue;
          }
          //check if the user is already logged in
          if(isActiveUser(name)){
            send(fd, "ERROR Already connected\n", 24, 0);
            printf("CHILD %lu: Sent ERROR (Already connected)\n", pthread_self());
            continue;
          } else{

            is_logged_in = true;
            pthread_mutex_lock(&lock);
            current_user = createUser(name,fd);
            pthread_mutex_unlock(&lock);
            // printf("Num active users: %d\n", num_active_users);
            tcp_num_bytes = send( fd, "OK!\n", 4, 0 );
            if ( tcp_num_bytes != 4 )
              perror( "send() failed" );
  
          }

        } else {

          perror("ERROR: Unauthorized user");
          continue;

        }

      } else { // user is logged in

          if(strstr(buffer,"LOGIN") != NULL){

            char* name = strstr(buffer,"LOGIN") + 6;
            printf("CHILD %lu: Rcvd LOGIN request for userid %s", pthread_self(), name);
            send(fd, "ERROR Already connected\n", 24, 0);
            printf("CHILD %lu: Sent ERROR (Already connected)\n", pthread_self());
            // perror("ERROR: Cannot log in as another user");
            continue;

          }else if(strstr(buffer,"WHO") != NULL){

            printf("CHILD %lu: Rcvd WHO request\n", pthread_self());

            //temporarily place all userids in a 2d array
            char** tmp = calloc(num_active_users, sizeof(char*));
            int len = 4; //OK!\n
            for(int i = 0; i < num_active_users; i++){
              char* id = active_users[i]->userid;
              tmp[i] = calloc(strlen(id)+1,sizeof(char));
              strcpy(tmp[i],id);
              len += (strlen(id) + 1);
            }
            sort((const char**)tmp,num_active_users);

            //send the string back to the user
            char* str = calloc(len+1, sizeof(char));
            strcat(str, "OK!\n");
            for(int i = 0; i < num_active_users; i++){
              strcat(str, tmp[i]);
              strcat(str, "\n");
            }
            tcp_num_bytes = send(fd, str, len, 0);
            if(tcp_num_bytes != len)
              perror("send() failed");

            //free the temporarily allocated memory
            free(str);
            for(int i = 0; i < num_active_users; i++){
              free(tmp[i]);
            }
            free(tmp);

          } else if(strstr(buffer,"LOGOUT") != NULL){

            printf("CHILD %lu: Rcvd LOGOUT request:\n", pthread_self());
            pthread_mutex_lock(&lock);
            deleteUser(current_user->userid);
            pthread_mutex_unlock(&lock);
            send(fd, "OK!\n", 4, 0);
            is_logged_in = false;
            // printf("CHILD %lu: Client disconnected\n", pthread_self());

          } else if(strstr(buffer,"SEND") != NULL){

            //make sure send is in the right format
            if(strlen(buffer) < 10){
              send(fd, "ERROR Invalid SEND format", 25, 0);
              printf("CHILD %lu: Sent ERROR (Invalid SEND format)\n", pthread_self());
              continue;
            }
            int num_spaces = 0, i = 0;
            int* space_indices = calloc(2, sizeof(int)); //keeps track of the index of each space in buffer
            //read until the newline character
            while(1){
              if(buffer[i] == '\n')
                break;
              if(buffer[i] == ' '){
                if(num_spaces < 2)
                  space_indices[num_spaces] = i; 
                num_spaces++;
              }
              i++;
            }
            if(num_spaces != 2){
              free(space_indices);
              send(fd, "ERROR Invalid SEND format", 25, 0);
              printf("CHILD %lu: Sent ERROR (Invalid SEND format)\n", pthread_self());
              continue;
            }
            int space_one_index = space_indices[0];
            int space_two_index = space_indices[1];
            int newline_index = i;

            //get the userid
            int userid_len = space_two_index - (space_one_index+1);
            char* userid = calloc(userid_len+1, sizeof(char));
            strncpy(userid, buffer+space_one_index+1, userid_len);
            userid[userid_len] = '\0';
            printf("CHILD %lu: Rcvd SEND request to userid %s\n", pthread_self(), userid);
            if(!isActiveUser(userid)){
              free(userid);
              send(fd, "ERROR Unknown userid", 20, 0);
              printf("CHILD %lu: Sent ERROR (Unknown userid)\n", pthread_self());
              continue;
            }

            //get the message length
            int msglen_str_len = newline_index - (space_two_index+1);
            char* msglen_str = calloc(msglen_str_len+1, sizeof(char));
            strncpy(msglen_str, buffer+space_two_index+1, msglen_str_len);
            msglen_str[msglen_str_len] = '\0';
            int msglen = (int) strtol(msglen_str, (char **)NULL, 10); // convert to int
            if(msglen < 1 || msglen > 990){
              free(msglen_str);
              send(fd, "ERROR Invalid msglen", 20, 0);
              printf("CHILD %lu: Sent ERROR (ERROR Invalid msglen)\n", pthread_self());
              continue;
            }


            //get the message
            char* msg = calloc(msglen+1, sizeof(char));
            strncpy(msg, buffer+newline_index+1, msglen);
            msg[msglen] = '\0';

            //send the message to the right user
            int msg_fd;
            for(int i = 0; i < num_active_users; i++){
              if(strcmp(active_users[i]->userid,userid) == 0){
                msg_fd = active_users[i]->fd;
                break;
              }
            }
            int whole_msg_len = 5 + strlen(current_user->userid) + 1 + strlen(msglen_str) + 1 + msglen + 1;
            char* whole_msg = calloc(whole_msg_len+1, sizeof(char));
            strcat(whole_msg, "FROM ");
            strcat(whole_msg, current_user->userid);
            strcat(whole_msg, " ");
            strcat(whole_msg, msglen_str);
            strcat(whole_msg, " ");
            strcat(whole_msg, msg);
            strcat(whole_msg, "\n");
            // printf("Whole msg len: %d\n", whole_msg_len);
            tcp_num_bytes = send(msg_fd, whole_msg, whole_msg_len, 0);
            send(fd, "OK!\n", 4, 0);
            free(userid);
            free(msglen_str);
            free(msg);
            free(whole_msg);
 
          } else if(strstr(buffer,"BROADCAST") != NULL){

            //make sure broadcast is in the right format
            if(strlen(buffer) < 13){
              send(fd, "ERROR Invalid BROADCAST format\n", 31, 0);
              printf("CHILD %lu: Sent ERROR Invalid BROADCAST format\n", pthread_self());
              continue;
            }
            int num_spaces = 0, i = 0;
            int space_index;
            //read until the newline character
            while(1){
              if(buffer[i] == '\n')
                break;
              if(buffer[i] == ' '){
                if(num_spaces < 1)
                  space_index = i; 
                num_spaces++;
              }
              i++;
            }
            if(num_spaces != 1){
              send(fd, "ERROR Invalid BROADCAST format\n", 31, 0);
              printf("CHILD %lu: Sent ERROR Invalid BROADCAST format\n", pthread_self());
              continue;
            }

            int newline_index = i;

            //get the message length
            int msglen_str_len = newline_index - (space_index+1);
            char* msglen_str = calloc(msglen_str_len+1, sizeof(char));
            strncpy(msglen_str, buffer+space_index+1, msglen_str_len);
            msglen_str[msglen_str_len] = '\0';
            int msglen = (int) strtol(msglen_str, (char **)NULL, 10); // convert to int
            if(msglen < 1 || msglen > 990){
              free(msglen_str);
              perror("ERROR Invalid msglen");
              continue;
            }

            //get the message
            char* msg = calloc(msglen+1, sizeof(char));
            strncpy(msg, buffer+newline_index+1, msglen);
            msg[msglen] = '\0';

            //send the message to all users
            int whole_msg_len = 5 + strlen(current_user->userid) + 1 + strlen(msglen_str) + 1 + msglen + 1;
            char* whole_msg = calloc(whole_msg_len+1, sizeof(char));
            strcat(whole_msg, "FROM ");
            strcat(whole_msg, current_user->userid);
            strcat(whole_msg, " ");
            strcat(whole_msg, msglen_str);
            strcat(whole_msg, " ");
            strcat(whole_msg, msg);
            strcat(whole_msg, "\n");
            printf("Whole msg len: %d\n", whole_msg_len);
            int msg_fd;
            for(int i = 0; i < num_active_users; i++){
              msg_fd = active_users[i]->fd;
              tcp_num_bytes = send(msg_fd, whole_msg, whole_msg_len, 0);
            }
            free(msglen_str);
            free(msg);
            free(whole_msg);

          } else if(strstr(buffer,"SHARE") != NULL){

            printf("CHILD %lu: Rcvd SHARE request:\n", pthread_self());

            //make sure broadcast is in the right format
            if(strlen(buffer) < 10){
              send(fd, "ERROR Invalid SHARE format\n", 27, 0);
              printf("CHILD %lu: Sent ERROR Invalid SHARE format\n", pthread_self());
              continue;
            }
            int num_spaces = 0, i = 0;
            int* space_indices = calloc(2, sizeof(int)); //keeps track of the index of each space in buffer
            //read until the newline character
            while(1){
              if(buffer[i] == '\n')
                break;
              if(buffer[i] == ' '){
                if(num_spaces < 2)
                  space_indices[num_spaces] = i; 
                num_spaces++;
              }
              i++;
            }
            if(num_spaces != 2){
              free(space_indices);
              send(fd, "ERROR Invalid SHARE format\n", 27, 0);
              printf("CHILD %lu: Sent ERROR Invalid SHARE format\n", pthread_self());
              continue;
            }
            int space_one_index = space_indices[0];
            int space_two_index = space_indices[1];
            int newline_index = i;

            //get the userid
            int userid_len = space_two_index - (space_one_index+1);
            char* userid = calloc(userid_len+1, sizeof(char));
            strncpy(userid, buffer+space_one_index+1, userid_len);
            userid[userid_len] = '\0';
            if(!isActiveUser(userid)){
              free(userid);
              send(fd, "ERROR Unknown userid\n", 21, 0);
              printf("CHILD %lu: Sent ERROR Unknown userid\n", pthread_self());
              continue;
            }

            //get the file length
            int filelen_str_len = newline_index - (space_two_index+1);
            char* filelen_str = calloc(filelen_str_len+1, sizeof(char));
            strncpy(filelen_str, buffer+space_two_index+1, filelen_str_len);
            filelen_str[filelen_str_len] = '\0';
            int filelen = (int) strtol(filelen_str, (char **)NULL, 10); // convert to int
            // printf("Recipient userid: %s, file len: %d\n", userid, filelen);
            tcp_num_bytes = send(fd, "OK!\n", 4, 0);

            // for(int i = 0; i < 1024; i++){
            //   buffer[i] = ' ';
            // }

            int num_chunks; //keeps track of the number of 1024 byte chunks that will be received
            if(filelen < 1024)
              num_chunks = 1;
            else
              num_chunks = (filelen + 1024 - 1) / 1024;
            // printf("num chunks: %d\n", num_chunks);

            //temporarily store each chunk
            char** chunks = calloc(num_chunks, sizeof(char*));

            //read in the file 1024 bytes at a time
            for(int i = 0; i < num_chunks; i++){
              tcp_num_bytes = recv( fd, buffer, BUFFER_SIZE, 0 );
              // printf("Just received from buffer: %s\n", buffer);
              send(fd, "OK!\n", 4, 0);
              chunks[i] = calloc(tcp_num_bytes+1, sizeof(char));
              //copy the temporary string
              for(int j = 0; j < tcp_num_bytes; j++){
                chunks[i][j] = buffer[j];
              }
            }
            //send the file to the specified user
            int msg_fd;
            for(int i = 0; i < num_active_users; i++){
              if(strcmp(active_users[i]->userid,userid) == 0){
                msg_fd = active_users[i]->fd;
                break;
              }
            }
            for(int i = 0; i < num_chunks; i++){
              send(msg_fd, chunks[i], strlen(chunks[i]), 0);
              free(chunks[i]);
            }
            free(chunks);

          }

      }

      // for(int i = 0; i < num_active_users; i++){
      //   User* u = active_users[i];
      //   printf("User [%d]: %s\n", i, u->userid);
      // }


    }

  }

  pthread_exit(NULL);
}

bool isAlphaNumStr(char* str, int len){
  for(int i = 0; i < len; i++){
    if(!isalnum(str[i]))
      return false;
  }
  return true;
}

// Function to sort the array 
void sort(const char** arr, int n){ 
    // calling qsort function to sort the array 
    // with the help of Comparator 
    qsort(arr, n, sizeof(const char*), myCompare); 
} 

static int myCompare(const void* a, const void* b){ 
  
    // setting up rules for comparison 
    return strcmp(*(const char**)a, *(const char**)b); 
}

bool isActiveUser(char* user_id){
  for(int i = 0; i < num_active_users; i++){
    char* id = active_users[i]->userid;
    if(strcmp(id, user_id) == 0)
      return true;
  }
  return false;
}

User* createUser(char* userid, int fd){
  User* u = malloc(sizeof(User));
  u->userid = calloc(strlen(userid)+1, sizeof(char));
  strcpy(u->userid,userid);
  u->fd = fd;
  active_users[num_active_users] = u;
  num_active_users++;
  return u;
}

void deleteUser(char* userid){
  for(int i = 0; i < num_active_users; i++){
    User* u = active_users[i];
    if(strcmp(u->userid,userid) == 0){
      for(int j = i; j < num_active_users-1; j++){
        active_users[j]->fd = active_users[j+1]->fd;
        active_users[j]->userid = realloc(active_users[j]->userid, strlen(active_users[j+1]->userid)*sizeof(char));
        strcpy(active_users[j]->userid,active_users[j+1]->userid);
      }
    }
  }
  num_active_users--;
  free(active_users[num_active_users]->userid);
  free(active_users[num_active_users]);
  active_users[num_active_users] = NULL;
}
