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
#include <uuid/uuid.h>
#include <limits.h>
#include "udpserver.h"

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


int file_not_found(int connfd){
  char buf[BUFSIZE];
  sprintf(buf, "HTTP/1.1 404 Not Found\r\nConnection: Close\r\n\r\n404 File not found");
  int n = write(connfd, buf, strlen(buf));
  if (n < 0) 
    error("ERROR writing to socket");
  return 1;  
}

node *find_neighbor(uuid_t uuid){
  int i;
  for(i = 0; i < num_neighbors; i++){
    if(uuid_compare(neighbors[i]->uuid, uuid) == 0){
      return neighbors[i];
    }
  }
  return NULL;
}

node *find_other_node(uuid_t uuid){
  int i;
  for(i=0; i<num_other_nodes; i++){
    if( uuid_compare(other_nodes[i]->uuid, uuid) == 0 ){
      return other_nodes[i];
    }
  }
  return NULL;
}

node *new_other_node(uuid_t uuid){
  node *n = (node *)malloc(sizeof(struct node));
  uuid_copy(n->uuid,uuid);
  n->seq_n = 0;
  char *name = (char *)malloc(10);
  sprintf(name, "node_%d", num_nodes); 
  num_nodes += 1;
  n->name = name;
  other_nodes[num_other_nodes] = n;
  num_other_nodes += 1;
  return n;
}


int peer_uuid(int connfd, uuid_t uuid, char *version){
  char buf[BUFSIZE], response[BUFSIZE];
  char c_uuid[100];
  uuid_unparse(uuid, c_uuid);
  sprintf(response, "{\"uuid\":\"%s\"}", c_uuid);
      
  sprintf(buf, "%s 200 OK \r\n", version); 
  sprintf(buf, "%sContent-type: %s\r\n", buf, "application/json"); 
  sprintf(buf, "%sContent-length: %lu\r\n",buf, strlen(response));
  sprintf(buf, "%s%s\r\n", buf, print_time());
  sprintf(buf, "%sConnection: Keep-Alive\r\n", buf);
  sprintf(buf, "%sAccept-Ranges: bytes\r\n\r\n%s", buf, response);

  printf("%s", buf);
  int n = write(connfd, buf, strlen(buf));
  if( n < 0)
    error("ERROR on write");
 
  return 0;
}


int peer_neighbors(int connfd, char *version)
{

  int i = 0;
  char buf[BUFSIZE];
  char header[BUFSIZE];
  char uuid[100];
  node *cur_node;
  
  sprintf(buf, "[");
  while (i < num_neighbors)
    {
      cur_node = neighbors[i];
      uuid_unparse(cur_node->uuid, uuid);
      sprintf(buf, "%s{\"uuid\":\"%s\", ", buf, uuid);
      sprintf(buf, "%s\"name\":\"%s\", ", buf, cur_node->name);
      sprintf(buf, "%s\"host\":\"%s\", ", buf, cur_node->host);
      sprintf(buf, "%s\"frontend\":%d, ", buf, cur_node->front_port);
      sprintf(buf, "%s\"backend\":%d, ", buf, cur_node->back_port);
      sprintf(buf, "%s\"metric\":%d}", buf, cur_node->metric);
      if( i != num_neighbors - 1)
	sprintf(buf, "%s,", buf);
      i++;
    }
  sprintf(buf, "%s]", buf);
  
  sprintf(header, "%s 200 OK \r\n", version); 
  sprintf(header, "%sContent-type: %s\r\n", header, "application/json");
  sprintf(header, "%sContent-length: %lu\r\n", header, strlen(buf));
  sprintf(header, "%s%s\r\n", header, print_time());
  sprintf(header, "%sConnection: Keep-Alive\r\n", header);
  sprintf(header, "%sAccept-Ranges: bytes\r\n\r\n", header);


  int n = write(connfd, header, strlen(header));
  if (n < 0)
    error("ERROR writing to socket");

  n = write(connfd, buf, strlen(buf));
  if (n < 0)
    error("ERROR writing to socket");
  return 0;
}




int peer_add(int connfd, char *request, char *version)
{
  //  printf("Starting peer_add: %s\n", request); 
  char *temp, key[100], val[100], buf[BUFSIZE], *file, *uuid_c, *rate;
  int n;
 

  file = (char *)malloc(100);
  uuid_c = (char *)malloc(100);
  rate = (char *)malloc(100);

  if( strstr(request, "rate") == NULL ){
    rate = "";
  }

  bzero(key, 100);
  bzero(val, 100);

  strtok(request, "?");
  temp = strtok(NULL, "&");
  while(temp){
    sscanf(temp, "%[^=]=%s", key, val);
    if(strcmp(key, "") == 0){
      //printf("Invalid GET request\n");
      return -1;
    }      
    if(strcmp(key, "path")==0)
      snprintf(file, sizeof(val), "%s", val); 
    if(strcmp(key, "peer")==0)
      snprintf(uuid_c, sizeof(val), "%s", val);
    if(strcmp(key, "rate")==0)
      snprintf(rate, sizeof(val), "%s", val); 
    
    temp = strtok(NULL, "&");
  }    
    
  if(strcmp(file,  "")==0){
    //printf("No file path found\n");
    return -1;
  }else if(strcmp(uuid_c,"")==0){
    //printf("No peer found\n");
    return -1;
  }

  dictionary[d_index] = (content_t)malloc(sizeof(struct content));

  uuid_parse(uuid_c, dictionary[d_index]->uuid);
  dictionary[d_index]->file = file;
  dictionary[d_index]->brate = rate;
  d_index++; 

  sprintf(buf, "%s 200 OK \r\n\r\n", version); 

  //  printf("connfd: %d\n", connfd);
  if ((n = write(connfd, buf, strlen(buf))) < 0)
    error("ERROR on write");
    
  return 0;
}

int peer_addNeighbor(int connfd, char *request, char *version){
  char name[10], *uuid, *host, *frontend, *backend, *metric, key[100], val[100];
    
  uuid = (char *)malloc(100);
  host = (char *)malloc(100);
  frontend = (char *)malloc(100);
  backend = (char *)malloc(100);
  metric =  (char *)malloc(100);
  
  bzero(key, 100);
  bzero(val, 100);

  
  char buf[BUFSIZE];
  sprintf(buf, "%s 200 OK \r\n\r\n", version);
  int n = write(connfd, buf, strlen(buf));
  if(n < 0)
    error("ERROR on write");

 

  strtok(request, "?");
  char *temp = strtok(NULL, "&");
 
  
  while(temp != NULL){
  
    sscanf(temp, "%[^=]=%s", key, val);
    if(strcmp(key, "") == 0){
      //printf("Invalid GET request\n");
      return -1;
    }      
    if(strcmp(key, "uuid")==0)
      snprintf(uuid, sizeof(val), "%s", val); 
    if(strcmp(key, "host")==0)
      snprintf(host, sizeof(val), "%s", val);
    if(strcmp(key, "frontend")==0)
      snprintf(frontend, sizeof(val), "%s", val); 
    if(strcmp(key, "backend")==0)
      snprintf(backend, sizeof(val), "%s", val); 
    if(strcmp(key, "metric")==0)
      snprintf(metric, sizeof(val), "%s", val); 

    temp = strtok(NULL, "&");
  }



  neighbors[num_neighbors] = (node *)malloc(sizeof(struct node));
  sprintf(name, "node_%d", num_nodes); 
  neighbors[num_neighbors]->name = (char *)malloc(strlen(name));
  neighbors[num_neighbors]->host = (char *)malloc(strlen(host));


  strcpy(neighbors[num_neighbors]->host,host);
  strcpy(neighbors[num_neighbors]->name,name);
  

  uuid_parse(uuid, neighbors[num_neighbors]->uuid);
  uuid_unparse(neighbors[num_neighbors]->uuid, uuid);

  neighbors[num_neighbors]->front_port = atoi(frontend);
  neighbors[num_neighbors]->back_port = atoi(backend);
  neighbors[num_neighbors]->metric = atoi(metric);
  neighbors[num_neighbors]->time = time(NULL);

  num_nodes += 1;
  num_neighbors += 1;
    
  send_link_state(NULL);

  return 0;
}

int peer_kill(){
  exit(0);
  return 0;
}


int peer_map(int connfd, char *version){
  char header[BUFSIZE], JSON_map[BUFSIZE];
  

  int i = 0;
  int j = 0;
  sprintf(JSON_map, "{"); 
  for(i=0; i<map_len; i++){
    sprintf(JSON_map, "%s\"%s\":{", JSON_map, map[i]->name);
    for(j=0; j<map[i]->len; j++){
      sprintf(JSON_map, "%s\"%s\":%d", JSON_map, map[i]->adjacencies[j]->name, map[i]->adjacencies[j]->metric);
      if(j == map[i]->len - 1)
	sprintf(JSON_map, "%s}", JSON_map);
      else
	sprintf(JSON_map, "%s,", JSON_map);
    }
    if( i == map_len -1)
      sprintf(JSON_map, "%s}", JSON_map);
    else
      sprintf(JSON_map, "%s,", JSON_map);
  }

  sprintf(header, "%s 200 OK \r\n", version); 
  sprintf(header, "%sContent-type: %s\r\n", header, "application/json");
  sprintf(header, "%sContent-length: %lu\r\n", header, strlen(JSON_map));
  sprintf(header, "%s%s\r\n", header, print_time());
  sprintf(header, "%sConnection: Keep-Alive\r\n", header);
  sprintf(header, "%sAccept-Ranges: bytes\r\n\r\n", header);


  int n = write(connfd, header, strlen(header));
  if (n < 0)
    error("ERROR writing to socket");
  



  n = write(connfd, JSON_map, strlen(JSON_map));
  if (n<0)
    error("ERROR writing to socket");

  return 0;
}


void remove_from_dictionary(uuid_t uuid){
  int shift = 0;
  for(int i = 0; i<d_index; i++){
    if( uuid_compare(dictionary[i]->uuid, uuid) == 0){
      shift += 1;
      free(dictionary[i]->file);
      free(dictionary[i]->brate);
      free(dictionary[i]);
    }else if(shift){
      dictionary[i-shift] = dictionary[i];
      dictionary[i] = NULL;
    }
  }
  d_index -= shift;
}

void remove_neighbor(int index)
{
  node *n = neighbors[index];
  remove_from_dictionary(n->uuid);
  for (int i=index; i<num_neighbors-1; i++){
    neighbors[i] = neighbors[i+1];
  }
  neighbors[num_neighbors] = NULL;
  num_neighbors -= 1;
  for (int i = 0; i<map[0]->len; i++){
    if(strcmp(map[0]->adjacencies[i]->name, n->name)){
      index = 1;
      break;
    }
  }
  for(int i = index; i<map[0]->len -1; i++){
    map[0]->adjacencies[i] = map[0]->adjacencies[i+1];
  }
  map[0]->adjacencies[map[0]->len] = NULL;
  map[0]->len -= 1;
  free(n->name);
  free(n->host);
  free(n);
  return;
}


void remove_other_node(int index){
  node *n = other_nodes[index];
  remove_from_dictionary(n->uuid);
  for (int i=index; i<num_other_nodes-1; i++)
    {
      other_nodes[i] = other_nodes[i+1];
    }
  free(n->name);
  free(n->host);
  free(n);
  num_other_nodes -= 1;
  return;


}

void remove_map_node_helper(uuid_t uuid){
  int i;
  for(i=0;i<map_len;i++){
    if(uuid_compare(uuid, map[i]->uuid) == 0){
      remove_map_node(i);
    }
  }
}


void remove_map_node(int index){
  vertex *v = map[index];
  for (int i=index; i<map_len-1; i++)
    {
      map[i] = map[i+1];
    }
  free(v);
  num_neighbors -= 1;
  return;

}

//returns JSON String
char *get_neighbor_metrics()
{
  char *buf = (char *)malloc(BUFSIZE);
  char uuid[100];
  node *cur_node;
  
  sprintf(buf, "{");
  int i = 0;
  while (i < num_neighbors)
    {
      cur_node = neighbors[i];
      uuid_unparse(cur_node->uuid, uuid);
      sprintf(buf, "%s\"%s\":", buf, uuid);
      if(i == num_neighbors - 1)
	sprintf(buf, "%s%d", buf, cur_node->metric);
      else
	sprintf(buf, "%s%d,", buf, cur_node->metric);
      i++;
    }
  sprintf(buf, "%s}", buf);
  return buf;
}

int send_link_state(uuid_t killed_uuid)
{
  char buf[BUFSIZE];
  uint16_t num = 0, linkstate_flag = 2; //0010
  char* metrics = get_neighbor_metrics(); //returns JSON string
  uint16_t len = strlen(metrics) + 36;
  struct hostent *server;
  struct sockaddr_in partneraddr;
  char uuid_c[37];
  bzero(uuid_c, 37);


  update_dijkstra = 1;
  

  if(killed_uuid != NULL){
    linkstate_flag = 3;
    len += 36;
  }


  memcpy(buf, &my_node->back_port, 2);
  
  memcpy(buf+4, &my_node->seq_n, 2);
  memcpy(buf+6, &num, 2);
  memcpy(buf+8, &len, 2);
  memcpy(buf+10, &num, 2);
  memcpy(buf+12, &linkstate_flag, 1);
  memcpy(buf+13, &num, 1);
  memcpy(buf+14, &num, 2);
  uuid_unparse(my_node->uuid, uuid_c);
  memcpy(buf+16, uuid_c, 36);
  if(killed_uuid != NULL){
    uuid_unparse(killed_uuid, uuid_c);
    memcpy(buf+52, uuid_c, strlen(uuid_c));
    memcpy(buf+88, metrics, len); 
  }else
    memcpy(buf+52, metrics, len); 
  

  free(metrics);

  for (int i=0; i< num_neighbors; i++)
    {
      memcpy(buf+2, &(neighbors[i]->back_port), 2);

      server = gethostbyname(neighbors[i]->host);
      if(server == NULL){
	fprintf(stderr, "ERROR, no such host as %s\n", neighbors[i]->host);
	exit(0);
      }
      bzero((char *) &partneraddr, sizeof(partneraddr));
      partneraddr.sin_family = AF_INET;
      bcopy((char *)server->h_addr,
	    (char *)&partneraddr.sin_addr.s_addr, server->h_length);
      partneraddr.sin_port = htons((unsigned short)neighbors[i]->back_port);

      int n = sendto(my_node->CCP_sockfd, buf, len+16, 0,
		     (struct sockaddr *) &(partneraddr), sizeof(partneraddr));
      if( n < 0 )
	error("ERROR on sendto");
    }
  my_node->seq_n += 1;
  return 0;
}

int send_keep_alive(uint16_t CCP_portno)
{
  char buf[BUFSIZE], uuid_c[37];
  uint16_t num = 0, keepalive_flag = 1;
  uint16_t uuid_len = 36;

  bzero(uuid_c, 37);

  memcpy(buf, &CCP_portno, 2);

  memcpy(buf+4, &num, 2);
  memcpy(buf+6, &num, 2);
  memcpy(buf+8, &uuid_len, 2);
  memcpy(buf+10, &num, 2);
  memcpy(buf+12, &keepalive_flag, 1);
  memcpy(buf+13, &num, 1);
  memcpy(buf+14, &num, 2);

  uuid_unparse(my_node->uuid, uuid_c);
  memcpy(buf+16, uuid_c, 36);
  
  struct hostent *server;
  struct sockaddr_in partneraddr;
  
  for (int i=0; i< num_neighbors; i++){
    
    memcpy(buf+2, &(neighbors[i]->back_port), 2);
    
    server = gethostbyname(neighbors[i]->host);
    if(server == NULL){
      fprintf(stderr, "ERROR, no such host as %s\n", neighbors[i]->host);
      exit(0);
    }
    bzero((char *) &partneraddr, sizeof(partneraddr));
    partneraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&partneraddr.sin_addr.s_addr, server->h_length);
    partneraddr.sin_port = htons((unsigned short)neighbors[i]->back_port);
    

    int n = sendto(my_node->CCP_sockfd, buf, uuid_len+16, 0,
		   (struct sockaddr *) &(partneraddr), sizeof(partneraddr));
    if( n < 0 )
      error("ERROR on sendto");
  }
  return 0;
}

int handle_search_response(char *buf, int len){
  char file[100], uuid_c[37];
  int location = 68;
  uuid_t peers[BUFSIZE];
  memcpy(file, buf+location, 100);
  location += 100;
  int i =0;
  while(location < len){
    memcpy(uuid_c, buf+location, 36);
    uuid_parse(uuid_c, peers[i]);
    i += 1;
    location += 36;
  }
  int peer_index=i;
  int found;
  for(i = 0; i < peer_index; i++){
    found = 0;
    for(int k = 0; k < d_index; k++){
      if( (uuid_compare(peers[i], dictionary[k]->uuid) == 0) && (strcmp(file, dictionary[k]->file) == 0)){
	found = 1;
	break;
      }
    }
    if(!found){
      dictionary[d_index] = (content_t)malloc(sizeof(struct content));
      dictionary[d_index]->file = malloc(strlen(file));

      uuid_copy(dictionary[d_index]->uuid,  peers[i]);
      memcpy( dictionary[d_index]->file, file, strlen(file));
      dictionary[d_index]->brate = NULL;
      d_index++; 
    }
  }
}


void remove_active_search(char *file){
  int i,found;
  search_t *search;
  for(i = 0; i < num_searches; i++){
    if( strcmp(file, active_searches[i]->file) == 0){
      found = 1;
      search = active_searches[i];
    }else if(found){
      active_searches[i-1] = active_searches[i];
      active_searches[i] = NULL;
    }
  }
  if(!found){
    return;
  }
  free(search->file);
  free(search);
  num_searches -= 1;
}

int peer_search(int connfd, char *request, char *version){
  char buf[BUFSIZE];
  sprintf(buf, "%s 200 OK\r\n", version);
  
  int n = write(connfd, buf, strlen(buf));
  if(n < 0){
    error("ERROR on write");
  }

  char file[BUFSIZE];
  sscanf(request, "/peer/search/%s", file);
  
  search_t *new_search = malloc(sizeof(search_t));
  
  new_search->file = malloc(100);
  strcpy(new_search->file, file);
  new_search->ttl = my_node->search_ttl;
  new_search->client = connfd;
  new_search->time = time(NULL);
  
  active_searches[num_searches] = new_search;
  
  send_search_packet(new_search, NULL); 
}


int send_search_packet(search_t *search, uuid_t sender){
  uuid_t peers[BUFSIZE];
  int peer_index;
  for (int i=0; i<d_index; i++){
    if (strcmp(search->file, dictionary[i]->file) == 0) {/* file found */
      uuid_copy(peers[peer_index], dictionary[i]->uuid);
      peer_index++;
    }
  }
  if( have_file(search->file) ){
    uuid_copy(peers[peer_index], my_node->uuid);
    peer_index++;
  }
  

  uint8_t *flags;
  node *node;
  if( sender == NULL ){
    int random_index = time(NULL)%num_neighbors;
    node = neighbors[random_index];
    *flags = 0x04;
  }else{
    node = find_neighbor(sender);
    *flags = 0x05;
  }
  
  char buf[BUFSIZE];
  uint16_t *len, *zero;;   
  

  *len = sizeof(int) + 36 + 100 + 36*peer_index;
  

  memcpy(buf, &(my_node->back_port), 2);
  memcpy(buf+2, &(node->back_port), 2);
  memcpy(buf+4, zero, 2);
  memcpy(buf+6, zero, 2);
  memcpy(buf+8, len, 2);
  memcpy(buf+10, zero, 2);
  memcpy(buf+12, flags, 1);
  memcpy(buf+13, zero, 1);
  memcpy(buf+14, zero, 2);
  
  char uuid_c[37];
  int k = sizeof(int);
  uuid_unparse(my_node->uuid, uuid_c);
  memcpy(buf+16, uuid_c, 36);
  memcpy(buf+52, &(search->ttl), sizeof(int));
  memcpy(buf+52+k, &(search->file), strlen(search->file));
  
  char *location = buf+52+k;
  for(int i = 0; i < peer_index; i++){
    location += 36;
    uuid_parse(uuid_c, peers[i]);
    memcpy(location, uuid_c, 36);
  }

  struct sockaddr_in partneraddr;
  struct hostent *server = gethostbyname(node->host);
  if(server == NULL){
    fprintf(stderr, "ERROR, no such host as %s\n", node->host);
    exit(0);
  }
  bzero((char *) &partneraddr, sizeof(partneraddr));
  partneraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
	(char *)&partneraddr.sin_addr.s_addr, server->h_length);
  partneraddr.sin_port = htons((unsigned short)node->back_port);

  int n = sendto(my_node->CCP_sockfd, buf, (size_t)len+16, 0, &partneraddr, sizeof(partneraddr));
  if (n< 0)
    error("ERROR ON SEND");   
}

int receive_search_packet(char *packet, int len)
{
  uuid_t sender;
  int ttl;
  char file_name[100];
  uuid_t peers[BUFSIZE];
  int bytes = 36 + sizeof(int) + 100;
  int index = 0;
  char sender_temp[36];
  search_t *cur_search;

  memcpy(sender_temp, packet, 36);
  memcpy(ttl, packet+36, 4);
  memcpy(file_name, packet+40, 100);

  uuid_parse(sender_temp, sender);

  for (int i=0; i<num_searches; i++)
    {
      if (strcmp(file_name, active_searches[i]->file) == 0) //already exits
	{
	  cur_search = active_searches[i];
	  break;
	}
    }
  if (cur_search == null)
    {
      cur_search = malloc(sizeof(search_t));
      cur_search->file = malloc(100);
      strcpy(cur_search->file, file);
      cur_search->ttl = ttl;
      cur_search->time = time(NULL);
    }
  while (bytes < len)
    {
      memcpy(temp, *(packet + bytes), 36);
      uuid_parse(temp, peers[index]);
      bytes += sizeof(uuid_t);
      index++;
    }
  for (i=0; i<d_index; i++){
    if (strcmp(file_name, dictionary[i]->file) == 0) {/* file found */
      if (!already_in(peers, dictionary[i]->uuid), peers_index)
	{
	  uuid_copy(peers[index], dictionary[i]->uuid);
	  peer_index++;
	}
    }
  }
  send_search_packet(cur_search, sender);
  return 0;
}




int peer_view(int connfd, char *request, char *version, int CCP_sockfd)
{
  char file[BUFSIZE];
  int i, found=0, loc_index=0;
  content_t locations[BUFSIZE]; /* holds all of the content structs of the requested uri */

  //printf("We are in peer_view: %s\n", request);

  sscanf(request, "/peer/view/%s", file);
  /* search dictionary for file */
  //printf("%d, \n", d_index);
  for (i=0; i<d_index; i++)
    {
      //printf("looking for %s in dictionary, %d\n", file, i);
      //printf("Current: %s\n", dictionary[i]->file);
      if (strcmp(file, dictionary[i]->file) == 0) {/* file found */
	//printf("found\n");
	found = 1;
	locations[loc_index] = dictionary[i];
	loc_index++;
      }
    }    
  
  //  printf("Our file is: %s, our host is: %s\n", locations[0]->file, locations[0]->host);

  if (!found) { 
    file_not_found(connfd);
    return -1;
  }
  /* send 200 OK */
  char buf[BUFSIZE];
  sprintf(buf, "%s 200 OK\r\n", version);
  int n = write(connfd, buf, strlen(buf));
  if(n < 0){
    error("ERROR on write");
  }

  
  int result = send_CCP_request(connfd, locations[0], CCP_sockfd);
  return result;
}

int peer_config(char *uri)
{
  uint16_t max_brate;
  
  strtok(uri, "rate=");
  max_brate = (uint16_t)atoi(strtok(NULL, " "));
  global_bit_rate = max_brate;
  return 0;
}


int peer_status(int connfd, char *version){
  char buf[BUFSIZE];
  char data[BUFSIZE];
  
  sprintf(data, "CURRENT FLOWS (num_flows: %d): \n",  num_flows);
  int i;
  unsigned long long percent = 0;
  char *type; 
  for(i = 0; i < num_flows; i++){
    if(flows[i]->file_fd != NULL){
      type = "Sending";
      percent = (ftell(flows[i]->file_fd)*100)/flows[i]->file_len;
    }else{
      type = "Receiving";
      percent = (flows[i]->received * 100)/ flows[i]->file_len;
    }
    sprintf(data, "%sAction: %s, Flow ID: %d, Client: %d, %s : %llu%% complete. \n", data, type, flows[i]->id,flows[i]->client_fd,
	   flows[i]->file_name, percent);
  }

  sprintf(buf, "%s 200 OK\r\n", version);
  sprintf(buf, "%sContent-type: text/plain\r\n", buf);
  sprintf(buf, "%sContent-length: %lu\r\n", buf, strlen(data));
  sprintf(buf, "%s%s\r\n", buf, print_time());
  sprintf(buf, "%sConnection: Keep-Alive\r\n", buf);
  sprintf(buf, "%sAccept-Ranges: bytes\r\n\r\n", buf);

  sprintf(buf, "%s%s", buf, data);
  
  int n = write(connfd, buf, strlen(buf));
  if(n < 0){
    error("ERROR on write");
  }
 
  return 0;
}

/*
 * returns 0 on success, -1 on failure
 */
int get_request(int connfd, char *request, int CCP_sockfd, uuid_t uuid)
{
  int closeConn = 0;
  //printf("get_request: %s", request);
  
  if(strlen(request) == 0){
    //printf("Pussy HTTP request");
    return -1;
  }


  char *uri, *version;


  if( (strstr(request, "keep-alive") == NULL) ){
    //printf("No keep-alive\n");
    closeConn = 1;
  }


    
  strtok(request, " ");
  uri = strtok(NULL, " ");
  version = strtok(NULL, "\r\n");

  //printf("%s, %s\n", uri,  version);




  if (strstr(uri, "add?")){
    peer_add(connfd, uri, version);
    close(connfd);
    closeConn = 1;
  } 
  else if (strstr(uri, "view")) {
    peer_view(connfd, uri, version, CCP_sockfd);
  }else if(strstr(uri, "kill")) {
    peer_kill();
  }else if (strstr(uri, "config")) {
    peer_config(uri);
  }else if (strstr(uri, "status")) {
    peer_status(connfd, version);
  }else if (strstr(uri, "addneighbor?")) {
    peer_addNeighbor(connfd, uri, version);
  }else if (strstr(uri, "neighbors")) {
    peer_neighbors(connfd,version);
  }else if (strstr(uri, "uuid")) {
    peer_uuid(connfd, uuid, version);
  }else if (strstr(uri, "map")) {
    peer_map(connfd, version);
  }else if (strstr(uri, "rank")) {
    peer_rank(connfd, uri, version);
  }else { /*not a valid get request*/
    //printf("Invalid GET");
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
    //printf("not a valid flow id");
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

  //printf("\n\n\nStarting send_CCP_request\n\n\n");

  af = (active_flow *)malloc(sizeof(active_flow));
  node *node = find_neighbor(file->uuid);
  
  af->destport = node->back_port;
  af->file_name = file->file;
  af->my_seq_n = 0;
  af->client_fd = connfd;
  af->file_fd = NULL;
  af->received = 0;
  //  printf("\n%s:%d\n", file->host, af->destport);

  uint16_t brate;
  if( strlen(file->brate) == 0 )
    brate = 60000;
  else
    brate = atoi(file->brate);
  
  

  struct hostent *server = gethostbyname(node->host);
  if(server == NULL){
    fprintf(stderr, "ERROR, no such host as %s\n", node->host);
    exit(0);
  }
  bzero((char *) &partneraddr, sizeof(partneraddr));
  partneraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
	(char *)&partneraddr.sin_addr.s_addr, server->h_length);
  partneraddr.sin_port = htons((unsigned short)af->destport);

  af->id = rand() % 100;

  af->partneraddr = partneraddr;
  af->max_window = 0;
  
  uint16_t len, win_size,  chk_sum;
  uint8_t flags;
  win_size = af->max_window;
  chk_sum = 0; //CHECK SUM
  flags = 1<<6; // SYN
  len = strlen(file->file);
  
  build_CCP_header(buf, af, &len, &win_size, &flags, &chk_sum);
  memcpy(buf+16, file->file, strlen(file->file));
 
  //printf("flow created, about to send, sock: %d\n", CCP_sockfd);
  
  int serverlen = sizeof(partneraddr);
  af->start = clock();
  n = sendto(CCP_sockfd, buf, len+16, 0, &partneraddr, serverlen);
  if (n< 0)
    error("ERROR ON SEND"); 
  
  af->my_seq_n += 1;
  flows[num_flows] = af;
  num_flows += 1;
  //printf("sent, data sent: %dbytes, num_flows:%d\n\n", n, num_flows); 
  return n;
}


uint16_t calc_window(active_flow *flow){
  uint16_t brate = ( ( flow->brate < global_bit_rate ) ? flow->brate : global_bit_rate );
  uint16_t window = (brate * flow->RTT) / (PACKETSIZE - 16);
  return window;
}


int send_CCP_accept(active_flow *flow, int CCP_sockfd){
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);
  


  fseek(flow->file_fd, 0, SEEK_END);
  uint64_t size = ftell(flow->file_fd);
  rewind(flow->file_fd);
  flow->file_len = size;

  uint16_t len, win_size, chk_sum;
  uint8_t  flags;
  len = sizeof(uint64_t);
  win_size = flow->window_size; // WINDOW SIZE()
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
  //printf("SEND CCP ACK\n"); 
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);

  uint16_t len, win_size, chk_sum;
  uint8_t flags;
  len = 0;
  win_size = flow->window_size; //WINDOW SIZE
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
  //printf("\n\nSEND CCP DATA\n\n");
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);

  uint16_t len, win_size, chk_sum;
  uint8_t flags;
  win_size = flow->max_window; // WINDOW SIZE
  chk_sum = 0; // CHECK SUM
  flags = 1<<7;

  unsigned long temp_len;
  fpos_t curr;

  //  printf("window_size: %d, window_used: %d, max_window: %d\n", flow->window_size, flow->window_used, flow->max_window);

  //need case for looping back in case packets are dropped

  for (int i=flow->window_used; i<flow->window_size; i++){
    //printf("Calculating Length...\n");
    fgetpos(flow->file_fd, &curr);
    temp_len = ftell(flow->file_fd);
    //printf("sending byte: %lu, seq_n: %d\n", temp_len, flow->my_seq_n);
    fseek(flow->file_fd, 0, SEEK_END); 
    temp_len = ftell(flow->file_fd) - temp_len;; 
    fsetpos(flow->file_fd, &curr);
    //printf("done.\n");

    if(temp_len == 0) break;

    if(temp_len > PACKETSIZE - 16) len = PACKETSIZE - 16;  
    else{
      //printf("\nSENDING FIN\n");
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
    flow->window_used += 1;
  }

  return 0;
}





int send_CCP_ackfin(active_flow *flow, int CCP_sockfd){
  //printf("SEND CCP ACKFIN\n\n");
  char buf[PACKETSIZE];
  bzero(buf, PACKETSIZE);

  uint16_t len, win_size, chk_sum;
  uint8_t flags;
  len = 0;
  win_size = flow->max_window; // WINDOW SIZE
  chk_sum = 0; // CHECK SUM
  flags = 1<<7 | 1<<5; // ack-fin
 
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
  
 // printf("PARSE HEADER\n\n");

  memcpy(source, buf, 2);
  memcpy(dest, buf+2, 2);
  memcpy(seq_n, buf+4, 2);
  memcpy(ack_n, buf+6, 2);
  memcpy(len, buf+8, 2);  
  memcpy(win_size, buf+10, 2);
  memcpy(chk_sum, buf+14, 2);
  memcpy(id, buf+13, 1);
  
  
  //read flags
  if(buf[12] == (char)0x01){ 
    handle_keep_alive(buf+16); //uuid
    return 1;
  }if(buf[12] == (char)0x02){
    handle_link_state(buf, *seq_n, *len, 0);//data
    return 1;
  }if(buf[12] == (char)0x03){
    handle_link_state(buf, *seq_n, *len, 1);
    return 1;
  }if(buf[12] == (char)0x04){
    handle_search_packet(buf+16, *len); 
    return 1;
  }if(buf[12] == (char)0x05){
    handle_search_response(buf+16, *len);
    return 1;
  }
		      
  if(buf[12] == (char)0xC0){ // 0xC0 = 1100
    //printf("ACK SYN\n");
    *ack = 1;
    *syn = 1;
    *fin = 0;
  }else if(buf[12] == (char)0xA0){ // 0xA0 = 1010
    //printf("ACK FIN\n");
    *ack = 1;
    *syn = 0;
    *fin = 1;
  }else if(buf[12] == (char)0x80){ // 0x80 = 1000
    //printf("ACK\n");
    *ack = 1;
    *syn = 0;
    *fin = 0;
  }else if(buf[12] == (char)0x40){ // 0x40 = 0100
    //printf("SYN\n");
    *ack = 0;
    *syn = 1;
    *fin = 0;
  }else{
    //printf("%x, %c, %c\n", buf[12], buf[12], (char)0x40);
    //printf("Invalid CCP header\n");
    return -1;
  }

  memcpy(data, buf+16, *len);
  data[*len] = '\0';
  

return 0;
}

int handle_keep_alive(char *uuid_c){
  uuid_t uuid;
  
  uuid_parse(uuid_c, uuid);
  node *n = find_neighbor(uuid);
  if( n == NULL) return 0;
  n->time = time(NULL);
  return 0;
}


int handle_link_state(char *data, uint16_t seq_n, uint16_t len, uint8_t killed){
  char uuid_c[37], buf[BUFSIZE], temp[BUFSIZE];
  uuid_t uuid;
  
  update_dijkstra = 1;

  bzero(uuid_c, 37);
  memcpy(temp, buf, len+16);
  data = data+16;
  memcpy(uuid_c, data, 36);
 
  uuid_parse(uuid_c, uuid);
  if(uuid_compare(uuid,my_node->uuid) == 0){
    return 0;
  } 
  node *n = find_neighbor(uuid);
  if( n == NULL )
    n = find_other_node(uuid);
  if( n == NULL ){
    
    n = new_other_node(uuid);
  }
  if( n->seq_n >= seq_n ){
    return 0;
  }
  n->seq_n = seq_n;


  if(killed){
    data = data+36;
    memcpy(uuid_c, data, 36);
    uuid_parse(uuid_c, uuid);
    int j=0;
    for(; j<num_other_nodes; j++){
      if( uuid_compare(other_nodes[j]->uuid, uuid) == 0){
	remove_other_node(j);
	break;
      }
    }
    for(j=0; j<map_len; j++){
      if( uuid_compare(map[j]->uuid, uuid) == 0){
	remove_map_node(j);
	break;
      }
    }
  }
  
  vertex *v = (vertex *)malloc(sizeof(vertex));
  uuid_copy(v->uuid, n->uuid);
  v->name = n->name;
  data = data+37; // removes uuid and { from JSON string
  int metric=0;
  char *line;
  int i=0;

  line = strtok(data, ",");
  while( line != NULL ){
    sscanf(line, "\"%36s\":%d", uuid_c,  &metric);
    uuid_parse(uuid_c, uuid);
    if(uuid_compare(uuid, my_node->uuid) == 0){
      n = my_node;
    }else{
      n = find_neighbor(uuid);
      if( n == NULL )
	n = find_other_node(uuid);
      if( n == NULL ){
	n = new_other_node(uuid);
      }
    }
    adj_vert *adv = (adj_vert *)malloc(sizeof(adj_vert));
    adv->name = n->name;
    adv->metric = metric;
    v->adjacencies[i] = adv;
    i++;
    line = strtok(NULL, ",");
  }
  
  v->len = i;
  int new = 1;
  for(i = 0; i<map_len; i++){
    if( strcmp(map[i]->name, v->name ) == 0){
      map[i] = v;
      new = 0;
    }
  }
  if(new){
    map[map_len] = v;
    map_len += 1;
  }
 
  
  forward_link_state(temp);
  return 0;
}


int forward_link_state(char *buf){
  int i;
  struct hostent *server;
  struct sockaddr_in partneraddr;
  for(i=0; i<num_neighbors; i++){
    server  = gethostbyname(neighbors[i]->host);
    if(server == NULL){
      fprintf(stderr, "ERROR, no such host as %s\n", neighbors[i]->host);
      exit(0);
    }
    bzero((char *) &partneraddr, sizeof(partneraddr));
    partneraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&partneraddr.sin_addr.s_addr, server->h_length);
    partneraddr.sin_port = htons((unsigned short)neighbors[i]->back_port);
    
    int n = sendto(my_node->CCP_sockfd, buf, 32, 0,
		   (struct sockaddr *) &(partneraddr), sizeof(partneraddr));
    if( n < 0 )
      error("ERROR on sendto");
  }
  return 0;
}



int handle_CCP_packet(char *buf, struct sockaddr_in clientaddr, int portno, int CCP_sockfd){
  uint64_t end = clock();
  uint16_t source, dest, seq_n, ack_n, len, win_size, ack, syn, fin, chk_sum;
  uint8_t id;
  char data[PACKETSIZE];

  int k = CCP_parse_header(buf, data, &source, &dest, &seq_n, &ack_n, &len, &win_size, &ack, &syn, &fin, &chk_sum, &id);
  if( k == 1)// link state or search
    return 0;
      
  if(!ack){
    if(syn){
      //printf("NEW FLOW: %d\n", id);
      if(find_flow(id) != NULL) printf("Repeated flow ID\n"); //Invalid flow ID
      else if(strlen(data) == 0) printf("Empty CCP request\n"); // data contains file request
      else{
	//printf("CCP file: %s \n", data);
	//create new active flow
	active_flow *new = (active_flow *)malloc(sizeof(active_flow));
	new->partneraddr = clientaddr;
	new->destport = dest;
	new->sourceport = portno;
	new->your_seq_n = seq_n+1; // should be 1!
	new->my_seq_n = 0; 
	new->start = 0;
	new->RTT = 0;
	new->client_fd = 0;
        new->file_fd = fopen(data, "r");
	new->id = id;
	new->window_size = 2; //start at window size of 2
	new->max_window = win_size;
	new->window_used = 0;
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
   // printf("EXISTING FLOW \n");
    active_flow *flow = find_flow(id);
    if( flow == NULL){
      printf("Invalid flow ID\n");
      return -1;
    }
    //    printf("Flow ID: %d, Seq_n: %d\n", id, seq_n);


    if(syn){ //SYN-ACK 
      //printf("Recieved SYN-ACK\n");
      flow->RTT = (end-flow->start)/1000;

      flow->max_window = calc_window(flow); 

      flow->your_seq_n = seq_n+1; // should be 1
      flow->file_len = *((uint64_t *)data);
      send_HTTP_header(flow); //buf = file length
      send_CCP_ack(flow, CCP_sockfd);
    }else if (flow->file_fd != NULL){ // sending file

      if (flow->your_seq_n != seq_n){
	//printf("\nERROR PACKET DROPPED: Expected: %d, recieved: %d\n", flow->your_seq_n, seq_n);
	
	flow->your_seq_n = seq_n+1;
	
	int diff = ack_n - flow->my_seq_n;
	//	printf("me: %d, needed: %d, diff: %d", flow->my_seq_n, ack_n, diff);
	flow->my_seq_n = ack_n;
	fseek(flow->file_fd, diff * (PACKETSIZE - 16), SEEK_CUR);
	flow->window_size = flow->window_size / 2 + 1;
	flow->window_used = 0;
	send_CCP_data(flow, CCP_sockfd);
	return -1;
      }
      
      //      printf("SENDING FILE \n");
      if( flow->window_used > 0)  flow->window_used -= 1;
      if (flow->window_size <= win_size)
	flow->window_size+=1;

      if(fin){ 
	//printf("REMOVE FLOW: %d\n", flow->id);
	remove_flow(flow->id);
      }else{
	flow->your_seq_n += 1; // increment partner sequence number
	send_CCP_data(flow, CCP_sockfd);
      }
    }else{ //recieving data
      //printf("receiving data\n");

      if (flow->error){
	if ( flow->your_seq_n != seq_n ){
	  return -1;
	}
	flow->error = 0;
      }

      //      printf("%d\n", seq_n);
      if (flow->your_seq_n != seq_n){ //PACKET LOST --
	//printf("\nERROR PACKET DROPPED: Expected: %d, recieved: %d\n", flow->your_seq_n, seq_n);
	flow->error = 1;
	flow->my_seq_n -= 1;
	send_CCP_ack(flow, CCP_sockfd);
	return -1;
      }
      
      
      //printf("sending client %d %d bytes  (%d)\n", flow->client_fd, len, seq_n);

      flow->received += len;
      int n = write(flow->client_fd, data, len);
      if(n < 0)
	error("ERROR on write");

      //printf("Yarnia\n");

      if(fin){
	//printf("CLOSING FLOW: %d\n", flow->id);
	send_CCP_ackfin(flow, CCP_sockfd);
	remove_flow(flow->id);
      }else{
	flow->your_seq_n += 1;
	send_CCP_ack(flow, CCP_sockfd);
      }
      //printf("RECIEVED THE DATA\n");
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

  //  printf("%s\n", buf);

  int n = write(flow->client_fd, buf, strlen(buf));
  if (n < 0)
    {  
      error("ERROR writing to socket");
      return -1;
    }
  //  printf("\nDONEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n");
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

vertex *find_vertex(char *name){
  for(int i =0; i<map_len; i++){
    if(strcmp(map[i]->name, name)==0){
      return map[i];
    }
  }
  return NULL;
}


adj_vert ***generate_graph(char **names){
  int total = num_neighbors + num_other_nodes + 1;
  adj_vert ***graph = (adj_vert ***)malloc(total*sizeof(adj_vert **));
  for(int i =0; i <total; i++){
    graph[i] = malloc(total*sizeof(adj_vert *));
  }   
  
  for(int row = 0; row < total; row++){
    vertex *v = find_vertex(names[row]);
    if(v == NULL) continue;
    
    for(int col = 0; col < total; col++){
      graph[row][col] = NULL;
    
      for(int a = 0; a < v->len; a++){
	
	if(strcmp( v->adjacencies[a]->name, names[col]) == 0){
	    graph[row][col] = v->adjacencies[a];
	}
	
      }
      
    }
  }
  return graph;
}

void swap(adj_vert *a, adj_vert *b){
  char *temp = a->name;
  int i_temp = a->metric;
  a->name = b->name;
  a->metric = b->metric;
  b->name = temp;
  b->metric = i_temp;
}


void dijkstra(){
  
  
  int total = num_neighbors + num_other_nodes+1;
  
  // distance from src to i

  int sptSet[total]; // sptSet[i] will true if vertex i is included in shortest
  // path tree or shortest distance from src to i is finalized


  fill_names();

  adj_vert ***graph = generate_graph(names);

  // Initialize all distances as INFINITE and stpSet[] as false
  for (int i = 0; i < total; i++)
    dist[i] = INT_MAX, sptSet[i] = 0;

  // Distance of source vertex from itself is always 0
  dist[get_index(my_node->name)] = 0;
  // Find shortest path for all vertices
  for (int count = 0; count < total-1; count++)
    {
      // Pick the minimum distance vertex from the set of vertices not
      // yet processed. u is always equal to src in first iteration.
      int u = minDistance(dist, sptSet);

      printf("count = %d, u = %d\n", count, u);
      // Mark the picked vertex as processed
      sptSet[u] = 1;

      // Update dist value of the adjacent vertices of the picked vertex.
      for (int v = 0; v < total; v++)

	// Update dist[v] only if is not in sptSet, there is an edge from 
	// u to v, and total weight of path from src to  v through u is 
	// smaller than current value of dist[v]
	if (!sptSet[v] && graph[u][v] != NULL && dist[u] != INT_MAX && dist[u]+graph[u][v]->metric < dist[v]){
	
	  dist[v] = dist[u] + graph[u][v]->metric;
	}
    }

  free(graph);
  // print the constructed distance array
  // printSolution(dist, V);
}


void fill_names(){
  names[0] = my_node->name;
  for(int i = 0; i<num_neighbors; i++){
    names[i+1] = neighbors[i]->name;
  }
  for(int i = 0; i<num_other_nodes; i++){
    names[i+num_neighbors+1] = other_nodes[i]->name;
  }
}

int get_index(char *name)
{
  int len = num_neighbors + num_other_nodes + 1;
  for (int i=0; i<len; i++){
    if (strcmp(names[i], name) == 0)
      return i;
  }
  return -1;
}


// A utility function to find the vertex with minimum distance value, from
// the set of vertices not yet included in shortest path tree
int minDistance(int dist[], int sptSet[])
{
  // Initialize min value
  int min = INT_MAX, min_index;
  int total = num_neighbors + num_other_nodes + 1;
  for (int v = 0; v < total; v++)

    if (sptSet[v] == 0 && dist[v] <= min)
      min = dist[v], min_index = v;

  return min_index;
}


int peer_rank(int connfd, char*uri, char* version)
{

  char* name_array[BUFSIZE];
  int id_index = 0;
  char *content_path;
  


 
  strtok(uri, "/peer/path/");
  content_path = strtok(NULL, " ");
  


  for (int i=0; i<d_index; i++){

    if (strcmp(dictionary[i]->file, content_path) == 0){

      node *n = find_neighbor(dictionary[i]->uuid);

      if (n == NULL){
	n = find_other_node(dictionary[i]->uuid);
      }if (n==NULL) continue;

      name_array[id_index] = n->name;
      id_index++;
    }
  }


  if(update_dijkstra){
    dijkstra();
    update_dijkstra = 0;
  }


  adj_vert *good_nodes[id_index];
  char *cur_name;
  int i = 0;
  int total = num_neighbors + num_other_nodes +1;
  while (i < id_index)
    {
      good_nodes[i] = (adj_vert *)malloc(sizeof(adj_vert));
      cur_name = name_array[i];
      good_nodes[i]->name = cur_name;
      for(int k =0; k < total; k++){
        if(strcmp(cur_name, names[k]) == 0){
          good_nodes[i]->metric = dist[k];
        }
      }
      i++;
    }
  int min_id;
  for(i =0; i<id_index; i++){  //sort good_nodes
    min_id = i;
    for(int j = i+1; j < id_index; j++)
      if(good_nodes[j]->metric < good_nodes[min_id]->metric)
	min_id = j;
    swap(good_nodes[min_id], good_nodes[i]);
  }

  char buf[BUFSIZE];
  sprintf(buf, "[");
  for(i=0; i<id_index; i++){
    if(i == id_index-1)
      sprintf(buf, "%s{\"%s\":%d}]", buf, good_nodes[i]->name, good_nodes[i]->metric);
    else
      sprintf(buf, "%s{\"%s\":%d},", buf, good_nodes[i]->name, good_nodes[i]->metric);
  }

  char buf2[BUFSIZE];
  sprintf(buf2, "%s 200 OK\r\n", version);
  sprintf(buf2, "%sContent-type: application/json\r\n", buf2);
  sprintf(buf2, "%sContent-length: %lu\r\n", buf2, strlen(buf));
  sprintf(buf2, "%s%s\r\n", buf2, print_time());
  sprintf(buf2, "%sConnection: Keep-Alive\r\n", buf2);
  sprintf(buf2, "%sAccept-Ranges: bytes\r\n\r\n", buf2);

  sprintf(buf2, "%s%s", buf2, buf);

  int n = write(connfd, buf2, strlen(buf2));
  if(n < 0)
    error("ERROR on write");
  
  return 0;
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
  char *node_name, *content_dir;
  uuid_t uuid;
  int i;
  
  /* 
   * check command line arguments 
   */


  char * config_file = "node.conf";  
  bzero(uuid, sizeof(uuid_t));

  if (argc > 1){
    if (argc != 3) {
      fprintf(stderr, "usage: %s or %s -c <config file>\n", argv[0], argv[0]);
      exit(1);
    }else if( strcmp("-c", argv[1])==0){
	config_file = argv[2];
    }else{
      fprintf(stderr, "usage: %s or %s -c <config file>\n", argv[0], argv[0]);
      exit(1);
    }      
  }

  FILE *config_fd = fopen(config_file, "r");
  

  HTTP_portno = 0;
  CCP_portno = 0;

  int search_ttl, search_interval;
  char key[100], val[100];
  node_name = (char *)malloc(100);
  while(fgets(buf, BUFSIZE, config_fd) != NULL){
    
    sscanf(buf, "%s = %[^\n]", key, val);
    if(strcmp(key, "uuid") == 0){
      uuid_parse(val, uuid);
    }else if(strcmp(key, "name") == 0){
      strcpy(node_name, val);
    }else if(strcmp(key, "frontend_port") == 0){
      HTTP_portno = atoi(val);
    }else if(strcmp(key, "backend_port") == 0){
      CCP_portno = atoi(val);
    }else if(strcmp(key, "content_dir") == 0){
      content_dir = val;
    }else if(strcmp(key, "search_ttl") == 0){
      search_ttl = atoi(val);
    }else if(strcmp(key, "search_interval") == 0){
      search_interval = atoi(val);
    }else if(strcmp(key, "peer_count") == 0){
      int peer_count = atoi(val);
      char P_name[100], P_uuid[100], P_hostname[100], P_frontend[100], P_backend[100], P_metric[100]; 
      for(i = 0; i<peer_count; i++){
	fgets(buf, BUFSIZE, config_fd);
	neighbors[i] = (node *)malloc(sizeof(struct node));
	
	sscanf(buf, "%s = %[^\n]", key, val);
	sscanf(val, "%[^,],%[^,],%[^,],%[^,],%[^,]",P_uuid, P_hostname,P_frontend,P_backend,P_metric);
	
	
	sprintf(P_name, "node_%d", num_nodes);
	
	neighbors[i]->name = (char *)malloc(strlen(P_name));
	neighbors[i]->host = (char *)malloc(strlen(P_hostname));
	strcpy(neighbors[i]->name, P_name);
	strcpy(neighbors[i]->host, P_hostname);
	

	uuid_parse(P_uuid, neighbors[i]->uuid);
		
	neighbors[i]->front_port = atoi(P_frontend);
	neighbors[i]->back_port = atoi(P_backend);
	neighbors[i]->metric = atoi(P_metric);
	neighbors[i]->time = time(NULL);
	num_neighbors += 1;
	num_nodes += 1;
      }
    }
    
  }


  if(uuid_is_null(uuid))
    uuid_generate(uuid);
  if(HTTP_portno == 0)
    HTTP_portno = 18345;
  if(CCP_portno == 0)
    CCP_portno = 18346;
  if(strcmp(content_dir,"")==0)
    content_dir = "content/";
  if(search_ttl == 0)
    search_ttl = 15;
  if(search_interval == 0)
    search_interval = 100;

  

  my_node = (node *)malloc(sizeof(struct node));
  uuid_copy(my_node->uuid, uuid);
  my_node->name = node_name;
  my_node->front_port = HTTP_portno;
  my_node->back_port = CCP_portno;
  my_node->seq_n = 1;
  my_node->search_ttl = search_ttl;
  my_node->search_interval = search_interval;


  map[0] = (vertex *)malloc(sizeof(struct vertex));
  map[0]->name = my_node->name;
  uuid_copy(map[0]->uuid, my_node->uuid);

  for(i=0; i < num_neighbors; i++){
    map[0]->adjacencies[i] = (adj_vert *)malloc(sizeof(struct adjacent_vertex));
    map[0]->adjacencies[i]->name = neighbors[i]->name;
    map[0]->adjacencies[i]->metric = neighbors[i]->metric;
  }
  map[0]->len = i;
  map_len += 1;
    




  /* 
   * socket: create the HTTP and CCP sockets 
   */
  CCP_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (CCP_sockfd < 0) 
    error("ERROR opening CCP socket");
  my_node->CCP_sockfd = CCP_sockfd;

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
  time_t my_time = time(NULL);
  int first = 0;
  while (1) {
    time_t cur_time = time(NULL);
    
    if (cur_time - my_time >10){
      if(first < 3){ // ADVERTISE LINK-STATE EEVERY 10 seconds for 30 seconds after booting
	send_link_state(0);
	first += 1;
      }
      my_time = cur_time;
      send_keep_alive(CCP_portno);
      for(i=0; i<num_neighbors; i++){
	if(cur_time - neighbors[i]->time > 30){
	  uuid_t killed_uuid;
	  uuid_copy(killed_uuid, neighbors[i]->uuid);
	  remove_neighbor(i);
	  remove_map_node_helper(neighbors[i]->uuid);
	  char uuid_c[37];
	  uuid_unparse(killed_uuid, uuid_c);
	  
	  send_link_state(killed_uuid);
	  i--;
	} 
      }
    }
    time_t cur_search_time = time(NULL);
    for (i=0; i<num_searches; i++)
      {
	if (cur_search_time - active_searches[i]->time > my_node->search_interval) {
	  if(active_searches[i]->ttl == 0){
	    send_search_JSON(active_searches[i]);
	    remove_active_search(active_searches[i]->file);
	  }else{
	    active_searches[i]->ttl--;
	    send_search_packet(active_searches[i], NULL);
	  }
	}
      }
     
    read_fds = active_fds;
    struct timeval timeout = {0, 10000};
    if( select( FD_SETSIZE, &read_fds, NULL, NULL, &timeout) <0 )
      error("ERROR on select");
    
    int act_fd;
    for(act_fd = 0; act_fd < FD_SETSIZE; ++act_fd){

      if( FD_ISSET(act_fd, &read_fds) ){
	
	if( act_fd == HTTP_sockfd ){ //New connection requested
	  //printf("Creating New Connection...\n");
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


	  n = recvfrom(act_fd, CCP_buf, PACKETSIZE, 0, 
		       (struct sockaddr *) &clientaddr, &clientlen);
	  if (n < 0)
	    error("ERROR in recvfrom");
	  if (n < 16){
	    //printf("Invalid CCP request");
	    continue; // dont bother parsing
	    }
	  hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	  if (hostp == NULL)
	    error("ERROR on gethostbyaddr");
	  hostaddrp = inet_ntoa(clientaddr.sin_addr);
	  if (hostaddrp == NULL)
	    error("ERROR on inet_ntoa\n");
	  //printf("\nserver received backend packet from %s (%s)\n", hostp->h_name, hostaddrp);

	  handle_CCP_packet(CCP_buf, clientaddr, CCP_portno, CCP_sockfd);
	
	}else{ // act_fd is a client (i.e. GET request)

	  bzero(buf, BUFSIZE);

	  //printf("connfd: %d\n", act_fd);
	  n = read(act_fd, buf, BUFSIZE);
	  if ( n < 0)
	    error("ERROR reading from client");
	  if( n == 0){
	    printf("\n\n CLOSING CONNECTION: %d \n\n", act_fd);
	    close(act_fd);
	    FD_CLR(act_fd, &active_fds);
	  }

	  //printf("Recieved: %s\n", buf);
	  int closeConn = 0;
	  if(strlen(buf) != 0){
	    closeConn = get_request(act_fd, buf, CCP_sockfd, uuid);
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
