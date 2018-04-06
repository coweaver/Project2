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
#include <uuid/uuid.h>


#define BUFSIZE 1024
#define PACKETSIZE 1500


typedef struct active_flow {
  uint8_t id;
  uint8_t error;
  struct sockaddr_in partneraddr;
  int addrlen;
  uint16_t destport;
  uint16_t sourceport;
  uint16_t my_seq_n;
  uint16_t your_seq_n;
  uint16_t window_size;
  uint16_t window_used;
  uint16_t brate;
  uint16_t RTT;
  clock_t start; 
  FILE *file_fd;
  uint64_t file_len;
  uint64_t received;
  int client_fd;
  char *version;
  char *file_name;
  uint16_t max_window;
} active_flow;

typedef struct node {
  uuid_t uuid;
  char *name;
  char *host;
  uint16_t front_port;
  uint16_t back_port;
  int metric;
  int seq_n;
  time_t time;
} node;


typedef struct content *content_t;
struct content {
  char *file;
  uuid_t uuid;
  char *brate;
};

typedef struct adjacent_vertex{
  char *name;
  int metric;
} adj_vert;

typedef struct vertex{
  char *name;
  int len;
  adj_vert *adjacencies[100];
}vertex;

int num_nodes = 2;
/* Global - holds list of neighbors */
node *neighbors[BUFSIZE];
int num_neighbors;
node *other_nodes[BUFSIZE];
int num_other_nodes;
/* Global - defines local node characteristics */
node *my_node;

/* Global - holds the files and paths */
content_t dictionary[BUFSIZE];
int d_index;

/* Global - holds list of all active CCP flows */
active_flow *flows[BUFSIZE];
int num_flows;

/* Global - maximum bitrate */
uint16_t global_bit_rate;


/* Global - JSON string of network map */
vertex *map[BUFSIZE];
int map_len;

void error(char *msg);

int file_not_found();
int peer_add(int connfd, char *request, char *version);
int peer_view(int connfd, char *request, char *version, int CCP_sockfd);
int peer_config(char *uri);
int peer_status(int connfd, char *version);
int peer_uuid(int connfd, uuid_t uuid, char *version);
int peer_addNeighbor(int  connfd, char *request, char *version);
int peer_neighbors(int connfd, char *version);
int peer_map(int connfd, char *version);



int get_request(int connfd, char *request, int CCP_sockfd, uuid_t uuid);

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

uint16_t calc_window(active_flow *flow);

active_flow *find_flow(uint8_t id);

int remove_flow(uint8_t id);

int send_HTTP_header(active_flow *flow);

char *print_time();

char *find_type();

node *find_neighbor(uuid_t uuid);

node *find_other_node(uuid_t uuid);

int handle_keep_alive(char *uuid_c);

int handle_link_state(char *data, uint16_t seq_n, uint16_t len);

int forward_link_state(char *data);

void remove_neighbor(int index);

char *get_neighbor_metrics();

int send_link_state();

int send_keep_alive(uint16_t CCP_portno);
