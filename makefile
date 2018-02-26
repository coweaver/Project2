CC = gcc
ARGS = -Wall -O2 -I .

all: server

server: udpserver.c
	$(CC) $(ARGS) -o vodserver udpserver.c

client: udpclient.c 
	$(CC) $(ARGS) -o vodclient udpclient.c

clean:
	rm -f *.o vodserver vodclient *~