#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

#define BUFLEN 100
#define MAX_FILENAME_SIZE 20
#define MAX_CONTENT 200

typedef struct entry {
  char user[MAX_FILENAME_SIZE];
  char ip[16];
  char port[7];
  short token;
  struct entry * next;
}
ENTRY;

typedef struct {
  char name[MAX_FILENAME_SIZE];
  ENTRY * head;
}
LIST;
LIST list[MAX_CONTENT];
int max_index = 0;

// PDU for UDP communication
typedef struct {
  char type;
  char data[BUFLEN];
}
PDU;
PDU tpdu;

void search(int, char * , struct sockaddr_in * );
void registration(int, char * , struct sockaddr_in * );
void deregistration(int, char * , struct sockaddr_in * );

struct sockaddr_in fsin;

// UDP Content Indexing Service
int main(int argc, char * argv[]) {
  struct sockaddr_in sin, * p_addr;
  ENTRY * p_entry;
  char * service = "10000";
  char name[MAX_FILENAME_SIZE], user[MAX_FILENAME_SIZE];
  int alen = sizeof(struct sockaddr_in);
  int s, n, i, len, p_sock;
  int pdulen = sizeof(PDU);
  struct hostent * hp;
  PDU rpdu;
  int j = 0;

  PDU spdu;

  for (n = 0; n < MAX_CONTENT; n++)
    list[n].head = NULL;
  switch (argc) {
  case 1:
    break;
  case 2:
    service = argv[1];
    break;
  default:
    fprintf(stderr, "Incorrect Arguments \n Use format: server [host] [port]\n");
  }

  memset( & sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons((u_short) atoi(service));

  // Allocate socket 
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    fprintf(stderr, "can't creat socket\n");
    exit(1);
  }

  // Bind socket 
  if (bind(s, (struct sockaddr * ) & sin, sizeof(sin)) < 0)
    fprintf(stderr, "can't bind to %s port\n", service);

  while (1) {
    if ((n = recvfrom(s, & rpdu, pdulen, 0, (struct sockaddr * ) & fsin, & alen)) < 0) {
      printf("recvfrom error: n=%d\n", n);
    }

    // Content Registration Request 'R'			
    if (rpdu.type == 'R') {
      printf("Registering\n");
      registration(s, rpdu.data, & fsin);
      printf("%d\n", s);
    }

    // Search Content Request 'S'		
    if (rpdu.type == 'S') {
      printf("Searching\n");
      search(s, rpdu.data, & fsin);
    }

    // List Content Request 'O' 
    if (rpdu.type == 'O') {
      printf("Listing Content\n");
      for (j = 0; j < max_index; j++) {
        if (list[j].head != NULL) {
          PDU spdu;
          spdu.type = 'O';
          strcpy(spdu.data, list[j].name);
          printf("%s\n", list[j].name);
          (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
        }
      }
      // Acknowledgment Response
      PDU opdu;
      opdu.type = 'A';
      (void) sendto(s, & opdu, sizeof(opdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
    }

    // Deregister Content Request 'T'		
    if (rpdu.type == 'T') {
      printf("de-register\n");
      deregistration(s, rpdu.data, & fsin);
    }
  }
  return;
}

/* 
  Searches for a file in the content list and responds with the file information or an error.
   
  @param s       The socket address
  @param *data   A pointer to the received data, containing the username and filename
  @param *addr   A pointer to the address of the client sending/requesting 
*/
void search(int s, char * data, struct sockaddr_in * addr) {
  int j;
  int found = 0;
  int used = 999;
  ENTRY * use;
  ENTRY * head;
  int pdulen = sizeof(PDU);
  PDU spdu;
  char user[20];
  char output[100];
  char fileName[20];
  char rep[2] = ",";

  // Split data into username and filename
  strcpy(user, strtok(data, rep));
  strcpy(fileName, strtok(NULL, rep));

  for (j = 0; j < max_index; j++) {
    printf("%s\n", list[j].name);
    if (strcmp(list[j].name, fileName) == 0 && (list[j].head != NULL)) {
      found = 1;
      head = list[j].head;
      while (head != NULL) {
        if (head -> token < used) {
          used = head -> token;
          use = head;
        }
        head = head -> next;
      }
      // Exit if content is found
      break; 
    }
  }
  if (found == 1) {
    spdu.type = 'S'; 
    strcpy(output, use -> user);
    strcat(output, ",");
    strcat(output, fileName);
    strcat(output, ",");
    strcat(output, use -> ip);
    strcat(output, ",");
    strcat(output, use -> port);
    printf("%s\n", output);
    strcpy(spdu.data, output);
    use -> token++;
    // Send to client
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  } else {
    spdu.type = 'E';
    strcpy(spdu.data, "File not found\n");
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  }
  printf("Ending\n");
}

/*
  Registers a new content entry to the server.

  @param s       The socket address
  @param *data   A pointer to the received data, containing the username and filename
  @param *addr   A pointer to the address of the client sending/requesting 
*/
void registration(int s, char * data, struct sockaddr_in * addr) {
  ENTRY * new = NULL;
  new = (ENTRY * ) malloc(sizeof(ENTRY));
  int j;
  ENTRY * head;
  int used = 999;
  int duplicateUser = 0;
  char rep[2] = ",";
  PDU spdu;
  char fileName[20];
  int found = 0;
  char * ip = inet_ntoa(fsin.sin_addr);

  printf("Sending IP address %s\n", ip);
  printf("Socket %d\n", s);
  printf("Data %s\n", data);
  strcpy(new -> user, strtok(data, rep));
  strcpy(fileName, strtok(NULL, rep));
  strcpy(new -> port, strtok(NULL, rep));
  strcpy(new -> ip, ip);

  printf("Stored IP address %s\n", new -> ip);
  new -> token = 0;
  new -> next = NULL;
  for (j = 0; j < max_index; j++) {
    if (strcmp(list[j].name, fileName) == 0) {
      head = list[j].head;
      found = 1;
      while (head != NULL) {
        if (head -> next == NULL) {
          break;
        }
        // Check if username exists
        if (strcmp(head -> user, data) == 0) {
          duplicateUser = 1;
          break;
        }
        head = head -> next;
      }
      if (head == NULL) {
        list[j].head = new;
      } else {
        head -> next = new;
      }
      break;
    }
  }
  // File not found
  if (found == 0) {
    strcpy(list[max_index].name, fileName);
    list[max_index].head = new;
    max_index++;
  }
  // Handle duplicate user registration
  if (duplicateUser == 1) {
    printf("Username already exists\n");
    spdu.type = 'E';
    strcpy(spdu.data, "Duplicate username"); 
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));

  } else {
    printf("Username is unique\n");
    spdu.type = 'A';
    strcpy(spdu.data, "Done");
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  }
}

/*
  Deregisters a file from the server by removing the specified user's entry.

  @param s       The socket address
  @param *data   A pointer to the received data, containing the username and filename
  @param *addr   A pointer to the address of the client sending/requesting 
*/
void deregistration(int s, char * data, struct sockaddr_in * addr) {
  int j;
  int use = -1;
  ENTRY * prev;
  ENTRY * head;
  int listIndex = 0;
  PDU spdu;
  char rep[2] = ",";
  char user[20];
  char fileName[20];

  printf("Deregistering %s\n", data);
  // Split data into username and filename
  strcpy(user, strtok(data, rep));
  strcpy(fileName, strtok(NULL, rep));
  for (j = 0; j < max_index; j++) {
    if (strcmp(list[j].name, fileName) == 0) {
      head = list[j].head;
      prev = list[j].head;
      listIndex = 0;

      while (head != NULL) {
        printf("User list = %s\n", head -> user);
        printf("User given = %s\n", user);

        if (strcmp(head -> user, user) == 0) {
          printf("Name Comparison Success\n");
          printf("List index = %d\n", listIndex);
          use = listIndex;

          if (listIndex == 0) {
            list[j].head = head -> next;
          } else {
            prev -> next = head -> next;
          }
          // Exit if entry is found
          break;
        }
        listIndex++;
        prev = head;
        head = head -> next;
      }
      // Exit if file is found
      break;
    }
  }
  if (use != -1) {
    spdu.type = 'A';
    strcpy(spdu.data, "Done");
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  } else {
    spdu.type = 'E';
    strcpy(spdu.data, "Failed");
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  }
}