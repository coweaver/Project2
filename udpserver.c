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
  printf("File Not Found \n");
  return 0;
}

int peer_add(int connfd, char *request, char *version)
{
  printf("Starting peer_add: %s\n", request); 
  char *temp, key[100], val[100], buf[BUFSIZE], *file, *host, *port, *rate;
  int n;
 

  file = (char *)malloc(100);
  host = (char *)malloc(100);
  port = (char *)malloc(100);
  rate = (char *)malloc(100);

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
  dictionary[d_index]->port =  port;
  dictionary[d_index]->brate = rate;
  d_index++; 

  sprintf(buf, "%s 200 OK \r\n file: %s\n host: %s\n port: %s\n brate: %s\n", version, file, host, port, rate); 

  if ((n = write(connfd, buf, strlen(buf))) < 0)
    error("ERROR on write");
    
  return 0;
}



int peer_view(int connfd, char *request, char *version, int CCP_sockfd)
{
  char file[BUFSIZE];
  int i, found=0, loc_index=0;
  content_t locations[BUFSIZE]; /* holds all of the content structs of the requested uri */

  printf("We are in peer_view: %s\n", request);

  sscanf(request, "/peer/view/%s", file);
  /* search dictionary for file */
  printf("%d, \n", d_index);
  for (i=0; i<d_index; i++)
    {
      printf("looking for %s in dictionary, %d\n", file, i);
      printf("Current: %s\n", dictionary[i]->file);
      if (strcmp(file, dictionary[i]->file) == 0) {/* file found */
	found = 1;
	locations[loc_index] = dictionary[i];
	loc_index++;
      }
    }    
  
  printf("Our file is: %s, our host is: %s\n", locations[0]->file, locations[0]->host);

  if (!found) { 
    file_not_found();
    return -1;
  }
  /* send 200 OK */
  char buf[BUFSIZE];
  sprintf(buf, "%s 200 OK\r\n", version);
  write(connfd, buf, strlen(buf));
  
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
  int closeConn = 0;
  printf("get_request: %s", request);
  
  if(strlen(request) == 0){
    printf("Pussy HTTP request");
    return -1;
  }


  char *uri, *version;


  if( (strstr(request, "keep-alive") == NULL) ){
    printf("No keep-alive\n");
    closeConn = 1;
  }

  strtok(request, " ");
  uri = strtok(NULL, " ");
  version = strtok(NULL, "\r\n");

  printf("%s, %s\n", uri,  version);


  if (strstr(uri, "add")){
    peer_add(connfd, uri, version);
    close(connfd);
    closeConn = 1;
  } 
  else if (strstr(uri, "view")) {
    peer_view(connfd, uri, version, CCP_sockfd);
  }
  else if (strstr(uri, "config")) {
    peer_config(connfd, uri, version);
  }
  else if (strstr(uri, "status")) {
    peer_status(uri);
  }
  else { /*not a valid get request*/
    printf("Invalid GET");
    return -1;
  }
  return closeConn;
} 

//Returns active flow struct on sucess, NULL on failure
active_flow *find_flow(uint8_t id){
  int i;
  for(i = 0; i < num_flows; i++){
    if( flows[i]->id == id ){
      return flows[i];
    }
  }
  return NULL;
}

int remove_flow(uint8_t id){
  int i,found;
  active_flow *flow;
  for(i = 0; i < num_flows; i++){
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
  memcpy(buf+8, len, 2);
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

  printf("\n\n\nStarting send_CCP_request\n\n\n");

  af = (active_flow *)malloc(sizeof(active_flow));
  
  af->destport = atoi(file->port);
  af->file_name = file->file;
  af->my_seq_n = 0;
  af->client_fd = connfd;
  af->file_fd = NULL;

  printf("\n%s:%d\n", file->host, af->destport);

  uint16_t brate = atoi(file->brate);


  struct hostent *server = gethostbyname(file->host);
  if(server == NULL){
    fprintf(stderr, "ERROR, no such host as %s\n", file->host);
    exit(0);
  }
  bzero((char *) &partneraddr, sizeof(partneraddr));
  partneraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
	(char *)&partneraddr.sin_addr.s_addr, server->h_length);
  partneraddr.sin_port = htons((unsigned short)af->destport);

  af->id = rand() % 10;

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
  
  printf("flow created, about to send, sock: %d\n", CCP_sockfd);
  
  int serverlen = sizeof(partneraddr);
  n = sendto(CCP_sockfd, buf, len+16, 0, &partneraddr, serverlen);
  if (n< 0)
    error("THAT SHIT DIDNT WORK"); 
  
  af->my_seq_n += 1;
  flows[num_flows] = af;
  num_flows += 1;
  printf("sent, data sent: %dbytes, num_flows:%d\n\n", n, num_flows); 
  return n;
}



int send_CCP_accept(active_flow *flow, int CCP_sockfd){
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);
  


  fseek(flow->file_fd, 0, SEEK_END);
  uint64_t size = ftell(flow->file_fd);
  rewind(flow->file_fd);
  
  uint16_t len, win_size, chk_sum;
  uint8_t  flags;
  len = sizeof(uint64_t);
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

  unsigned long temp_len;
  fpos_t curr;

  printf("Calculating Length...\n");

  fgetpos(flow->file_fd, &curr);

  temp_len = ftell(flow->file_fd);
  printf("sending byte: %lu\n", temp_len);
  fseek(flow->file_fd, 0, SEEK_END); 
  temp_len = ftell(flow->file_fd) - temp_len;; 

  fsetpos(flow->file_fd, &curr);

  printf("done.\n");



  if(temp_len > PACKETSIZE - 16) len = PACKETSIZE - 16;  
  else{
    printf("\nSENDING FIN\n");
    len = (uint16_t)temp_len;
    flags = flags | 1<<5; //fin
  }

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


  int n = sendto(CCP_sockfd, buf, 16, 0,
		 (struct sockaddr *) &(flow->partneraddr), sizeof(flow->partneraddr)); 
  if( n < 0 )
    error("ERROR on sendto");
  return 0;
}



// Fills header values and puts data in buf, returns -1 on failure
int CCP_parse_header(char buf[], char data[], uint16_t *source, uint16_t *dest, uint16_t *seq_n, 
		     uint16_t *ack_n, uint16_t *len, uint16_t *win_size, uint16_t *ack, uint16_t *syn,
		     uint16_t *fin, uint16_t *chk_sum, uint8_t *id){
 
  memcpy(source, buf, 2);
  memcpy(dest, buf+2, 2);
  memcpy(seq_n, buf+4, 2);
  memcpy(ack_n, buf+6, 2);
  memcpy(len, buf+8, 2);  
  memcpy(win_size, buf+10, 2);
  memcpy(chk_sum, buf+14, 2);
  memcpy(id, buf+13, 1);
  

  //read flags
  if(buf[12] == (char)0xC0){ // 0xC0 = 1100
    *ack = 1;
    *syn = 1;
    *fin = 0;
  }else if(buf[12] == (char)0xA0){ // 0xA0 = 1010
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
    printf("%x, %c, %c\n", buf[12], buf[12], (char)0x40);
    printf("Invalid CCP header\n");
    return -1;
  }


  memcpy(data, buf+16, *len);
  data[*len] = '\0';


return 0;
}



int handle_CCP_packet(char *buf, struct sockaddr_in clientaddr, int portno, int CCP_sockfd){

  uint16_t source, dest, seq_n, ack_n, len, win_size, ack, syn, fin, chk_sum;
  uint8_t id;
  char data[PACKETSIZE];
  



  CCP_parse_header(buf, data, &source, &dest, &seq_n, &ack_n, &len, &win_size, &ack, &syn, &fin, &chk_sum, &id);


  if(!ack){
    if(!syn)
      printf("Invalid CCP Header");
    else{// new flow
      if(strlen(data) == 0) printf("Empty CCP request"); // data contains file request
      else{
	printf("CCP file: %s \n", data);
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
        new->file_fd = fopen(data, "r");
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
    printf("Flow ID: %d, Seq_n: %d\n", id, seq_n);

    if(syn){ //SYN-ACK 
      printf("Recieved SYN-ACK\n");
      flow->your_seq_n = seq_n+1; // should be 1
      flow->file_len = *((uint64_t *)data);
      send_HTTP_header(flow); //buf = file length
      send_CCP_ack(flow, CCP_sockfd);
    }else if (flow->file_fd != NULL){ // sending file
      if(fin) 
	remove_flow(flow->id);
      else{
	flow->your_seq_n += 1; // increment partner sequence number
	send_CCP_data(flow, CCP_sockfd);
      }
    }else{ //recieving data
      flow->your_seq_n += 1;
      int n = write( flow->client_fd, data, len);
      if( n < 0)
	error("ERROR on write");
      if(fin){
	printf("CLOSING FLOW: %d", flow->id);
	send_CCP_ackfin(flow, CCP_sockfd);
	remove_flow(flow->id);
      }else{
	send_CCP_ack(flow, CCP_sockfd);
      }
    }
  }
  return num_flows;
}


int send_HTTP_header(active_flow *flow)
{
  char buf[BUFSIZE];
  sprintf(buf, "Content-type: %s\r\n", find_type(flow->file_name)); 
  sprintf(buf, "%sContent-length: %llu\r\n",buf, flow->file_len);
  sprintf(buf, "%s%s\r\n", buf, print_time());
  sprintf(buf, "%sConnection: Keep-Alive\r\n", buf);
  sprintf(buf, "%sAccept-Ranges: bytes\r\n\r\n", buf);

  printf("%s\n", buf);

  int n = write(flow->client_fd, buf, strlen(buf));
  if (n < 0)
    {  
      error("ERROR writing to socket");
      return -1;
    }

  return 0;
}

/*
 *  get_time - prints the date header
 */
char *print_time(){
  char buf[BUFSIZE];
  time_t t = time(NULL);
  struct tm *tm = gmtime(&t);
  char s[64];
  strftime(s, sizeof(s), "%c", tm); /* formats time into a string */
  sprintf(buf, "Date: %s GMT", s);
  char *k = buf;
  return k;
}

char *find_type(char* filename){
  
  if(strstr(filename, ".txt"))
    return "text/plain";
  if(strstr(filename, ".css"))
    return "text/css";
  if(strstr(filename, ".htm") || strstr(filename, ".html"))
    return "text/html";
  if(strstr(filename, ".gif"))
    return "image/gif";
  if(strstr(filename, ".jpg") || strstr(filename, ".jpeg"))
    return "image/jpeg";
  if(strstr(filename, ".png"))
    return "image/png";
  if(strstr(filename, ".js"))
    return "application/javascript";
  if(strstr(filename, ".mp4") || strstr(filename, ".m4v"))
    return "video/mp4";
  if(strstr(filename, ".webm"))
    return "video/webm";
  if(strstr(filename,".ogg"))
    return "video/ogg";
  else
    return "application/octet-stream";
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

    /*    printf("\n\n CURRENT FILE TABLE (d_index: %d): \n", d_index);
    int i;
    for(i = 0; i < d_index; i++){
      printf("file: %s, host:, %s, port: %s\n", dictionary[i]->file,dictionary[i]->host,
	     dictionary[i]->port);
    }
    
    printf(" CURRENT FLOW TABLE (num_flows: %d): \n", num_flows);
    for(i = 0; i < num_flows; i++){
      printf("Flow ID: %d, my_seq_n: %d, your_seq_n: %d, file_name: %s\n", flows[i]->id,flows[i]->my_seq_n,
	     flows[i]->your_seq_n, flows[i]->file_name);
    }
    */
    

    read_fds = active_fds;
    if( select( FD_SETSIZE, &read_fds, NULL, NULL, NULL) <0 )
      error("ERROR on select");
    
    int act_fd;
    for(act_fd = 0; act_fd < FD_SETSIZE; ++act_fd){

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

	  printf("Connection created: %d\n", connfd);

	}
        else if ( act_fd == CCP_sockfd ){ //Backend (CCP) Packet received
	  char CCP_buf[PACKETSIZE];
	  bzero(CCP_buf, PACKETSIZE);

	  //RTT TIMING SHOULD BE IMPLEMENTED HERE
	  n = recvfrom(act_fd, CCP_buf, PACKETSIZE, 0, 
		       (struct sockaddr *) &clientaddr, &clientlen);
	  if (n < 0)
	    error("ERROR in recvfrom");
	  if (n < 16){
	    printf("Invalid CCP request");
	    continue; // dont bother parsing
	    }
	  hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	  if (hostp == NULL)
	    error("ERROR on gethostbyaddr");
	  hostaddrp = inet_ntoa(clientaddr.sin_addr);
	  if (hostaddrp == NULL)
	    error("ERROR on inet_ntoa\n");
	  printf("\nserver received backend packet from %s (%s)\n", 
		 hostp->h_name, hostaddrp);

	  handle_CCP_packet(CCP_buf, clientaddr, CCP_portno, CCP_sockfd);
	
	}else{ // act_fd is a client (i.e. GET request)

	  bzero(buf, BUFSIZE);

	  printf("connfd: %d\n", act_fd);
	  n = read(act_fd, buf, BUFSIZE);
	  if ( n < 0)
	    error("ERROR reading from client");


	  printf("Recieved: %s\n", buf);
	  int closeConn = 0;
	  if(strlen(buf) != 0){
	    closeConn = get_request(act_fd, buf, CCP_sockfd);
	  }
	  if(closeConn){
	    printf("\n\n CLOSING CONNECTION: %d \n\n", act_fd);
	    close(act_fd);
	    FD_CLR(act_fd, &active_fds);
	  }
	}
      }
    }
  }
}
