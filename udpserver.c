/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

typedef struct content *content_t;
struct content {
  char *file;
  char *host;
  char *port;
  char *brate;
};

/* Global - holds the files and paths */
content_t dictionary[BUFSIZE];
/* Global - next free index in dictionary */
int d_index;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int peer_add(int connfd, char *request, char *version)
{
  char buf[BUFSIZE], *file, *host, *port, *rate;
  int n;

  strtok(request, "path=");
  file = strtok(NULL, "&");
  strtok(NULL, "host=");
  host = strtok(NULL, "&");
  strtok(NULL, "port=");
  port = strtok(NULL, "&");
  strtok(NULL, "rate=");
  rate = strtok(NULL, "&");
  
  dictionary[d_index]->file = file;
  dictionary[d_index]->host = host;
  dictionary[d_index]->port = port;
  dictionary[d_index]->brate = rate;
  d_index++; 

  sprintf(buf, "%s 200 OK \r\n file: %s\n host: %s\n port: %s\n brate: %s\n", version, file, host, port, rate); 
  if (n = write(connfd, buf, strlen(buf)) < 0)
    return -1;
  return 0;
}

int build_tcp_headers(content_t *file_array, int n)
{
  int i, chunks = n;
  for (i=0; i<n; i++)
  {
    
  }
  return 0;
} 

int peer_view(int connfd, char *request, char *version)
{
  char buf[BUFSIZE], *file;
  int i, n, found=0, loc_index=0, result;
  content_t locations[BUFSIZE]; /* holds all of the content structs of the requested uri */

  strtok(request, "view/");
  file = strtok(NULL, " ");
  /* search dictionary for file */
  for (i=0; i<index; i++)
  {
    if (strcmp(file, dictionary[i]->file) == 0) {/* file found */
      found = 1;
      locations[loc_index] = dictionary[i];
      loc_index++;
    }
  }    
  if (!found) { 
    file_not_found();
    return -1;
  }
  result = build_tcp_headers(locations, loc_index);
  return result;
}

int peer_config(int connfd, char *request, char *version)
{
  char buf[BUFSIZE], *rate;
  int n;

  strtok(request, "rate=");
  rate = strtok(NULL, " ");
}

/*
 * returns 0 on success, -1 on failure
 */
int get_request(int connfd, char *request)
{
  char *uri, *version;
  int result;
  uri = malloc(BUFSIZE);
  version = malloc(BUFSIZE);

  strtok(request, " ");
  *uri = strtok(NULL, " ");
  *version = strtok(NULL, " ");

  if (strstr(uri, "add")){
    result = peer_add(connfd, uri, version);
  } 
  else if (strstr(uri, "view")) {
    result = peer_view(connfd, uri, version);
  }
  else if (strstr(uri, "config")) {
    result = peer_config(connfd, uri, version);
  }
  else if (strstr(uri, "status")) {
    result = peer_status(uri);
  }
  else { /*not a valid get request*/
    result -1;
  }
  free(uri);
  free(version);
  return result;
} 

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    /* 
     * sendto: echo the input back to the client 
     */
    n = sendto(sockfd, buf, strlen(buf), 0, 
	       (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }
}
