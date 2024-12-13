#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <errno.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <strings.h>
#include <sys/wait.h>
#include <sys/signal.h>


#define QUIT "quit"
#define SERVER_PORT 10000
#define CONNECTION_REQUEST_LIMIT 5
#define BUFLEN 100
#define MAX_FILENAME_SIZE 20
#define MAX_CONTENT 200

typedef struct {
  char type;
  char data[BUFLEN];
}
PDU;

typedef struct {
  char type;
  char data[BUFLEN];
  int size;
}
SizePDU;

PDU rpdu;

struct {
  int val;
  char name[MAX_FILENAME_SIZE];
}
table[MAX_CONTENT];

char usr[MAX_FILENAME_SIZE];

int s_sock, peer_port;
int fd, nfds;
fd_set rfds, afds;

int client_download(char * , PDU * );
int server_download(int);
void local_list();
void quit(int);
void handler();
void reaper(int);
int indexs = 0;
char peerName[10];
int pid;

int main(int argc, char ** argv) {
  int s_port = SERVER_PORT;
  int n;
  int alen = sizeof(struct sockaddr_in);

  struct hostent * hp;
  struct sockaddr_in server;
  char c, * host, name[MAX_FILENAME_SIZE];
  struct sigaction sa;
  char dataToSend[100];
  switch (argc) {
  case 2:
    host = argv[1];
    break;
  case 3:
    host = argv[1];
    s_port = atoi(argv[2]);
    break;
  default:
    printf("Usage: %s host [port]\n", argv[0]);
    exit(1);
  }

  // TCP
  int sd, new_sd, client_len, port, m;
  struct sockaddr_in client, client2;
  // Create a stream socket	
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Can't create a socket\n");
    exit(1);
  }

  char myIP[16];
  bzero((char * ) & client, sizeof(struct sockaddr_in));
  socklen_t len = sizeof(client);
  if (bind(sd, (struct sockaddr * ) & client, sizeof(client)) == -1) {
    fprintf(stderr, "Can't bind name to socket\n");
    exit(1);
  }

  listen(sd, CONNECTION_REQUEST_LIMIT);

  (void) signal(SIGCHLD, reaper);

  // UDP 
  memset( & server, 0, alen);
  server.sin_family = AF_INET;
  server.sin_port = htons(s_port);

  if (hp = gethostbyname(host))
    memcpy( & server.sin_addr, hp -> h_addr, hp -> h_length);
  else if ((server.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
    printf("Can't get host entry\n");
    exit(1);
  }
  s_sock = socket(AF_INET, SOCK_DGRAM, 0); 

  if (s_sock < 0) {
    printf("Can't create socket\n");
    exit(1);
  }
  if (connect(s_sock, (struct sockaddr * ) & server, sizeof(server)) < 0) {
    printf("Can't connect\n");
    exit(1);
  }
  // Retrieve the Port that the Client has opened
  getsockname(sd, (struct sockaddr * ) & client, & len);
  inet_ntop(AF_INET, & client.sin_addr, myIP, sizeof(myIP));
  int myPort;
  myPort = ntohs(client.sin_port);
  char * ip = inet_ntoa(((struct sockaddr_in * ) & client) -> sin_addr);
  char address[45];
  char cport[25];
  sprintf(address, "%s", myIP);
  sprintf(cport, "%u", myPort);

  // Choose Username
  printf("Choose a username\n");
  n = read(0, peerName, 10);

  FD_ZERO( & afds);
  FD_SET(s_sock, & afds);
  FD_SET(0, & afds);
  FD_SET(sd, & afds);
  nfds = 1;
  for (n = 0; n < MAX_CONTENT; n++)
    table[n].val = -1;

  // Signal Handler Setup
  sa.sa_handler = handler;
  sigemptyset( & sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, & sa, NULL);
  switch ((pid = fork())) {
  case 0: 
    while (1) {
      client_len = sizeof(client2);
      new_sd = accept(sd, (struct sockaddr * ) & client2, & client_len);
      if (new_sd < 0) {
        fprintf(stderr, "Can't accept client\n");
        exit(1);
      }
      switch (fork()) {
      case 0: 
        (void) close(sd);
        exit(server_download(new_sd));
      default:
        (void) close(new_sd);
        break;
      case -1:
        fprintf(stderr, "Error Creating New Process\n");
      }
    }
    default:
      // User Command Loop
      while (1) { 
        printf("Command:\n");
        memcpy( & rfds, & afds, sizeof(rfds));
        if (select(nfds, & rfds, NULL, NULL, NULL) == -1) {
          printf("Select error: %s\n", strerror(errno));
          exit(1);
        }

        if (FD_ISSET(0, & rfds)) {
          char command[40];
          n = read(0, command, 40);

          // Help Command '?'
          if (command[0] == '?') {
            printf("R-Content Registration; T-Content Deregistration; L-List Local Content\n");
            printf("D-Download Content; O-List all Online Content; Q-Quit\n\n");
            continue;
          }

          // Register Content 'R'
          if (command[0] == 'R') {
            // Set PDU type to 'R' indicating a registration request
            rpdu.type = 'R';	
            printf("Content Name:\n");
            char contentName[20];
            n = read(0, contentName, 20);
            peerName[strcspn(peerName, "\n")] = 0;
            contentName[strcspn(contentName, "\n")] = 0;
            if (fopen(contentName, "r") == NULL){
               printf("File Does Not Exist!");
               continue;
            }
            strcpy(dataToSend, peerName);
            strcat(dataToSend, ",");
            strcat(dataToSend, contentName);
            strcat(dataToSend, ",");
            strcat(dataToSend, cport);
            strcpy(rpdu.data, dataToSend);
            // Send PDU to Server
            write(s_sock, & rpdu, sizeof(rpdu));
            PDU r2pdu;
            while (1) {
              // Read Response PDU from Server
              n = read(s_sock, & r2pdu, sizeof(r2pdu));
              printf("%d\n", n);
              if (r2pdu.type == 'E') {
                printf("%s\n", r2pdu.data);
                break;
              }
              if (r2pdu.type == 'A') {
                printf("%s\n", r2pdu.data);
                table[indexs].val = 1;
                strcpy(table[indexs].name, contentName);
                indexs++;
                break;
              }
            }
          }
          // List Content 'L'
          if (command[0] == 'L') {
            local_list();
          }

          // List Online Content 'O'
          if (command[0] == 'O') {
            rpdu.type = 'O';
            write(s_sock, & rpdu, sizeof(rpdu));
            PDU r2pdu;
            while (1) {
              n = read(s_sock, & r2pdu, sizeof(r2pdu));
              if (r2pdu.type == 'E') {
                printf("%s\n", r2pdu.data);
                break;
              }
              if (r2pdu.type == 'O') {
                printf("%s\n", r2pdu.data);
              }
              if (r2pdu.type == 'A') {
                break;
              }

            }
          }

          // Download Content 'D'
          if (command[0] == 'D') {
            // Set PDU type to 'S' indicating a search request
            rpdu.type = 'S';
            printf("Content Name:\n");
            char contentName[20];
            n = read(0, contentName, 20);
            peerName[strcspn(peerName, "\n")] = 0;

            strcpy(dataToSend, peerName);
            strcat(dataToSend, ",");
            contentName[strcspn(contentName, "\n")] = 0;
            strcat(dataToSend, contentName);
            strcpy(rpdu.data, dataToSend);
            // Send PDU to Server
            write(s_sock, & rpdu, sizeof(rpdu));
            PDU r2pdu;
            while (1) {
              n = read(s_sock, & r2pdu, sizeof(r2pdu));
              if (r2pdu.type == 'E') {
                printf("%s\n", r2pdu.data);
                break;
              }
              if (r2pdu.type == 'S') {
                printf("%s\n", r2pdu.data);
                client_download(contentName, & r2pdu);
                
		            // Register Download Content 'R'
                rpdu.type = 'R';
                peerName[strcspn(peerName, "\n")] = 0;
                contentName[strcspn(contentName, "\n")] = 0;
                strcpy(dataToSend, peerName);
                strcat(dataToSend, ",");
                strcat(dataToSend, contentName);
                strcat(dataToSend, ",");
                strcat(dataToSend, cport);

                table[indexs].val = 1;
                strcpy(table[indexs].name, contentName);
                indexs++;

                strcpy(rpdu.data, dataToSend);
                write(s_sock, & rpdu, sizeof(rpdu));
                PDU r2pdu;
                while (1) {
                  n = read(s_sock, & r2pdu, sizeof(r2pdu));
                  if (r2pdu.type == 'E') {
                    printf("%s\n", r2pdu.data);
                    break;
                  }
                  if (r2pdu.type == 'A') {
                    printf("%s\n", r2pdu.data);
                    break;
                  }
                }
                break;
              }
            }
          }

          // Deregister Content 'T'
          if (command[0] == 'T') {
            // Set PDU type to 'T' indicating a deregistration request
            rpdu.type = 'T';
            char contentName[20];
            local_list();	
            printf("Enter Number In List:\n");
            int theValueInList;
            while (scanf("%d", & theValueInList) != 1) {
              printf("Please enter the index to deregister\n");
            }
            printf("Index chosen = %d\n", theValueInList);
            if (theValueInList >= indexs) {
              continue;
            }
            strcpy(contentName, table[theValueInList].name);

            peerName[strcspn(peerName, "\n")] = 0;

            strcpy(dataToSend, peerName);
            strcat(dataToSend, ",");
            contentName[strcspn(contentName, "\n")] = 0;
            strcat(dataToSend, contentName);
            strcpy(rpdu.data, dataToSend);
            write(s_sock, & rpdu, sizeof(rpdu));
            PDU r2pdu;
            while (1) {
              n = read(s_sock, & r2pdu, sizeof(r2pdu));
              printf("%d\n", n);
              if (r2pdu.type == 'E') {
                printf("%s\n", r2pdu.data);
                break;
              }
              if (r2pdu.type == 'A') {
                printf("%s\n", r2pdu.data);
                table[theValueInList].val = 0;
                break;
              }
            }
          }

          // Quit 'Q'
          if (command[0] == 'Q') {
            quit(s_sock);
            exit(0);
          }
        }
      }
  }
}

void quit(int s_sock) {
  // Deregister all registrations in the index server
  rpdu.type = 'T';
  int a = 0;
  char dataToSend[100];
  int n;
  for (a = 0; a < indexs; a++) {
    if (table[a].val == 1) {
      char contentName[20];
      strcpy(contentName, table[a].name);
      peerName[strcspn(peerName, "\n")] = 0;

      strcpy(dataToSend, peerName);
      strcat(dataToSend, ",");
      contentName[strcspn(contentName, "\n")] = 0;
      strcat(dataToSend, contentName);
      strcpy(rpdu.data, dataToSend);
      write(s_sock, & rpdu, sizeof(rpdu));
      PDU r2pdu;

      while (1) {
        n = read(s_sock, & r2pdu, sizeof(r2pdu));
        if (r2pdu.type == 'E') {
          printf("%s\n", r2pdu.data);
          break;			
        }
        if (r2pdu.type == 'A') {
          printf("%s\n", r2pdu.data);
          break;
        }
      }
    }
  }
  kill(pid, SIGKILL); //Kill all processes
}

/* List local content	*/
void local_list() {
  int i = 0;
  for (i = 0; i < indexs; i++) {
    if (table[i].val == 1) {
      printf("[%d]: %s\n", i, table[i].name);
    }
  }
}

/* Respond to download request from a peer */
int server_download(int sd) {
  char * bp, buf[BUFLEN], rbuf[BUFLEN], sbuf[BUFLEN];
  int n, bytes_to_read, m;
  FILE * pFile;
  SizePDU spdu;

  n = read(sd, buf, BUFLEN);

  pFile = fopen(buf, "r");
  if (pFile == NULL) {
    printf("Error: File not found\n");
    spdu.type = 'E';
    strcpy(spdu.data, "Error: File not found\n");
    write(sd, & spdu, sizeof(spdu));
  } else {
    while ((m = fread(spdu.data, sizeof(char), 100, pFile)) > 0) {
      spdu.type = 'C';
      spdu.size = m;
      write(sd, & spdu, sizeof(spdu));
    }
  }
  close(pFile);

  close(sd);
  return (0);
}

/* Initiate download with content server */
int client_download(char * name, PDU * pdu) {
  const char s[2] = ",";
  char fileName[20];
  char ouput[100];
  char user[20];
  int sd, port, i, n;
  struct sockaddr_in server;
  struct hostent * hp;
  char host[20], portString[20], * bp, rbuf[BUFLEN], sbuf[BUFLEN];
  SizePDU rpdu;
  strcpy(user, strtok(pdu -> data, s));
  strcpy(fileName, strtok(NULL, s));
  strcpy(host, strtok(NULL, s));
  strcpy(portString, strtok(NULL, s));

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Socket Creation Failed\n");
    exit(1);
  }

  bzero((char * ) & server, sizeof(struct sockaddr_in));
  server.sin_family = AF_INET;
  server.sin_port = htons(atoi(portString));
  if (hp = gethostbyname(host))
    bcopy(hp -> h_addr, (char * ) & server.sin_addr, hp -> h_length);
  else if (inet_aton(host, (struct in_addr * ) & server.sin_addr)) {
    fprintf(stderr, "Acquiring Server Address Failed\n");
    exit(1);
  }
  if (connect(sd, (struct sockaddr * ) & server, sizeof(server)) == -1) {
    fprintf(stderr, "Can't connect to a server \n");
    exit(1);
  }

  printf("Transmitting\n");

  strcpy(sbuf, fileName);
  write(sd, sbuf, 100);

  bp = rbuf;
  FILE * fp = fopen(fileName, "w");
  i = read(sd, & rpdu, sizeof(rpdu));
  while (i > 0) {
    // Data Received 'C'
    if (rpdu.type == 'C') {
      fwrite(rpdu.data, sizeof(char), rpdu.size, fp);
      fflush(fp);
      i = read(sd, & rpdu, sizeof(rpdu));
    } else if (rpdu.type == 'E') {
      printf("%s\n", rpdu.data);
      break;
    }
  }
  // Close the file connection
  fclose(fp);
  // Close the socket connection
  close(sd);
  return 0;
}

void handler() {
  quit(s_sock);
}

void reaper(int sig) {
  int status;
  while (wait3( & status, WNOHANG, (struct rusage * ) 0) >= 0);
}
