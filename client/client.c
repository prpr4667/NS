//
// Created by Pratik P R
//

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
#define PKT_SIZE 10240
enum msgTypes { NEW, ACK };
enum commands { GET, PUT, DELETE, LIST, MSG };
struct Packet {
	int prompt;
	int sequence_number;
	char filename[100];
	int type;
	char packet_content[PKT_SIZE];
	char message[100];
	int related_packets;
	int file_size;
};

/* x
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(0);
}

char* add_to_packet(char *file, int increment, char packetContents[PKT_SIZE]) {
	int startIndex = (increment * PKT_SIZE);
	int endIndex = 0;
	int j = 0;
	if (startIndex == 0)
		endIndex = PKT_SIZE;
	else
		endIndex = startIndex + PKT_SIZE;
	for (int i = startIndex; i < endIndex; i+=1) {
		packetContents[j] = file[i];
		j+=1;
	}
	return packetContents;
}

void get_timestamp(char* tvalue) {
	time_t t = time(0);
	struct tm* lt = localtime(&t);
	sprintf(tvalue, "%02d%02d%02d%02d%02d",
			lt->tm_mon + 1, lt->tm_mday,
			lt->tm_hour, lt->tm_min, lt->tm_sec);
	tvalue[strlen(tvalue)] = '\0';
}

void get_file_extension(char filename[100], char extension[10]) {
	char prev[10];
	char *ptr = strtok(filename, ".");

	while(ptr != NULL)
	{
		strcpy(prev, ptr);
		ptr = strtok(NULL, ".");
	}

	strcpy(extension, prev);
	extension[strlen(prev)] = '\0';
}

void set_packet_data(struct Packet *packet, int sequence_number, int type, int prompt){
	packet->prompt = prompt;
	packet->type = type;
	packet->sequence_number = sequence_number;
}


int main(int argc, char **argv) {
	int sockfd, portno, n;
	int serverlen;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
	char buf[BUFSIZE];
	// char *buf = (char*)malloc(BUFSIZE);
	char msg[PKT_SIZE];
	char *file = NULL;  /* declare a pointer, and initialize to NULL */
	// max file receivable is 500MB
	file = malloc (504857600);
	FILE *fp;
	char ch;
	int i;
	char *fname;
	char filename[100], extension[10];
	char *created_file;
	created_file = malloc(500);
	char *timestamp;
	timestamp = malloc(500);
	char *command;
	struct Packet packet;
	int sequence_number = 0;
	char *packetContents;
	packetContents = malloc (PKT_SIZE * sizeof(char));
	int total_packets_in_file = 0, current_count = 1, packets_sent = 0;
	int fileSize = 0;
	int is_valid = 1;

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
		fprintf(stderr,"ERROR, no host on %s\n", hostname);
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
		// take input command from user
		printf("\n\nType a command \nget [file_name] \nput [file_name]\ndelete [file_name]\nls \nexit \n");
		fgets(buf, BUFSIZE, stdin);
		command = strtok(buf, " ");
		fname = strtok(NULL, " ");
		sequence_number += 1;
		packet.sequence_number = sequence_number;
		packet.type = NEW;

		// read the file name from the input
		if (fname != NULL) {
			for (i = 0; i < strlen(fname); i++) {
				if (fname[i] != '\n') {
					filename[i] = fname[i];
				}
			}
		}
		filename[i] = '\0';
		if (strcmp(command, "put") == 0) {
			// read the contents of the file into a buffer
			fp = fopen(filename, "r");
			if (fp == NULL) {
				printf("File not found\n");
				continue;
			}
			i=0;
			long fsize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			char *file = malloc(fsize + 1);
			fread(file, fsize, 1, fp);
			fclose(fp);
			file[fsize] = 0;
			packet.prompt = (int)PUT;
			// calculate the number of packets to be sent
			packets_sent = 1;
			if (fsize > PKT_SIZE) {
				packets_sent = (int)(fsize / PKT_SIZE);
				if ((PKT_SIZE * packets_sent) < fsize) {
					packets_sent += 1;
				}
			}
			i=0;
			// copy first packet contents
			memcpy(packet.packet_content, add_to_packet(file, i, packetContents), PKT_SIZE);
			sequence_number += 1;
			set_packet_data(&packet, sequence_number, NEW, PUT);
			packet.related_packets = packets_sent;
			packet.file_size = fsize;
			strncpy(packet.filename, filename, strlen(filename));
			printf("\nSending packet: %d, total packets: %d \n", packet.sequence_number, packet.related_packets);
			n = sendto(sockfd, &packet, sizeof(struct Packet), 0,
					   (struct sockaddr *) &serveraddr, serverlen);
			if (packets_sent == 1) {
				// if only 1 packet to be sent, then wait for the ACK
				recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
						 (struct sockaddr *) &serveraddr, &serverlen);
				if (packet.type == ACK) {
					printf("Transfer complete");
					continue;
				}
			} else {
				while(current_count != packets_sent) {
					// wait for all subsequent ACKs from client
					n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
								 (struct sockaddr *) &serveraddr, &serverlen);
					if (n < 0) {
						// if ACK receive timed out then resend the previous packet
						printf("\nPacket not received. Resending %d \n", packet.sequence_number);
						n = sendto(sockfd, &packet, sizeof(packet), 0,
								   (struct sockaddr *) &serveraddr, serverlen);
					}
						// start sending packets after receiving prev ACK; n = 1
					else if (packet.type == ACK) {
						printf("\n ACK received for %d \n", packet.sequence_number);
						current_count += 1; sequence_number += 1, i += 1;
						// get packet contents in increments of PACKET_SIZE
						memcpy(packet.packet_content, add_to_packet(file, i, packetContents), PKT_SIZE);
						set_packet_data(&packet, sequence_number, NEW, PUT);
						printf("\nSending packet %d \n", packet.sequence_number);
						n = sendto(sockfd, &packet, sizeof(packet), 0,
								   (struct sockaddr *) &serveraddr, serverlen);
					} else {
						printf("Unknown packet received: %d \n", packet.type);
					}
				}
			}
		}
		else if (strcmp(command, "get") == 0) {
			packet.prompt = (int)GET;
			strcpy(packet.filename, filename);
		}
		else if (strcmp(command, "delete") == 0) {
			packet.prompt = (int)DELETE;
			strcpy(packet.filename, filename);
		}
		else if (strcmp(command, "ls\n") == 0) {
			packet.prompt = (int)LIST;
		}
		else if (strcmp(command, "exit\n") == 0) {
			printf("Exiting...\n");
			return 0;
		}
		else {
			is_valid = 0;
			printf("%s \n", command);
		}
		if (is_valid == 1) {
			// send the command to server
			n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
			if (n < 0)
				printf("error in send \n");
			// receive the response
			n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
			printf("Received command: %d \n", packet.prompt);
			if (n < 0)
				error("Error in receive \n");
			if (packet.type == NEW) {
				sequence_number += 1;
				if (packet.prompt == GET) {
					// number of continuous packets of same type
					total_packets_in_file = packet.related_packets;
					fileSize = packet.file_size;
					i = 0;
					current_count = 1;
					memset(file, 0, sizeof file);
					printf("\nReceived Packet seqNo: %d \n\n", packet.sequence_number);
					// COPY contents from first packet
					memcpy(file + (i * PKT_SIZE), packet.packet_content, PKT_SIZE);
					packet.prompt = GET;
					packet.type = ACK;
					bzero(packet.packet_content, PKT_SIZE);
					printf("Sending ACK for seqNo: %d \n", packet.sequence_number);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
					i += 1;
					current_count += 1;
					// recursively keep receiving following packets if many more
					while(current_count <= total_packets_in_file) {
						n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
						if (packet.sequence_number == sequence_number + 1) {
							set_packet_data(&packet, sequence_number, ACK, GET);
							memcpy(file + (i * PKT_SIZE), packet.packet_content, PKT_SIZE);
							bzero(packet.packet_content, PKT_SIZE);
							printf("\nReceived Packet %d\n", packet.sequence_number);
							// keep copying the packet contents to a file buffer
							printf("ACK for packet: %d \n", packet.sequence_number);
							n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
							i += 1; sequence_number += 1; current_count += 1;
						}
						else {
							printf("Received an out of order packet, dropping \n");
						}
					}
					// create the file
					printf("Full file received \n\n");
					// create the file with the received contents
					get_timestamp(timestamp);
					get_file_extension(filename, extension);
					strcat(created_file, filename);
					strcat(created_file, "_");
					strcat(created_file, timestamp);
					strcat(created_file, ".");
					strcat(created_file, extension);
					printf("File created on client %s \n", created_file);
					fp = fopen(created_file, "w");
					if (fp == NULL) {
						printf("Error opening file");
						return 0;
					}
					for (i = 0; i < (fileSize / sizeof(char)); i++) {
						fprintf(fp, "%c", file[i]);
					}
					fclose(fp);
				}
				else if (packet.prompt == LIST) {
					printf("\n Listing files:\n");
					printf("%s \n", packet.packet_content);
					packet.type = ACK;
					bzero(packet.packet_content, PKT_SIZE);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
				}
				else if (packet.prompt == DELETE) {
					printf("%s \n", packet.message);
					packet.type = ACK;
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
				}
				else if (packet.prompt == MSG) {
					printf("Received message from server: %s \n", packet.message);
				}
			}
		}
	}
}
