/*                                                                                                           
 * udpserver.h - A front-end HTTP server with CCP backend.                                     
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

#define BUFSIZE 1024

typedef struct active_flow {
  struct sockaddr_in partneraddr;
  int addrlen;
  uint16_t destport;
  uint16_t sourceport;
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
int d_index;

/* Global - holds list of all active CCP flows */
active_flow *flows[10];
int num_flows;


void error(char *msg);

int file_not_found();
int peer_add(int connfd, char *request, char *version);
int peer_view(int connfd, char *request, char *version);
int peer_config(int connfd, char *request, char *version);
int peer_status(char *uri);
int get_request(int connfd, char *request);

int send_CCP_request(int connfd, char *file, content_t locations[], int loc_index);
int send_CCP_accept(active_flow *flow);
int send_CCP_ack(active_flow *flow);
int send_CCP_data(active_flow *flow);


int CCP_parse_header(char buf[], uint16_t *source, uint16_t *dest, uint16_t *seq_n, uint16_t *ack_n, 
		     uint16_t *len, uint16_t *win_size, uint16_t *ack, uint16_t *syn, uint16_t *fin,
		     uint16_t *chk_sum);

int handle_CCP_packet(char *buf, char *hostaddrp);
