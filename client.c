
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#define HOST "127.0.0.1"

extern int errno;
int port;

void * handle_commands(void * arg) {
  int buffer_size = 2000;
  char buffer[buffer_size];
  int socket = *(int *)arg;

  while (1) {
    bzero(buffer, buffer_size);

    if (read(socket, buffer, buffer_size) < 0) {
      perror("ERROR reading from socket");
      exit(1);
    }

    if (buffer[0] == 'm') {
      if (buffer[2] == '1') {
        printf("Your move: ");
      } else {
        printf("Waiting for the opponent...");
      }
    } else if (buffer[0] == 'e') {
      if (buffer[2] == '1') {
        printf("Syntax error! A valid move should have the following syntax: A2->A3\n");
      } else {
        printf("Invalid move\n");
      }
    } else if (buffer[0] == 'c') {
      printf("Check! You have %c available move(s) before checkmate!\nYour move: ", buffer[2]);
    } else if (strcmp(buffer, "game over") == 0) {
      printf("You lost!\n");
    } else {
      system("clear");
      printf("%s\n", buffer);
    }
  }
}

int main (int argc, char *argv[])
{
  setbuf(stdout, NULL); // ca sa nu mai fie problema cu mesajul care apare mai tarziu
  int sd;
  struct sockaddr_in server; 
  char command[1000];

  if (argc != 2) {
    printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  port = atoi (argv[1]);

  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {
    perror ("Eroare la socket().\n");
    return errno;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(HOST);
  server.sin_port = htons (port);
  
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
  {
    perror ("[client]Eroare la connect().\n");
    return errno;
  }
  printf("Connected to the server\n");

  pthread_t th[100];

  // Response thread
  pthread_create(&th[0], NULL, &handle_commands, &sd);

  while (1) {
    bzero(command, 64);
    fflush(stdout);
    fgets(command, 64, stdin);

    if (send(sd, command, strlen(command), 0) < 0) {
      perror("ERROR writing to socket");
      exit(1);
    }
  }
    
  close(sd);
  return 0;
}