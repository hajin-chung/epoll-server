CC=gcc

server: server.c
	$(CC) server.c -o build/server

client: client.c
	$(CC) client.c -o build/client

all: client server
