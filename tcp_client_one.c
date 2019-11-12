/* tcp-client.c */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main()
{
  /* create TCP client socket (endpoint) */
  int sd_one = socket( PF_INET, SOCK_STREAM, 0 );
  int sd_two = socket( PF_INET, SOCK_STREAM, 0 );

  if ( sd_one == -1 )
  {
    perror( "socket() failed" );
    exit( EXIT_FAILURE );
  }

#if 0
  struct hostent * hp = gethostbyname( "127.0.0.1" );
  struct hostent * hp = gethostbyname( "128.113.126.29" );
#endif

  struct hostent * hp = gethostbyname( "localhost" );  /* 127.0.0.1 */

  // struct hostent * hp = gethostbyname( "linux04.cs.rpi.edu" );

  if ( hp == NULL )
  {
    fprintf( stderr, "ERROR: gethostbyname() failed\n" );
    return EXIT_FAILURE;
  }

  struct sockaddr_in server_one;
  server_one.sin_family = AF_INET;
  memcpy( (void *)&server_one.sin_addr, (void *)hp->h_addr, hp->h_length );
  unsigned short port = 8124;
  server_one.sin_port = htons( port );

  printf( "server_one address is %s\n", inet_ntoa( server_one.sin_addr ) );


  printf( "connecting to server_one.....\n" );
  if ( connect( sd_one, (struct sockaddr *)&server_one, sizeof( server_one ) ) == -1 )
  {
    perror( "connect() failed" );
    return EXIT_FAILURE;
  }

  if ( connect( sd_two, (struct sockaddr *)&server_one, sizeof( server_one ) ) == -1 )
  {
    perror( "connect() failed" );
    return EXIT_FAILURE;
  }


  /* The implementation of the application layer protocol is below... */
  char* input = calloc(1024, sizeof(char));

  char* msg1 = "LOGIN Rick\n";
  char* msg2 = "LOGIN Morty\n";
  char* msg3 = "WHO\n";
  char* msg4 = "SEND Morty 21\nHere's a short fable!";
  char* msg5 = "SHARE Morty 917\n";
  char* msg6 = "sadfsadffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffdsfdsafsdasfasdasfasdfsfdvffgfhghjmghrweqrwersdjamfopmpdxmopxopd,sfmkfjkf;fjk;lsdsadfsadffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffdsfdsafsdasfasdasfasdfsfdvffgfhghjmghrweqrwersdjamfopmpdxmopxopd,sfmkfjkf;fjk;lsdsadfsadffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffdsfdsafsdasfasdasfasdfsfdvffgfhghjmghrweqrwersdjamfopmpdxmopxopd,sfmkfjkf;fjk;lsdsadfsadffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffdsfdsafsdasfasdasfasdfsfdvffgfhghjmghrweqrwersdjamfopmpdxmopxopd,sfmkfjkf;fjk;lsdsadfsadffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffdsfdsafsdasfasdasfasdfsfdvffgfhghjmghrweqrwersdjamfopmpdxmopxopd,sfmkfjkf;fjk;lsdsadfsadffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffdsfdsafsdasfasdasfasdfsfdvffgfhghjmghrweqrwersdjamfopmpdxmopxopd,sfmkfjkf;";
  char* msg7 = "SEND Rick 10\nmeaningful";
  char* msg8 = "LOGOUT\n";

  int n;
  char buffer[ BUFFER_SIZE ];

  //Client #1
  printf("sending [%s]\n", msg1);
  n = write( sd_one, msg1, strlen( msg1 ) );    /* or send()/recv() */
  n = read( sd_one, buffer, BUFFER_SIZE - 1 );    /* BLOCKING */
  printf("TCP rcvd: [%s]\n", buffer);
  // printf("sending [%s]\n", msg7);
  // n = write( sd_one, msg7, strlen( msg7 ));
  // n = read( sd_one, buffer, BUFFER_SIZE - 1 );
  // printf("TCP rcvd: [%s]\n", buffer);

  //Client #2
  printf("sending [%s]\n", msg2);
  n = write( sd_two, msg2, strlen( msg2 ) );
  n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  printf("sending [%s]\n", msg3);
  n = write( sd_two, msg3, strlen( msg3 ) );
  n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);

  //Client #1
  printf("sending [%s]\n", msg4);
  n = write( sd_one, msg4, strlen( msg4 ) );
  n = read( sd_one, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  printf("sending [%s]\n", msg5);
  n = write( sd_one, msg5, strlen( msg5 ) );
  n = read( sd_one, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  printf("sending [%s]\n", msg6);
  n = write( sd_one, msg6, strlen( msg6 ) );
  n = read( sd_one, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);

  //Client #2
  n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  printf("sending [%s]\n", msg7);
  n = write( sd_two, msg7, strlen( msg7 ) );
  n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  // printf("sending [%s]\n", msg12);
  // n = write( sd_two, msg12, strlen( msg12 ) );
  // n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  // printf("TCP rcvd: [%s]\n", buffer);
  // printf("sending [%s]\n", msg6);
  // n = write( sd_two, msg6, strlen( msg6 ) );
  // n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  // printf("TCP rcvd: [%s]\n", buffer);

  //Client #1
  printf("sending [%s]\n", msg8);
  n = write( sd_one, msg8, strlen( msg8 ) );
  n = read( sd_one, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);

  //Client #2
  printf("sending [%s]\n", msg8);
  n = write( sd_two, msg8, strlen( msg8 ) );
  n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  printf("TCP rcvd: [%s]\n", buffer);
  // printf("sending [%s]\n", test1_3);
  // n = write( sd_two, test1_3, strlen( test1_3 ) );
  // n = read( sd_two, buffer, BUFFER_SIZE - 1 );
  // printf("TCP rcvd: [%s]\n", buffer);


    // if ( n == -1 )
    // {
    //   perror( "read() failed" );
    //   return EXIT_FAILURE;
    // }
    // else if ( n == 0 )
    // {
    //   printf( "Rcvd no data; also, server_one socket was closed\n" );
    // }
    // else  /* n > 0 */
    // {
    //   buffer[n] = '\0';    /* assume we rcvd text-based data */
    //   printf( "Rcvd from server_one: %s\n", buffer );
    // }

    // n = write( sd_one, msg5, strlen( msg5 ));
    // n = read( sd_one, buffer, BUFFER_SIZE - 1 );    /* BLOCKING */
    // buffer[n] = '\0';
    // printf("Received from server_one: %s\n", buffer);
    // n = write( sd_one, msg5, strlen( msg5 ));
    // n = read( sd_one, buffer, BUFFER_SIZE - 1 );    /* BLOCKING */
    // buffer[n] = '\0';
    // printf("Received from server_one: %s\n", buffer);
    // n = write( sd_one, msg6, strlen( msg6 ));
    // n = read( sd_one, buffer, BUFFER_SIZE - 1 );    /* BLOCKING */
    // buffer[n] = '\0';
    // printf("Received from server_one: %s\n", buffer);


  // }

  close( sd_one );
  free(input);

  return EXIT_SUCCESS;
}
