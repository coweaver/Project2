/* 
 * udpserver.c - A front-end HTTP server with CCP backend. 
 * usage: udpserver <port> <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "udpserver.h"

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
  printf("Starting peer_add: %s\n", request); 
  char *temp, key[100], val[100], buf[BUFSIZE], file[100], host[100], port[100], rate[100];
  int n;
 
  bzero(key, 100);
  bzero(val, 100);

  strtok(request, "?");
  temp = strtok(NULL, "&");
  while(temp){
    sscanf(temp, "%[^=]=%s", key, val);
    if(strcmp(key, "") == 0){
      printf("Invalid GET request\n");
      return -1;
    }      
    if(strcmp(key, "path")==0)
      snprintf(file, sizeof(val), "%s", val); 
    if(strcmp(key, "host")==0)
      snprintf(host, sizeof(val), "%s", val); 
    if(strcmp(key, "port")==0)
      snprintf(port, sizeof(val), "%s", val); 
    if(strcmp(key, "rate")==0)
      snprintf(rate, sizeof(val), "%s", val); 
    
    temp = strtok(NULL, "&");
  }    
    
  if(strcmp(file,  "")==0){
    printf("No file path found\n");
    return -1;
  }else if(strcmp(host, "")==0){
    printf("No host found\n");
    return -1;
  }else if(strcmp(port,"")==0){
    printf("No port found\n");
    return -1;
  }

  dictionary[d_index] = (content_t)malloc(sizeof(struct content));

  dictionary[d_index]->file = file;
  dictionary[d_index]->host = host;
  dictionary[d_index]->port = port;
  dictionary[d_index]->brate = rate;
  d_index++; 

  sprintf(buf, "%s 200 OK \r\n file: %s\n host: %s\n port: %s\n brate: %s\n", version, file, host, port, rate); 

  if ((n = write(connfd, buf, strlen(buf))) < 0)
    error("ERROR on write");
    

  printf("\n\nfile: %s is located at %s:%s\n\n", dictionary[d_index-1]->file, dictionary[d_index-1]->host, dictionary[d_index-1]->port); 
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

  int result = send_CCP_request(connfd, file, locations, loc_index);
  return result;
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
  printf("get_request: %s", request);
  char *uri, *version;
  int result;

  strtok(request, " ");
  uri = strtok(NULL, " ");
  version = strtok(NULL, "\r\n");

  printf("%s, %s\n", uri,  version);

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
  return result;
} 


int send_CCP_request(int connfd, char *file, content_t locations[], int loc_index){
  return 0;
}



int send_CCP_accept(active_flow *flow, int portno){
  char buf[BUFSIZE];
  bzero(buf, BUFSIZE);
  
  

}





// Fills header values and puts data in buf, returns -1 on failure
int CCP_parse_header(char buf[], uint16_t *source, uint16_t *dest, uint16_t *seq_n, 
		     uint16_t *ack_n, uint16_t *len, uint16_t *win_size, uint16_t *ack, uint16_t *syn,
		     uint16_t *fin, uint16_t *chk_sum){

  if(buf[strlen(buf)-1] == '\n'){
    buf[strlen(buf)-1] = 0;
  }

  if(strlen(buf) < 16){
    printf("Invalid CCP_header\n");
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
    printf("Invalid CCP header\n");
    return -1;
  }
  strcpy(buf, buf+16);
return 0;
}



int handle_CCP_packet(char *buf, struct sockaddr_in clientaddr, int portno){

  uint16_t source, dest, seq_n, ack_n, len, win_size, ack, syn, fin, chk_sum;

  CCP_parse_header(buf, &source, &dest, &seq_n, &ack_n, &len, &win_size, &ack, &syn, &fin, &chk_sum);
  if(!ack){
    if(!syn)
      printf("Invalid CCP Header");
    else{// new flow
      if(strlen(buf) == 0) printf("Empty CCP request");
      else{
	active_flow *new = (active_flow *)malloc(sizeof(active_flow));
	new->partneraddr = clientaddr;
	new->destport = dest;
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
	
	send_CCP_accept(new);
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
  if (HTTP_sockfd < 0)
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

  /* main loop: wait for an active FD, then appropriately respond
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    read_fds = active_fds;
    if( select( FD_SETSIZE, &read_fds, NULL, NULL, NULL) <0 )
      error("ERROR on select");

    for(int act_fd = 0; act_fd < FD_SETSIZE; ++act_fd){

      if( FD_ISSET(act_fd, &read_fds) ){
	
	if( act_fd == HTTP_sockfd ){ //New connection requested
	  printf("Creating New Connection...\n");

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

	  printf("Connection created.\n");

	}
        else if ( act_fd == CCP_sockfd ){ //Backend (CCP) Packet received
	  bzero(buf, BUFSIZE);

	  //RTT TIMING SHOULD BE IMPLEMENTED HERE
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

	  handle_CCP_packet(buf, clientaddr, CCP_portno);
	
	}else{ // act_fd is a client (i.e. GET request)

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
