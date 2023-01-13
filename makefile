.DEFAULT_GOAL := build

client:
	gcc client.c -lncurses -o client
	./client 8081 8080
server:
	gcc server.c -o server
	./server 8080
build:
	gcc client.c -lncurses -o client
	gcc server.c -o server