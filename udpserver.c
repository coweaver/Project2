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
#include <time.h>
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



int peer_view(int connfd, char *request, char *version, int CCP_sockfd)
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
  int result = send_CCP_request(connfd, locations[0], CCP_sockfd);
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
int get_request(int connfd, char *request, int CCP_sockfd)
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
    result = peer_view(connfd, uri, version, CCP_sockfd);
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

//Returns active flow struct on sucess, NULL on failure
active_flow *find_flow(uint8_t id){
  for(int i = 0; i < num_flows; i++){
    if( flows[i]->id == id ){
      return flows[i];
    }
  }
  return NULL;
}

int remove_flow(uint8_t id){
  int found=0;
  active_flow *flow;
  for(int i = 0; i < num_flows; i++){
    if( flows[i]->id == id ){
      found = 1;
      flow = flows[i];
    }else if(found){
      flows[i-1] = flows[i];
      flows[i] = NULL;
    }
  }
  if(!found){
    printf("not a valid flow id");
    return -1;
  }
  free(flow);
  num_flows -= 1;
  return 0;
}



int build_CCP_header(char *buf, active_flow *flow, uint16_t *len, uint16_t *win_size, 
		     uint8_t *flags, uint16_t *chk_sum){  
  memcpy(buf, &(flow->sourceport), 2);
  memcpy(buf+2, &(flow->destport), 2);
  memcpy(buf+4, &(flow->my_seq_n), 2);
  memcpy(buf+6, &(flow->your_seq_n), 2);
  if(*len > 0)
    memcpy(buf+8, len, 2);
  else{
    memcpy(buf+8, (char *)0xFF, 2); 
  }
  memcpy(buf+10, win_size, 2);
  memcpy(buf+12, flags, 1);
  memcpy(buf+13, &(flow->id), 1);
  memcpy(buf+14, chk_sum, 2);
  
  return 0;
}



int send_CCP_request(int connfd, content_t file, int CCP_sockfd){
  char buf[PACKETSIZE];
  int n;
  active_flow *af;
  struct sockaddr_in partneraddr;


  af = (active_flow *)malloc(sizeof(active_flow));
  
  memcpy(&(af->destport), file->port, 2);

  af->my_seq_n = 0;
  af->client_fd = connfd;

  bzero((char *) &partneraddr, sizeof(partneraddr));
  partneraddr.sin_family = AF_INET;
  partneraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  partneraddr.sin_port = htons((unsigned short)file->port);

  af->partneraddr = partneraddr;

  uint16_t len, win_size,  chk_sum;
  uint8_t flags;
  win_size = 0; // WINDOW SIZE
  chk_sum = 0; //CHECK SUM
  flags = 1<<6; // SYN
  len = strlen(file->file);

  build_CCP_header(buf, af, &len, &win_size, &flags, &chk_sum);
  memcpy(buf+16, file->file, strlen(file->file));

  af->last_clock = clock();
  n = sendto(CCP_sockfd, buf, len+16, 0, 
	     (struct sockaddr *) &af->partneraddr, sizeof(af->partneraddr));
  if (n< 0)
    error("THAT SHIT DIDNT WORK\n"); 
  
  af->my_seq_n += 1;
  flows[num_flows] = af;
  num_flows += 1;
  return n;
}



int send_CCP_accept(active_flow *flow, int CCP_sockfd){
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);
  
  fseek(flow->file_fd, 0, SEEK_END);
  int size = ftell(flow->file_fd);
  rewind(flow->file_fd);
  
  uint16_t len, win_size, chk_sum;
  uint8_t  flags;
  len = sizeof(int);
  win_size = 0; // WINDOW SIZE()
  chk_sum = 0; // CHECKSUM()
  flags = 1<<7 | 1<<6;
  
  build_CCP_header(buf, flow, &len, &win_size, &flags, &chk_sum);
  
  memcpy(buf+16, &size, len);

  int n = sendto(CCP_sockfd, buf, len+16, 0, 
	     (struct sockaddr *) &(flow->partneraddr), sizeof(flow->partneraddr)); 
  if (n < 0)
    error("ERROR on sendto");
  
  flow->my_seq_n += 1;
  
  return 0;
}






int send_CCP_ack(active_flow *flow, int CCP_sockfd){
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);

  uint16_t len, win_size, chk_sum;
  uint8_t flags;
  len = 0;
  win_size = 0; //WINDOW SIZE
  chk_sum = 0; // CHECK SUM
  flags = 1<<7; // ACK
  
  build_CCP_header(buf, flow, &len, &win_size, &flags, &chk_sum);
  
  int n = sendto(CCP_sockfd, buf, len+16, 0, 
	     (struct sockaddr *) &(flow->partneraddr), sizeof(flow->partneraddr)); 
  if (n < 0 )
    error("ERROR on sendto");
  
  flow->my_seq_n += 1;

  return 0;
}


int send_CCP_data(active_flow *flow, int CCP_sockfd){
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);

  uint16_t len, win_size, chk_sum;
  uint8_t flags;
  win_size = 0; // WINDOW SIZE
  chk_sum = 0; // CHECK SUM
  flags = 1<<7;

  fpos_t curr;
  fgetpos(flow->file_fd, &curr);
  fseek(flow->file_fd, 0, SEEK_END);
  len = ftell(flow->file_fd) - curr; 
  fsetpos(flow->file_fd, &curr);

  if(len > PACKETSIZE - 16) len = PACKETSIZE - 16;  
  else flags = flags | 1<<5; //fin

  build_CCP_header(buf, flow, &len, &win_size, &flags, &chk_sum);
  
  fread(buf+16, len, 1, flow->file_fd);

  int n = sendto(CCP_sockfd, buf, len+16, 0, 
	     (struct sockaddr *) &(flow->partneraddr), sizeof(flow->partneraddr)); 
  if (n < 0 )
    error("ERROR on sendto");

  flow->my_seq_n += 1;
  
  return 0;
}

int send_CCP_ackfin(active_flow *flow, int CCP_sockfd){
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);

  uint16_t len, win_size, chk_sum;
  uint8_t flags;
  len = 0;
  win_size = 0; // WINDOW SIZE
  chk_sum = 0; // CHECK SUM
  flags = 1<<7 | 1<<5;
 
  build_CCP_header(buf, flow, &len, &win_size, &flags, &chk_sum);


  flow->last_clock = 0;
  int n = sendto(CCP_sockfd, buf, 16, 0,
		 (struct sockaddr *) &(flow->partneraddr), sizeof(flow->partneraddr)); 
  if( n < 0 )
    error("ERROR on sendto");
}



// Fills header values and puts data in buf, returns -1 on failure
int CCP_parse_header(char buf[], uint16_t *source, uint16_t *dest, uint16_t *seq_n, 
		     uint16_t *ack_n, uint16_t *len, uint16_t *win_size, uint16_t *ack, uint16_t *syn,
		     uint16_t *fin, uint16_t *chk_sum, uint8_t *id){

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
  memcpy(id, buf+13, 1);
  

  //read flags
  if(buf[12] == (char)0xC){ // 0xC = 1100
    *ack = 1;
    *syn = 1;
    *fin = 0;
  }else if(buf[12] == (char)0xA0){ // 0xA = 1010
    *ack = 1;
    *syn = 0;
    *fin = 1;
  }else if(buf[12] == (char)0x80){ // 0x80 = 1000
    *ack = 1;
    *syn = 0;
    *fin = 0;
  }else if(buf[12] == (char)0x40){ // 0x40 = 0100
    *ack = 0;
    *syn = 1;
    *fin = 0;
  }else{
    printf("Invalid CCP header\n");
    return -1;
  }
  memcpy(buf, buf+16, sizeof(int));
return 0;
}



int handle_CCP_packet(char *buf, struct sockaddr_in clientaddr, int portno, int CCP_sockfd){

  uint16_t source, dest, seq_n, ack_n, len, win_size, ack, syn, fin, chk_sum;
  uint8_t id;

  CCP_parse_header(buf, &source, &dest, &seq_n, &ack_n, &len, &win_size, &ack, &syn, &fin, &chk_sum, &id);

  if(!ack){
    if(!syn)
      printf("Invalid CCP Header");
    else{// new flow
      if(strlen(buf) == 0) printf("Empty CCP request"); // buf contains file request
      else{
	//create new active flow
	active_flow *new = (active_flow *)malloc(sizeof(active_flow));
	new->partneraddr = clientaddr;
	new->destport = dest;
	new->sourceport = portno;
	new->your_seq_n = seq_n+1; // should be 1!
	new->my_seq_n = 0; 
	new->last_clock = 0;
	new->RTTEstimate = 0;
	new->client_fd = 0;
        new->file_fd = fopen(buf, "r");
	new->id = id;
	if( new->file_fd == NULL){ // could not locate file
	  printf("CCP requested a file that does not exist");
	  return -1;
	}
	//add flow to flows
	flows[num_flows] = new;
	num_flows += 1;
	
	send_CCP_accept(new, CCP_sockfd);
      }
    }
  }else{//Existing Flow
    active_flow *flow = find_flow(id);
    if( flow == NULL){
      printf("Invalid flow ID");
      return -1;
    }

    if(syn){ //SYN-ACK 
      flow->your_seq_n = seq_n; // should be 0
      send_HTTP_header(flow, buf); //buf = file length 
      send_CCP_ack(flow, CCP_sockfd);
    }else if (flow->file_fd != NULL){ // sending file
      flow->your_seq_n += 1; // increment partner sequence number
      send_CCP_data(flow, CCP_sockfd);
    }else{ //recieving data in buf
      int n = write( flow->client_fd, buf, strlen(buf));
      if( n < 0)
	error("ERROR on write");
      send_CCP_ack(flow, CCP_sockfd);
      if(fin){
	send_CCP_ackfin(flow, CCP_sockfd);
	remove_flow(flow->id);
      }
    }
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

  bzero((char *) &CCP_serveraddr, sizeof(CCP_serveraddr));
  CCP_serveraddr.sin_family = AF_INET;
  CCP_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  CCP_serveraddr.sin_port = htons((unsigned short)CCP_portno);

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

    for(int i = 0; i < num_flows; i++){
      if( flows[i]->last_clock )
	break;
    }

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
	  char CCP_buf[PACKETSIZE];
	  bzero(CCP_buf, PACKETSIZE);

	  //RTT TIMING SHOULD BE IMPLEMENTED HERE
	  n = recvfrom(act_fd, CCP_buf, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
	  if (n < 0)
	    error("ERROR in recvfrom");
	  if (n < 16){
	    printf("invalid CCP request");
	    continue; // dont bother parsing
	    }
	  hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	  if (hostp == NULL)
	    error("ERROR on gethostbyaddr");
	  hostaddrp = inet_ntoa(clientaddr.sin_addr);
	  if (hostaddrp == NULL)
	    error("ERROR on inet_ntoa\n");
	  printf("server received backend packet from %s (%s)\n", 
		 hostp->h_name, hostaddrp);

	  handle_CCP_packet(CCP_buf, clientaddr, CCP_portno, CCP_sockfd);
	
	}else{ // act_fd is a client (i.e. GET request)

	  bzero(buf, BUFSIZE);

	  n = read(act_fd, buf, BUFSIZE);
	  if ( n < 0)
	    error("ERROR reading from client");
	  
	  get_request(act_fd, buf, CCP_sockfd);
	  
	  close(act_fd);
	  FD_CLR(act_fd, &active_fds);
	}
      }
    }
  }
}
