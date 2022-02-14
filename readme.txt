Implemented here is a UDP based reliable file transfer between a remote client and server.
Following commands are implemented.
1. Get <filename>: The client will get a file named <filename> from the server and create a new file in its local with the filename as the created timestamp.
2. Put <filename>: The client will transmit an existing file to server, which will read all the packets and create a new file with the timestamp as the filename.
3. List: The client will list all the files present on the remote server.
4. Delete <filename>: The client will delete a file mathcing the filename on the server.
5. Exit: The client will exit gracefully.


Implementation details:
Incase of PUT and GET, the packet size decided here is 10KB, if a file is greater than 10KB then it is divided into n number of packets
and sent over the network. On the receiver end the incoming packets are accumulated and then once all the packets arrive a file is created.
Since there can be multiple transmission, the file name here will be the timestamp, making it unique.
For reliable transfer: Here the implementation follows send and wait, which will basically send a packet and wait for the ACK to arrive with a timeout of 3seconds.
If no ACK is received the packet is resent.


How to run:

Use the makefile to compile both client and server code.
If you want to compile both, then run
	make
If you want to compile just client or server, then run
	"make client" or "make server"

To start the client, run:
	./client/client localhost <portno>

To start the server, run:
	./server/server <portno>

GET command:
	to get file from server which is in ./server folder, enter:
	Get ./server/test.txt


PUT command:
	to put file to server which is in ./client folder, enter:
	Put ./client/test.txt

DELETE command:
	to delete file on server which is in ./server folder, enter:
	Delete ./server/test.txt