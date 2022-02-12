/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>

#define BUFSIZE 4096
#define PACKET_SIZE 10240
enum msgTypes { NEW, ACK };
enum commands { GET, PUT, DELETE, LIST, MSG };
struct Packet {
	int command;
	int seq_number;
	char filename[100];
	int msgType;
	char packetContents[PACKET_SIZE];
    char message[100];
	int relatedPacketsCount;
	int fileSize;
};

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(0);
}

char* getPacketContents(char *file, int increment, char packetContents[PACKET_SIZE]) {
	// char packetContents[PACKET_SIZE];
	// packetContents = malloc(PACKET_SIZE * sizeof(char));
	int startIndex = (increment * PACKET_SIZE);
	int endIndex = 0;
	int j = 0;
	if (startIndex == 0) 
		endIndex = PACKET_SIZE;
	else 
		endIndex = startIndex * 2;
	for (int i = startIndex; i < endIndex; i+=1) {
		packetContents[j] = file[i];
		j+=1;
	}
	return packetContents;
}

char* getTimeStamp(char tvalue[256]) {
	time_t seconds;
    seconds = time(NULL);
    sprintf(tvalue, "%ld", seconds);
	return tvalue;
}

char* getFileExtension(char filename[100], char extension[10]) {
	int dotIndex = 0, i, j = 0;
	for (i = 0; i < strlen(filename); i++) {
		if (filename[i] == '.') {
			dotIndex = i;
		}
	}
	printf("%d", dotIndex);
	for (i = dotIndex; i < strlen(filename); i++) {
		extension[j] = filename[i];
		j++;
	}
	return extension;
}


int main(int argc, char **argv) {
	int sockfd, portno, n;
	int serverlen;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
	char buf[BUFSIZE];
	// char *buf = (char*)malloc(BUFSIZE);
	char msg[PACKET_SIZE];
	char *file = NULL;  /* declare a pointer, and initialize to NULL */
	file = malloc (1048576 * sizeof *file);
	char *msgId;
	char serverResponse[BUFSIZE];
	FILE *fp;
	char ch;
	int i, currentPacketCount = 1;
	char *fname;
	char filename[100], extension[10];
	char *newFilename;
	char timestamp[256];
	char *command;
    struct Packet packet;
	int sequenceNumber = 0;
	char *packetContents;
	packetContents = malloc (PACKET_SIZE * sizeof(char));
	int relatedPacketsCount = 0;
	int writtenBytes = 0;
	int fileSize = 0;
	int commandValidity = 1, sendPacketCount = 0;

	/* check command line arguments */
	if (argc != 3) {
	   fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
	   exit(0);
	}
	hostname = argv[1];
	portno = atoi(argv[2]);

	/* socket: create the socket */ 
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	/* gethostbyname: get the server's DNS entry */
	server = gethostbyname(hostname);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host as %s\n", hostname);
		exit(0);
	}

	/* build the server's Internet address */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(portno);
	serverlen = sizeof(serveraddr);

	/* get a message from the user */
	// printf("Please enter msg: ");
	while(1) {
		bzero(buf, BUFSIZE);
		printf("Type a command \n 1. Get <File name>, 2. Put <File name>, 3. Delete <File name>, 4. List, 5. Exit \n");
		fgets(buf, BUFSIZE, stdin);
		command = strtok(buf, " ");
		fname = strtok(NULL, " ");
		sequenceNumber += 1;
		packet.seq_number = sequenceNumber;
		packet.msgType = NEW;
		if (fname != NULL) {
			for (i = 0; i < strlen(fname); i++) {
				if (!(fname[i] == '\n')) {
					filename[i] = fname[i];
				}
			}
		}
		filename[i] = '\0';
		if (strcmp(command, "Put") == 0) {
			fp = fopen(filename, "r");
			if (fp == NULL) {
				printf("Invalid file \n");
				continue;
			}
			i=0;
			do {
				ch = fgetc(fp);
				file[i] = ch;
				i++;
			} while (ch != EOF);
			packet.command = (int)PUT;
            fileSize = i * sizeof(char);
			sendPacketCount = 1;
			if (fileSize > PACKET_SIZE) {
				sendPacketCount = (int)(fileSize / PACKET_SIZE);
				if ((PACKET_SIZE * sendPacketCount) < fileSize) {
					sendPacketCount += 1;
				}
			}
			i=0;
			strcpy(packet.packetContents, getPacketContents(file, i, packetContents));
			sequenceNumber += 1;
			packet.seq_number = sequenceNumber;
			packet.command = PUT;
			packet.msgType = NEW;
			packet.relatedPacketsCount = sendPacketCount;
			packet.fileSize = fileSize;
			strncpy(packet.filename, filename, strlen(filename));
			printf("----------------------------------------------------- \n");
			printf("Sending first packet - seqNo: %d, total packet count: %d \n", packet.seq_number, packet.relatedPacketsCount);
			n = sendto(sockfd, &packet, sizeof(struct Packet), 0, 
				(struct sockaddr *) &serveraddr, serverlen);
			if (sendPacketCount == 1) {
				recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
						(struct sockaddr *) &serveraddr, &serverlen);
				if (packet.msgType == ACK) {
					printf("Transfer complete");
					continue;
				}
			} else {
				while(currentPacketCount != sendPacketCount) {
					n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
						(struct sockaddr *) &serveraddr, &serverlen);
					if (n < 0) {
						printf("----------------------------------------------------- \n");
						printf("Packet not received after timeout, sending seqNo: %d again \n", packet.seq_number);
						n = sendto(sockfd, &packet, sizeof(packet), 0, 
							(struct sockaddr *) &serveraddr, serverlen);
					}
					else if (packet.msgType == ACK) {
						printf("----------------------------------------------------- \n");
						printf("Received ACK for Packet seqNo: %d \n", packet.seq_number);
						currentPacketCount += 1; sequenceNumber += 1, i += 1;
						strcpy(packet.packetContents, getPacketContents(file, i, packetContents));
						packet.seq_number = sequenceNumber;
						packet.command = PUT;
						packet.msgType = NEW;
						printf("----------------------------------------------------- \n");
						printf("Sending next packet seqNo: %d \n", packet.seq_number);
						n = sendto(sockfd, &packet, sizeof(packet), 0, 
							(struct sockaddr *) &serveraddr, serverlen);
					} else {
						printf("Unknown packet received: %d \n", packet.msgType);
					}
				}
			}
		}
		else if (strcmp(command, "Get") == 0) {
			packet.command = (int)GET;
			strcpy(packet.filename, filename);
		}
		else if (strcmp(command, "Delete") == 0) {
			packet.command = (int)DELETE;
			strcpy(packet.filename, filename);
		}
		else if (strcmp(command, "List\n") == 0) {
			packet.command = (int)LIST;
		}
		else if (strcmp(buf, "Exit\n") == 0) {
			printf("Goodbye!!!, exiting...\n");
			return 0;
		} 
		else {
			commandValidity = 0;
			printf("Command  not recognized, please enter valid command \n");
		}
		if (commandValidity == 1) {
			n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
			if (n < 0)
				printf("error in send \n");
			n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
			printf("Received command: %d \n", packet.command);
			if (n < 0) 
				error("Error in receive \n");
			if (packet.msgType == NEW) {
				sequenceNumber += 1;
				if (packet.command == GET) {
					relatedPacketsCount = packet.relatedPacketsCount;
					fileSize = packet.fileSize;
					i = 0, currentPacketCount = 1;
					memset(file, 0, sizeof file);
					printf("----------------------------------------------------- \n");
					printf("Received Packet seqNo: %d \n\n", packet.seq_number);
					memcpy(file + (i * PACKET_SIZE), packet.packetContents, PACKET_SIZE * sizeof(char));
					packet.command = GET;
					packet.msgType = ACK;
					bzero(packet.packetContents, PACKET_SIZE);
					// strcpy(packet.packetContents, NULL);
					printf("Sending ACK for seqNo: %d \n", packet.seq_number);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
					i += 1; currentPacketCount += 1;
					while(currentPacketCount <= relatedPacketsCount) {
						n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
						packet.seq_number = sequenceNumber;
						packet.command = GET;
						packet.msgType = ACK;
						bzero(packet.packetContents, PACKET_SIZE);
						// strcpy(packet.packetContents, NULL);
						printf("----------------------------------------------------- \n");
						printf("Received Packet seqNo: %d \n\n", packet.seq_number);
						memcpy(file + (i * PACKET_SIZE), packet.packetContents, PACKET_SIZE * sizeof(char));
						printf("Sending ACK for seqNo: %d \n", packet.seq_number);
						n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
						i += 1; sequenceNumber += 1; currentPacketCount += 1;
					}
					// create the file
					printf("Full file received \n\n");
					strcat(newFilename, getTimeStamp(timestamp));
					strcat(newFilename, getFileExtension(filename, extension));
					printf("New file created: %s \n", newFilename);
					fp = fopen(newFilename, "w");
					if (fp == NULL) {
						printf("Invalid file");
						return 0;
					}
					for (i = 0; i < (fileSize / sizeof(char)); i++) {
						fprintf(fp, "%c", file[i]);
					}
					fclose(fp);
				}
				else if (packet.command == LIST) {
					printf("--------------------Files on server--------------------\n");
					printf("%s \n", packet.packetContents);
					printf("--------------------END--------------------\n");
					packet.msgType = ACK;
					bzero(packet.packetContents, PACKET_SIZE);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
				}
				else if (packet.command == DELETE) {
					printf("%s \n", packet.message);
					packet.msgType = ACK;
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
				}
				else if (packet.command == MSG) {
					printf("Received message from server: %s \n", packet.message);
				}
			}
		}
	}
}
