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
#define PACKETSIZE 1500


typedef struct active_flow {
  uint8_t id;
  struct sockaddr_in partneraddr;
  int addrlen;
  uint16_t destport;
  uint16_t sourceport;
  uint16_t my_seq_n;
  uint16_t your_seq_n;
  clock_t last_clock;
  int RTTEstimate;
  FILE *file_fd;
  uint64_t file_len;
  int client_fd;
  char *version;
  char *file_name;
  uint16_t max_window;
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
active_flow *flows[BUFSIZE];
int num_flows;



void error(char *msg);

int file_not_found();
int peer_add(int connfd, char *request, char *version);
int peer_view(int connfd, char *request, char *version, int CCP_sockfd);
int peer_config(int connfd, char *request, char *version);
int peer_status(char *uri);
int get_request(int connfd, char *request, int CCP_sockfd);

int send_CCP_request(int connfd, content_t file, int CCP_sockfd);
int resend_CCP_request(active_flow *flow, int CCP_sockfd);

int send_CCP_accept(active_flow *flow, int CCP_sockfd);
int send_CCP_ack(active_flow *flow, int CCP_sockfd);
int send_CCP_data(active_flow *flow, int CCP_sockfd);
int send_CCP_ackfin(active_flow *flow, int CCP_sockfd);

int CCP_parse_header(char buf[], char data[], uint16_t *source, uint16_t *dest, uint16_t *seq_n, 
		     uint16_t *ack_n, uint16_t *len, uint16_t *win_size, uint16_t *ack, uint16_t *syn, 
		     uint16_t *fin,uint16_t *chk_sum, uint8_t *id);

int handle_CCP_packet(char *buf, struct sockaddr_in clientaddr, int portno, int CCP_sockfd);

int build_CCP_header(char *buf, active_flow *flow, uint16_t *len, uint16_t *win_size,
		     uint8_t *flags, uint16_t *chk_sum);


active_flow *find_flow(uint8_t id);

int remove_flow(uint8_t id);

int handle_packet_loss(active_flow *flow, uint16_t seq_n);

int synchronize_seq(active_flow *flow, uint16_t seq_n);

int send_HTTP_header(active_flow *flow);

char *print_time();

char *find_type();
