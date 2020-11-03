#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>

#define PORT 2908
#define MAX_CLIENTS 2
#define SOCKET_WRITE_ERROR "ERROR writing to socket"
#define SOCKET_READ_ERROR "ERROR reading from socket"
#define INVALID_SYNTAX "e-1"
#define INVALID_MOVE "e-2"
#define EMPTY_CELL "__"

extern int errno;

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

static void *treat(void *);

pthread_cond_t player_to_join;
pthread_mutex_t global_mutex_lock;
int challenging_player = 0;
int player_in_lobby = 0;

char* initial_board[9][9] = {
  {"0", " A", " B", " C", " D", " E", " F", " G", " H"},
  {"1", "TA", "CA", "NA", "KA", "QA", "NA", "CA", "TA"},
  {"2", "PA", "PA", "PA", "PA", "PA", "PA", "PA", "PA"},
  {"3", "__", "__", "__", "__", "__", "__", "__", "__"},
  {"4", "__", "__", "__", "__", "__", "__", "__", "__"},
  {"5", "__", "__", "__", "__", "__", "__", "__", "__"},
  {"6", "__", "__", "__", "__", "__", "__", "__", "__"},
  {"7", "PN", "PN", "PN", "PN", "PN", "PN", "PN", "PN"},
  {"8", "TN", "CN", "NN", "KN", "QN", "NN", "CN", "TN"},
};

int is_valid_syntax(char * move) {
  // Valid move syntax: A2->A3
  if (strlen(move) != 7) return 0; // including /n
  if (move[2] != '-' || move[3] != '>') return 0;
  if (move[1] - '0' > 8 || move[5] - '0' > 8) return 0;
  if (move[0] - '0' > 56 || move[4] - '0' > 56) return 0;

  return 1;
}

void display_chess_board(int player_one, int player_two) {
  int i, j, board_size = 256;
  char chessboard[board_size];
  chessboard[0] = '\0';
  for (i = 0; i < 9; i++) {
    for (j = 0; j < 9; j++) {
      strcat(chessboard, initial_board[i][j]);
      if (j < 8) strcat(chessboard, " ");
      else strcat(chessboard, "\n");
    }
  }
  send(player_one, chessboard, board_size, 0);
  send(player_two, chessboard, board_size, 0);
  printf("Board is displayed to both players\n");
}

char* get_king_for_player(int player) {
  return player == 1 ? "KA" : "KN";
}

char* get_king_position(int player) {
  int i, j;
  char* position = malloc(3);
  char* king = malloc(3);
  king = get_king_for_player(player);
  for (i = 1; i < 9; i++) {
    for (j = 1; j < 9; j++) {
      if (strcmp(initial_board[i][j], king) == 0) sprintf(position, "%d%d", i, j);
    }
  }
  
  return position;
}

int is_line_check(char* king_position, int player) {
  // verificam daca e sah pe linie (tura sau regina)
  int i, column = king_position[1] - '0', row = king_position[0] - '0';
  char *piece = malloc(3), *king = malloc(3);
  king = get_king_for_player(player);

  printf("Line check start\n");

  for (i = row - 1; i > 1; i--) {
    piece = initial_board[i][column];
    printf("Row check 1, piece %s\n", piece);
    if (strcmp(piece, king) == 0 || strcmp(piece, EMPTY_CELL) == 0) continue;
    if ((piece[1] == 'A' && player == 1) || (piece[1] == 'N' && player == 2)) return 0;
    if (piece[0] == 'T' || piece[0] == 'Q') return 1;
    else continue;
  }

  for (i = row + 1; i < 9 ; i++) {
    piece = initial_board[i][column];
    printf("Row check 2, piece %s\n", piece);
    if (strcmp(piece, king) == 0 || strcmp(piece, EMPTY_CELL) == 0) continue;
    if ((piece[1] == 'A' && player == 1) || (piece[1] == 'N' && player == 2)) return 0;
    if (piece[0] == 'T' || piece[0] == 'Q') return 1;
    else continue;
  }

  for (i = column - 1; i > 1; i--) {
    piece = initial_board[row][i];
    if (strcmp(piece, king) == 0 || strcmp(piece, EMPTY_CELL) == 0) continue;
    if ((piece[1] == 'A' && player == 1) || (piece[1] == 'N' && player == 2)) return 0;
    if (piece[0] == 'T' || piece[0] == 'Q') return 1;
    else continue;
  }

  for (i = column + 1; i < 9 ; i++) {
    piece = initial_board[row][i];
    if (strcmp(piece, king) == 0 || strcmp(piece, EMPTY_CELL) == 0) continue;
    if ((piece[1] == 'A' && player == 1) || (piece[1] == 'N' && player == 2)) return 0;
    if (piece[0] == 'T' || piece[0] == 'Q') return 1;
    else continue;
  }

  return 0;
}

int is_out_of_bounds(char *king_position, int *position) {
  int column = king_position[1] - '0', row = king_position[0] - '0';
  if (row + position[0] < 0 || row + position[0] > 8 || column + position[1] < 0 || column + position[1] > 8) return 1;
  return 0;
}

int is_L_check(char* king_position, int player) {
  //printf("Start L check\n");
  char *king = malloc(3), *enemy_piece = malloc(3);
  king = get_king_for_player(player);
  enemy_piece = player == 1 ? "CN" : "CA";
  int i, possible_knight_positions[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
  int column = king_position[1] - '0', row = king_position[0] - '0';
  for (i = 0; i < 8; i++) {
    int *position = possible_knight_positions[i];
    //printf("L check for cell %d%d, position: %d%d\n", row, column, position[0], position[1]);
    if (is_out_of_bounds(king_position, position)) continue;
    if (strcmp(initial_board[row + position[0]][column + position[1]], enemy_piece) == 0) return 1;
  }
  //printf("L check end\n");
  return 0;
}

int get_covered_distance(char origin, char destination) {
  int x = origin - '0';
  int y = destination - '0';
  return abs(x - y);
}

char * convert_buffer_to_move(char *buffer) {
  char *toReturn = malloc(4);
  sprintf(toReturn, "%d%d%d%d", buffer[1] - '0', buffer[0] - 'A' + 1, buffer[5] - '0', buffer[4] - 'A' + 1);
  return toReturn;
}

void move_piece_on_board(char *move) {
  initial_board[move[2] - '0'][move[3] - '0'] = initial_board[move[0] - '0'][move[1] - '0'];
  initial_board[move[0] - '0'][move[1] - '0'] = EMPTY_CELL;
}

char * get_selected_piece(char *move) {
  char *piece;
  piece = initial_board[move[0] - '0'][move[1] - '0'];
  return piece;
}

int is_first_move(int player, char *move) {
  if ((player == 1 && move[0] == '2') || (player == 2 && move[0] == '7')) return 1;
  return 0;
}

int is_move_valid(int player, char *piece, char* move) {
  printf("Validating move for player %d, piece %s, move %s\n", player, piece, move);
  if (strcmp(piece, EMPTY_CELL) == 0) return 0;
  if ((piece[1] == 'A' && player != 1) || (piece[1] == 'N' && player != 2)) return 0;

  char *destinationPiece;
  destinationPiece = initial_board[move[2] - '0'][move[3] - '0'];
  if ((destinationPiece[1] == 'A' && player == 1) || (destinationPiece[1] == 'N' && player == 2)) return 0;

  char pieceType = piece[0];
  int verticalDistance = get_covered_distance(move[0], move[2]);
  int horizontalDistance = get_covered_distance(move[1], move[3]);
  printf("Distances: %d - %d\n", verticalDistance, horizontalDistance);
  switch (pieceType) {
    case 'K': {
      if (horizontalDistance > 1 || verticalDistance > 1) return 0;
      break;
    }
    case 'T': {
      if (horizontalDistance > 0 && verticalDistance > 0) return 0;
      int i;
      // verificam daca sunt piese pe parcurs
      for (i = move[0] - '0' + 1; i < move[2] - '0'; i++) {
        // daca avem orice piesa pe drum, miscarea este invalida
        if (strcmp(initial_board[i][move[1] - '0'], EMPTY_CELL)) return 0;
      }
      for (i = move[1] - '0' + 1; i < move[3] - '0'; i++) {
        if (strcmp(initial_board[move[0] - '0'][i], EMPTY_CELL)) return 0;
      }
      // trebuie verificat si in sens opus
      break;
    }
    case 'N': {
      if (abs(horizontalDistance) - abs(verticalDistance) != 0) return 0;
      // trebuie verificat daca exista piesa pe drum
      break;
    }
    case 'C': {
      if (
        (abs(horizontalDistance) != 1 || abs(verticalDistance) != 2) &&
        (abs(horizontalDistance) != 2 || abs(verticalDistance) != 1)
      ) {
        return 0;
      }
      break;
    }
    case 'P': {
      if (verticalDistance > 2) return 0; // mai mult de doua spatii
      if (verticalDistance == 2 && !is_first_move(player, move)) return 0; // doua spatii in afara primei miscari
      if (
        (player == 1 && move[0] - '0' > move[2] - '0') ||
        (player == 2 && move[0] - '0' < move[2] - '0')
      ) return 0; // miscare inapoi
      if (horizontalDistance > 0) {
        if (verticalDistance != 1) return 0; // orice miscare diagonala invalida
        if (strcmp(initial_board[move[2] - '0'][move[3] - '0'], EMPTY_CELL) == 0) return 0; // miscare diagonala pe o celula goala 
      } else if (strcmp(initial_board[move[2] - '0'][move[3] - '0'], EMPTY_CELL)) return 0; // piesa in fata
      break;
    }
    case 'Q': {
      return 1; // trebuie verificata miscarea calului care este invalida
    }
    default:
      return 0;
  }
  return 1;
}

int is_check(int player) {
  char *king_position = malloc(3);
  king_position = get_king_position(player);

  printf("Got king position %s for player %d\n", king_position, player);

  return (is_line_check(king_position, player) || is_L_check(king_position, player));
}

static void *treat(void * arg) {		
  int player_one = *(int*)arg;
  int player_two;
  int buffer_size = 128;
  char buffer[buffer_size];
  char *move, *message = malloc(3);
  int valid_syntax, valid_move, remaining_moves;

  player_in_lobby = 1;

  if (send(player_one, "Welcome! Waiting for another player to join...\n", 64, 0) < 0) {
     perror(SOCKET_WRITE_ERROR);
     exit(1);
  }
  
  pthread_mutex_lock(&global_mutex_lock); // Wait for player two
  pthread_cond_wait(&player_to_join, &global_mutex_lock); // Wait for player wants to join signal 

  player_two = challenging_player; // Asign the player_two to challenging_player
  player_in_lobby = 0; // Now none is waiting

  pthread_mutex_unlock(&global_mutex_lock); 

  if (send(player_two, "Welcome! Joining a game...\n", 64, 0) < 0) {
     perror(SOCKET_WRITE_ERROR);
     exit(1);
  }

  sleep(1);

  display_chess_board(player_one, player_two);

  while (1) {
    send(player_two, "m-2", 3, 0);

    //-------------START Player 1------------------

    printf("Player one moves...\n");

    valid_move = 0;
    valid_syntax = 0;
    remaining_moves = 2;

    while(!valid_move || !valid_syntax) {
      bzero(buffer, buffer_size);

      printf("While start for player 1...\n");
      int is_check_for_player = is_check(1);

      if (is_check_for_player) {
        printf("Player one check\n");
        sprintf(message, "c-%d", remaining_moves);
        send(player_one, message, 3, 0);
      } else {
        send(player_one, "m-1", 3, 0);
      }

      if (read(player_one, buffer, 7) < 0) {
        perror(SOCKET_READ_ERROR);
        exit(1);
      }

      printf("Player one made the following move: %s\n", buffer);

      printf("Validating syntax\n");
      valid_syntax = is_valid_syntax(buffer);

      if (!valid_syntax) {
        send(player_one, INVALID_SYNTAX, 4, 0);
        printf("Invalid syntax\n");
        continue;
      }

      move = convert_buffer_to_move(buffer);
      printf("Converted move for player one: %s\n", move);

      printf("Validating move\n");
      valid_move = is_move_valid(1, get_selected_piece(move), move);

      if (!valid_move) {
        send(player_one, INVALID_MOVE, 4, 0);
        printf("Invalid move\n");
        continue;
      } else {
        printf("Valid move\n");
      }

      if (is_check_for_player) {
        printf("Board copy...\n");
        // copiem starea curenta a tablei
        char* board_copy[9][9];
        int i, j;
        memcpy(board_copy, initial_board, sizeof(board_copy));
        // simulam miscarea pe tabla
        move_piece_on_board(move);
        printf("Move simulation done\n");
        // verificam din nou daca este sah
        if (is_check(1)) {
          remaining_moves--;
          if (!remaining_moves) {
            send(player_one, "game over", 9, 0);
            printf("---------Player two wins!-----------\n");
            close(player_one);
            close(player_two);
          } else {
            valid_move = 0;
            memcpy(initial_board, board_copy, sizeof(initial_board));
          }
        } else {
          printf("Valid move\n");
          valid_move = 1;
          memcpy(initial_board, board_copy, sizeof(initial_board));
        }
      } else {
        printf("Not check\n");
      }
    }

    printf("Start actual move\n");

    move_piece_on_board(move);

    display_chess_board(player_one, player_two);

    sleep(1);

    send(player_one, "m-2", 3, 0);

    //-------------END Player 1------------------
    //-------------START Player 2------------------

    printf("Player two moves...\n");

    valid_move = 0;
    valid_syntax = 0;
    remaining_moves = 2;

    while(!valid_move || !valid_syntax) {
      bzero(buffer, buffer_size);

      printf("While start for player 2...\n");
      int is_check_for_player = is_check(2);

      if (is_check_for_player) {
        printf("Player two check\n");
        sprintf(message, "c-%d", remaining_moves);
        send(player_two, message, 3, 0);
      } else {
        send(player_two, "m-1", 3, 0);
      }

      printf("Check finished\n");

      if (read(player_two, buffer, 7) < 0) {
        perror(SOCKET_READ_ERROR);
        exit(1);
      }

      printf("Player two made the following move: %s\n", buffer);

      printf("Validating syntax\n");
      valid_syntax = is_valid_syntax(buffer);

      if (!valid_syntax) {
        send(player_two, INVALID_SYNTAX, 4, 0);
        printf("Invalid syntax\n");
        continue;
      }

      move = convert_buffer_to_move(buffer);
      printf("Converted move for player two: %s\n", move);

      printf("Validating move\n");
      valid_move = is_move_valid(2, get_selected_piece(move), move);
      
      if (!valid_move) {
        send(player_two, INVALID_MOVE, 4, 0);
        printf("Invalid move\n");
        continue;
      } else {
        printf("Valid move\n");
      }

      if (is_check_for_player) {
        printf("Board copy...\n");
        // copiem starea curenta a tablei
        char* board_copy[9][9];
        int i, j;
        memcpy(board_copy, initial_board, sizeof(board_copy));
        // simulam miscarea pe tabla
        move_piece_on_board(move);
        printf("Move simulation done\n");
        // verificam din nou daca este sah
        if (is_check(2)) {
          remaining_moves--;
          if (!remaining_moves) {
            send(player_two, "game over", 9, 0);
            printf("---------Player one wins!-----------\n");
          } else {
            valid_move = 0;
            memcpy(initial_board, board_copy, sizeof(initial_board));
          }
        } else {
          printf("Valid move\n");
          valid_move = 1;
          memcpy(initial_board, board_copy, sizeof(initial_board));
        }
      } else {
        printf("Not check\n");
      }
    }
    printf("Start actual move\n");

    move_piece_on_board(move);

    display_chess_board(player_one, player_two);

    sleep(1);

    //-------------END Player 2------------------
  }
     
  return 0;
};

int main () {
  struct sockaddr_in server;	// structura folosita de server
  struct sockaddr_in from;
  int nr, sd , client_socket , c , *new_sock, pid, client_length;
  pthread_t th[100];    //Identificatorii thread-urilor care se vor crea

  pthread_cond_init(&player_to_join, NULL);
  pthread_mutex_init(&global_mutex_lock, NULL);

  /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  
  server.sin_family = AF_INET;	
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1) {
    perror ("[server]Eroare la bind().\n");
    return errno;
  }
  
  printf("Server started\n");

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, MAX_CLIENTS) == -1) {
    perror ("[server]Eroare la listen().\n");
    return errno;
  }

  printf("Waiting for incoming connections on port %d...", PORT);

  while(1) {
     client_length = sizeof(from);

     /* Accept actual connection from the client */
     client_socket = accept(sd, (struct sockaddr *)&from, (unsigned int *)&client_length);
     printf("– Connection accepted from %d –\n", client_socket);

     if (client_socket < 0) {
        perror("ERROR on accept");
        exit(1);
     }

     pthread_mutex_lock(&global_mutex_lock); 
     // Create thread if we have no user waiting
     if (player_in_lobby == 0) {
       printf("Connected player, creating new game...\n");
       pthread_create(&th[0], NULL, &treat, &client_socket);
       pthread_mutex_unlock(&global_mutex_lock); 
     }
     // If we've a user waiting join that room
     else {
       // Send user two signal
       printf("Connected player, joining game room... %d\n", client_socket);
       challenging_player = client_socket;
       pthread_mutex_unlock(&global_mutex_lock); 
       pthread_cond_signal(&player_to_join);
     }
   }
  
  if (client_socket < 0)
  {
    perror("accept failed");
    return 1;
  }
  close(sd);
  
  return 0;  
};

