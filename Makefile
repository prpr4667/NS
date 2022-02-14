default: compile

compile:
	gcc ./client/uftp_client.c -o ./client/client -g
	gcc ./server/uftp_server.c -o ./server/server -g

client:
	gcc ./client/uftp_client.c -o ./client/client -g

server:
	gcc ./server/uftp_server.c -o ./server/server -g