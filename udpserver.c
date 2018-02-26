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

typedef struct active_flow {
  char *hostaddrp;
  uint16_t my_seq_n;
  uint16_t your_seq_n;
  struct timeval last_clock;
  int RTTEstimate; 
  FILE *file_fd;
  int client_fd;
} active_flow;


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


int file_not_found(){
  return 0;
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

  if ((n = write(connfd, buf, strlen(buf))) < 0)

    return -1;
  return 0;
}



int peer_view(int connfd, char *request, char *version)
{
  char *file;
  int i, found=0, loc_index=0;
  content_t locations[BUFSIZE]; /* holds all of the content structs of the requested uri */

  strtok(request, "view/");
  file = strtok(NULL, " ");
  /* search dictionary for file */

  for (i=0; i<BUFSIZE; i++)
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

  //  result = build_tcp_headers(locations, loc_index);
  return 0;
}

int peer_config(int connfd, char *request, char *version)
{
  char *rate;
  
  strtok(request, "rate=");
  rate = strtok(NULL, " ");
  return 0;
}


int peer_status(char *uri){
  return 0;
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
  uri = strtok(NULL, " ");
  version = strtok(NULL, " ");

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

    result =  -1;
  }
  free(uri);
  free(version);
  return result;
} 


int TCP_write_header(char buf[BUFSIZE], char *source, char *dest, char *seq_n, char *ack_n, 		     char *win_size, char *chk_sum, char *ack, char *syn, char *fin){
  sprintf(buf, "%s%s%s%s%s", source, dest, seq_n, ack_n, win_size);
  unsigned char flags = 0;
  if(*ack){
    flags = flags | 0x80;
  }if(*syn){
    flags = flags | 0x40;
  }if(*fin){
    flags = flags | 0x20;
  }  
  unsigned char reserved = (unsigned char)0x20;
  sprintf(buf, "%s%c%c%s%c%c", buf,  flags, reserved, chk_sum, reserved, reserved); 
  return 0;
}



// Fills header values and puts data in buf, returns -1 on failure
int TCP_parse_header(char buf[], uint16_t *source, uint16_t *dest, uint16_t *seq_n, 
		     uint16_t *ack_n, uint16_t *len, uint16_t *win_size, uint16_t *ack, uint16_t *syn,
		     uint16_t *fin, uint16_t *chk_sum){

  if(buf[strlen(buf)-1] == '\n'){
    buf[strlen(buf)-1] = 0;
  }

  if(strlen(buf) < 16){
    printf("Invalid TCP_header\n");
    return -1;
  }
 
 
  memcpy(source, buf, 2);
  memcpy(dest, buf+2, 2);
  memcpy(seq_n, buf+4, 2);
  memcpy(ack_n, buf+6, 2);
  memcpy(len, buf+8, 2);  
  memcpy(win_size, buf+10, 2);
  memcpy(chk_sum, buf+14, 2);
  

  //read flags
  if(buf[12] == (char)0xC0){ // 0xC0 = 1100 0000
    *ack = 1;
    *syn = 1;
    *fin = 0;
  }else if(buf[12] == (char)0xA0){ // 0xA0 = 1010 0000
    *ack = 1;
    *syn = 0;
    *fin = 0;
  }else if(buf[12] == (char)0x80){ // 0x80 = 1000 0000
    *ack = 1;
    *syn = 0;
    *fin = 0;
  }else if(buf[12] == (char)0x40){ // 0x40 = 0100 0000 
    *ack = 0;
    *syn = 1;
    *fin = 0;
  }else{
    printf("Invalid TCP header\n");
    return -1;
  }
  strcpy(buf, buf+16);
return 0;
}


int send_synack(active_flow *flow){
  return 0;
}

int handle_CCP_packet(char *buf, char *hostaddrp, active_flow *flows[], int num_flows){

  uint16_t source, dest, seq_n, ack_n, len, win_size, ack, syn, fin, chk_sum;

  TCP_parse_header(buf, &source, &dest, &seq_n, &ack_n, &len, &win_size, &ack, &syn, &fin, &chk_sum);
  if(!ack){
    if(!syn)
      printf("Invalid CCP Header");
    else{// new flow
      if(strlen(buf) == 0) printf("Empty CCP request");
      else{
	active_flow *new = (active_flow *)malloc(sizeof(active_flow));
	new->hostaddrp = hostaddrp;
	new->your_seq_n = seq_n;
	new->my_seq_n = rand() % 65536; // 2^16 (maximum sequence number)
	new->last_clock.tv_usec = 0;
	new->RTTEstimate = 0;
	new->client_fd = 0;
        new->file_fd = fopen(buf, "r");
	if( new->file_fd == NULL) // could not locate file
	  error("CCP requested a file that does not exist");
	flows[num_flows] = new;
	num_flows += 1;
	
	send_synack(new);
      }
    }  
  }else{//Existing Flow

    
  }
  
  return num_flows;
}




int main(int argc, char **argv) {
  int HTTP_sockfd, CCP_sockfd; /* socket */
  int HTTP_portno, CCP_portno; /* port to listen on */
  unsigned int clientlen; /* byte size of client's address */
  struct sockaddr_in HTTP_serveraddr, CCP_serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 3) {
    fprintf(stderr, "usage: %s <port> <port>\n", argv[0]);
    exit(1);
  }
  HTTP_portno = atoi(argv[1]);
  CCP_portno = atoi(argv[2]);

  /* 
   * socket: create the HTTP and CCP sockets 
   */
  CCP_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (CCP_sockfd < 0) 
    error("ERROR opening CCP socket");

  HTTP_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (CCP_sockfd < 0)
    error("ERROR opening HTTP socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(CCP_sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));
  setsockopt(HTTP_sockfd, SOL_SOCKET, SO_REUSEADDR,
	     (const void *)&optval, sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &HTTP_serveraddr, sizeof(HTTP_serveraddr));
  HTTP_serveraddr.sin_family = AF_INET;
  HTTP_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  HTTP_serveraddr.sin_port = htons((unsigned short)HTTP_portno);

  bzero((char *) &CCP_serveraddr, sizeof(HTTP_serveraddr));
  CCP_serveraddr.sin_family = AF_INET;
  CCP_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  CCP_serveraddr.sin_port = htons((unsigned short)HTTP_portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(HTTP_sockfd, (struct sockaddr *) &HTTP_serveraddr, 
	   sizeof(HTTP_serveraddr)) < 0) 
    error("ERROR on binding");
  if (bind(CCP_sockfd, (struct sockaddr *) &CCP_serveraddr,
	   sizeof(CCP_serveraddr)) < 0)
    error("ERROR on binding");

  /* listen: make the HTTP socket a listening socket ready to accept connections*/
  if (listen(HTTP_sockfd, 5) < 0)
    error("ERROR on listen");
  
  /* initialize socket sets for select */
  fd_set read_fds, active_fds;
  FD_ZERO(&active_fds);
  FD_SET(HTTP_sockfd, &active_fds);
  FD_SET(CCP_sockfd, &active_fds);

  // active-flow list -- keeps track of active flows.

  active_flow *flows[10]; //maximum 10 simultaneous transfers.
  int num_flows = 0;
  /* 
   * main loop: wait for an active FD, then appropriately respond
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    read_fds = active_fds;
    if( select( FD_SETSIZE, &read_fds, NULL, NULL, NULL) <0 )
      error("ERROR on select");

    for(int act_fd = 0; act_fd < FD_SETSIZE; ++act_fd){
      if( FD_ISSET(act_fd, &read_fds) ){
	
	if( act_fd == HTTP_sockfd ){ //New connection requested
	
	  int connfd = accept(act_fd, (struct sockaddr *) &clientaddr, &clientlen);
	  if(connfd < 0)
	    error("ERROR on accept");
	  
	  hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	  if (hostp == NULL)
	    error("ERROR on gethostbyaddr");
	  hostaddrp = inet_ntoa(clientaddr.sin_addr);
	  if (hostaddrp == NULL)
	    error("ERROR on inet_ntoa\n");
	  printf("server received connection request from %s (%s)\n", 
		 hostp->h_name, hostaddrp);
	  
	  FD_SET(connfd, &active_fds);

	}else if ( act_fd == CCP_sockfd ){ //Backend (CCP) Packet received
	  bzero(buf, BUFSIZE);

	  //TIMING SHOULD BE IMPLEMENTED HERE
	  n = recvfrom(act_fd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
	  if (n < 0)
	    error("ERROR in recvfrom");
	  
	  hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	  if (hostp == NULL)
	    error("ERROR on gethostbyaddr");
	  hostaddrp = inet_ntoa(clientaddr.sin_addr);
	  if (hostaddrp == NULL)
	    error("ERROR on inet_ntoa\n");
	  printf("server received connection request from %s (%s)\n", 
		 hostp->h_name, hostaddrp);
	  printf("server received %d/%d bytes: %s\n", (int)strlen(buf), n, buf);

	  handle_CCP_packet(buf, hostaddrp, flows, num_flows);
	
	}else{ // act_fd is a client (i.e. GET request
	  bzero(buf, BUFSIZE);
	  n = read(act_fd, buf, BUFSIZE);
	  if ( n < 0)
	    error("ERROR reading from client");
	  
	  get_request(act_fd, buf);
	  
	  close(act_fd);
	  FD_CLR(act_fd, &active_fds);
	}
      }
    }
  }
}
