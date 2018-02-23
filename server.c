/* 
 * server.c - A simple server 
 * usage: server <port>
 */

#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFSIZE 1024

#if 0
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


/*
 *  get_time - prints the date header
 */
char * print_time()
{
  char buf[BUFSIZE];
  time_t t = time(NULL);
  struct tm *tm = gmtime(&t);
  char s[64];
  strftime(s, sizeof(s), "%c", tm); /* formats time into a string */
  sprintf(buf, "Date: %s GMT", s);
  return buf;
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


/*
 * handle_get_request - handles the client GET Request and attempts to open the URI
 */
int handle_get_request(char *request, int connfd)
{
  //  printf("%s\n", request);
 
  char buf[BUFSIZE], content[BUFSIZE], uri[BUFSIZE], version[BUFSIZE], method[BUFSIZE];
  FILE *fp;
  int n;
  char *line;
  
  line = strtok(request, "\r\n");
  //printf("%s\n",line);
  sscanf(line, "%s %s %s", method, uri, version);
  // printf("%s %s %s\n", method, uri, version);

  char tmp[100];
  int range_low=-1, range_high = -1;
  if(strstr(request, "Range:")){
    line = strtok(request, "Range: bytes=");
    line = strtok(NULL, "\r\n"); 
    sscanf(line, "%d-%d", range_low, range_high);
  }

  strncpy(content, "./content", 10);
  strcat(&content, uri);
  // printf("OPEN FILE : %s\n", content);
  fp = fopen(&content, "r");
 
  if (fp == NULL){  //could not locate file 
    // printf("404\n");
    sprintf(buf, "HTTP/1.1 404 Not Found\r\nConnection: Close\r\n\r\n404 File not found");
    int n = write(connfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");
    return 1;


  } else{ //file found
    // printf("OK\n\n");
    char *file_buf, *type;
    uint64_t fsize;
    
    if(range_low == -1 || range_high == -1){ //No range Request
      fseek(fp, 0, SEEK_END);
      fsize = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      
      //status line
      sprintf(buf, "%s 200 OK\r\n", version);
      
      
    }else{  //range request
      fseek(fp, 0, SEEK_END);
      int full_size = ftell(fp);
      fseek(fp, range_low, SEEK_SET);
      fsize = range_high - range_low + 1;
      sprintf(buf, "%s 206 Partial Content\r\nContent Range: bytes %d-%d/%ul", version, range_low,
	      range_high, full_size);
    }


    type = find_type(content);
    //send headers
    sprintf(buf, "%sContent-type: %s\r\n",buf, type);
    sprintf(buf, "%sContent-length: %llu\r\n",buf,fsize);
    sprintf(buf, "%s%s\r\n", buf, print_time());
    sprintf(buf, "%sConnection: Keep-Alive\r\n", buf);
    sprintf(buf, "%sAccept-Ranges: bytes\r\n\r\n", buf);

    
    if( fsize > INT_MAX){
      sprintf(buf, "HTTP/1.1 400  Bad Request\r\nConnection: Close\r\n\r\n400: Bad Request");
      //printf("%s\n", buf);
      n = write(connfd, buf, strlen(buf));
      if (n < 0) 
	error("ERROR writing to socket");      
      return 1;
    }
    
    //printf("%s\n", buf);
    
    n = write(connfd, buf, strlen(buf));
    if (n < 0) 
      error("ERROR writing to socket");      
    

    
    file_buf = malloc(fsize);
    fread(file_buf, fsize+1, 1, fp);
    fclose(fp);      
    n = write(connfd, file_buf, fsize);
    //printf("Bytes written: %d\n", n);
    if (n < 0) 
      error("ERROR writing to socket");      
    free(file_buf);
    
  }

  return 0;

}

int main(int argc, char **argv) {
  int listenfd; /* listening socket */
  int connfd; /* connection socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  fd_set read_fds, active_fds; /* sets of file descriptors */

  /* check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* socket: create a socket */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /* build the server's internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET; /* we are using the Internet */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); /* accept reqs to any IP addr */
  serveraddr.sin_port = htons((unsigned short)portno); /* port to listen on */

  /* bind: associate the listening socket with a port */
  if (bind(listenfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* listen: make it a listening socket ready to accept connection requests */
  if (listen(listenfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");

  /* Initialize socket sets */
  FD_ZERO(&active_fds);
  FD_SET(listenfd, &active_fds);
  int i;


  /* main loop: wait for a connection request, echo input line, 
     then close connection. */
  clientlen = sizeof(clientaddr);
  while (1) {

    read_fds = active_fds;
    if( select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) < 0)
      error("ERROR on select");

    
    for(i = 0; i < FD_SETSIZE; ++i){
      if( FD_ISSET(i, &read_fds )){
	if( i == listenfd ){
	  /* accept: wait for a connection request */
	  connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
	  if (connfd < 0)
	    error("ERROR on accept");

	  /* gethostbyaddr: determine who sent the message */
	  hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	  if (hostp == NULL)
	    error("ERROR on gethostbyaddr");
	  hostaddrp = inet_ntoa(clientaddr.sin_addr);
	  if (hostaddrp == NULL)
	    error("ERROR on inet_ntoa\n");
	  printf("server established connection with %s (%s)\n", 
		 hostp->h_name, hostaddrp);
	  
	  FD_SET(connfd, &active_fds);
	} else {
	  /* read: read input string from the client */
	  bzero(buf, BUFSIZE);
	  n = read(connfd, buf, BUFSIZE);
	  if (n < 0) 
	    error("ERROR reading from socket");
	  // printf("server received %d bytes:\n %s", n, buf);
	  
	  /* open url */
	  handle_get_request(buf, connfd);	  
	  
	  //printf("CLosing\n");
	  close(connfd);
	  FD_CLR(connfd, &active_fds);
	  
	}
      }
    }
  }
}
