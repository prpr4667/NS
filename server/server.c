/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

#define BUFSIZE 4096
#define PKT_SIZE 8192
enum msgTypes { NEW, ACK };
enum commands { GET, PUT, DELETE, LIST, MSG };

enum MSG_ATTRIBUTES { COMMAND, SEQ_NO, PACKET_COUNT, FILE_NAME, MSG_TYPE };
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

/*
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(1);
}

char* add_to_packet(char *file, int packet_offset, char packet[PKT_SIZE]) {
	int start = packet_offset * PKT_SIZE;
	int end = start + PKT_SIZE;
	int i;
	// write file content to packet
	for (i = start; i < end; i += 1) {
		packet[i-start] = file[i];
	}
	return packet;
}

void get_timestamp(char *tvalue) {
	time_t t = time(0);
	struct tm* lt = localtime(&t);
	sprintf(tvalue, "%02d%02d%02d%02d%02d",
			lt->tm_mon + 1, lt->tm_mday,
			lt->tm_hour, lt->tm_min, lt->tm_sec);
	tvalue[strlen(tvalue)] = '\0';
}

void get_file_extension(char filename[100], char file_extension[10]) {
	char prev[10];
	char *ptr = strtok(filename, ".");

	while(ptr != NULL)
	{
		strcpy(prev, ptr);
		ptr = strtok(NULL, ".");
	}
	strcpy(file_extension, prev);
	file_extension[strlen(prev)] = '\0';
}

void set_packet_data(struct Packet *packet, int sequence_number, int type, int prompt){
	packet->prompt = prompt;
	packet->type = type;
	packet->sequence_number = sequence_number;
}

int main(int argc, char **argv) {

	int sockfd; /* socket */
	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char buf[BUFSIZE]; /* message buf */
	char msg[PKT_SIZE];
	char packet_content[PKT_SIZE];
	// packetContents = malloc (PACKET_SIZE * sizeof(char));
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	char command[7];
	char filename[100], file_extension[10];
	char *created_file;
	created_file = malloc (500);
	char *timestamp;
	timestamp = malloc(500);
	int i, j=0;
	FILE *fp;
	char ch;
	long fileSize = 0;
	int packets_sent = 0, current_count = 1, total_packets_in_file = 0;
	struct Packet packet;
	int sequence_number;
	struct timeval tv;
	DIR *d;
	struct dirent *directory;


	/*
	 * check command line arguments
	 */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]);

	/*
	 * socket: create the parent socket
	 */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	tv.tv_sec = 10;
	tv.tv_usec = 0;
	/* setsockopt: Handy debugging trick that lets
	 * us rerun the server immediately after we kill it;
	 * otherwise we have to wait about 20 secs.
	 * Eliminates "ERROR on binding: Address already in use" error.
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
			   (const char*)&tv, sizeof tv);

	// setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
	// 		 (const void *)&optval , sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	/*
	 * bind: associate the parent socket with a port
	 */
	if (bind(sockfd, (struct sockaddr *) &serveraddr,
			 sizeof(serveraddr)) < 0)
		error("ERROR on binding");

	/*
	 * main loop: wait for a datagram, then echo it
	 */
	clientlen = sizeof(clientaddr);
	while (1) {
		/*
		 * recvfrom: receive a UDP datagram from a client
		*/
		bzero(msg, PKT_SIZE);
		n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
					 (struct sockaddr *) &clientaddr, &clientlen);
		if (n > 0) {
			sequence_number = packet.sequence_number;
			if (packet.prompt == GET) {
				// received Get file command from server
				printf("Getting file %s from server \n", packet.filename);
				fp = fopen(packet.filename, "r");

				// if file not present on server, send error message
				if (fp == NULL) {
					printf("File not found on server \n");
					set_packet_data(&packet, sequence_number, NEW, MSG);
					strcpy(packet.message, "File not found");
					n = sendto(sockfd, &packet, sizeof(packet), 0,
							   (struct sockaddr *) &clientaddr, clientlen);
				}
					//file found
					// read entire file into a buffer
				else {
					fseek(fp, 0, SEEK_END);
					// get size of file in bytes
					fileSize = ftell(fp);
					fseek(fp, 0, SEEK_SET);
					char *file = malloc(fileSize + 1);
					fread(file, fileSize, 1, fp);
					fclose(fp);
					file[fileSize] = 0;
//					fileSize = fileSize;
					packets_sent = 1;
					// calculate number of packets to be sent
					if (fileSize > PKT_SIZE) {
						packets_sent = (int)(fsize / PKT_SIZE);
						if ((PKT_SIZE * packets_sent) < fsize) {
							packets_sent += 1;
						}
					}
					printf("\nPackets to be sent = %d\n", packets_sent);
					i=0;
					// first packet
					memcpy(packet.packet_content, add_to_packet(file, i, packet_content), PKT_SIZE);
					sequence_number += 1;
					set_packet_data(&packet, sequence_number, NEW, GET);
					packet.related_packets = packets_sent;
					packet.file_size = fileSize;
					printf("\nSending packet: %d, total packets: %d \n", packet.sequence_number, packet.related_packets);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0,
							   (struct sockaddr *) &clientaddr, clientlen);
					if (packets_sent == 1) {
						// receive ACK for packet if only 1 packet to be sent
						recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
								 (struct sockaddr *) &clientaddr, &clientlen);
						if (packet.type == ACK) {
							printf("Transfer complete...");
						}
					} else {
						while(current_count <= packets_sent) {
							// wait for all subsequent ACKs from client
							n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
										 (struct sockaddr *) &clientaddr, &clientlen);

							// graceful shutdown
							if (n < 0) {
								printf("\nPacket not received. Resending %d \n", packet.sequence_number);
								n = sendto(sockfd, &packet, sizeof(packet), 0,
										   (struct sockaddr *) &clientaddr, clientlen);
							}
								// start sending packets after receiving prev ACK; n = 1
							else if (packet.type == ACK) {
								printf("\n ACK received for %d \n", packet.sequence_number);
								// if(current_count >= packets_sent-1)
								// 	break;
								current_count += 1;
								sequence_number += 1;
								i += 1;
								// reset packet with null
								bzero(packet_content, PKT_SIZE);
								// append to packet content from i packet sizes
								memcpy(packet.packet_content, add_to_packet(file, i, packet_content), PKT_SIZE);
								set_packet_data(&packet, sequence_number, NEW, GET);
								printf("\nSending packet %d \n", packet.sequence_number);
								n = sendto(sockfd, &packet, sizeof(packet), 0,
										   (struct sockaddr *) &clientaddr, clientlen);
							} else {
								printf("Packet %d received out of order: %d \n", packet.type, packet.sequence_number);
							}
						}
					}
				}
			}
			else if (packet.prompt == PUT) {
				// first packet
				total_packets_in_file = packet.related_packets;
				fileSize = packet.file_size;
				i = 0;
				current_count = 0;
				strcpy(filename, packet.filename);
				char *file = NULL;
				file = malloc(500000000);
				printf("\nReceived Packet %d \n\n", packet.sequence_number);
				memcpy(file + (i * PKT_SIZE), packet.packet_content, PKT_SIZE);
				packet.prompt = PUT;
				packet.type = ACK;
				bzero(packet.packet_content, PKT_SIZE);
				printf("\nACK for packet %d \n", packet.sequence_number);
				n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&clientaddr, clientlen);
				i += 1;
				current_count += 1;
				while(current_count < total_packets_in_file) {
					printf("\nCurrent Count: %d, Total packets: %d\n", current_count, total_packets_in_file);
					n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&clientaddr, &clientlen);
					printf("\nReceived Packet %d and current seq no: %d\n", packet.sequence_number, sequence_number);
					if (packet.sequence_number == sequence_number + 1) {
						set_packet_data(&packet, sequence_number, ACK, PUT);
						memcpy(file + (i * PKT_SIZE), packet.packet_content, PKT_SIZE);
						bzero(packet.packet_content, PKT_SIZE);
						printf("ACK for packet: %d \n", packet.sequence_number);
						if(current_count >= total_packets_in_file)
							break;
						n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&clientaddr, clientlen);
						i += 1;
						sequence_number += 1;
						current_count += 1;
					}
					else {
						printf("Received an out of order packet, dropping \n");
					}
				}
				// create the file
				printf("Full file received \n\n");
				get_timestamp(timestamp);
				get_file_extension(filename, file_extension);
				strcat(created_file, filename);
				strcat(created_file, "_");
				strcat(created_file, timestamp);
				strcat(created_file, ".");
				strcat(created_file, file_extension);
				printf("File created on server %s \n", created_file);
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
			else if (packet.prompt == DELETE) {
				n = remove(packet.filename);
				sequence_number += 1;
				set_packet_data(&packet, sequence_number, NEW, DELETE);

				if (n == 0) {
					strcpy(packet.message, "Removed file!");
				} else {
					strcpy(packet.message, "Error deleting file");
				}
				n = sendto(sockfd, &packet, sizeof(struct Packet), 0,
						   (struct sockaddr *) &clientaddr, clientlen);
				n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
							 (struct sockaddr *) &clientaddr, &clientlen);
				if (packet.type == ACK) {
					printf("Delete Request completed \n");
				}
			}
			else if (packet.prompt == LIST) {
				// list files
				d = opendir("./");
				if (d)
				{
					char files[PKT_SIZE];
					char *fileList;
					fileList = malloc (PKT_SIZE * sizeof(char));
					while ((directory = readdir(d)) != NULL)
					{
						strcat(fileList, directory->d_name);
						strcat(fileList, "\t\t");
						char type[5];
						sprintf(type, "%d", directory->d_type);
						strcat(fileList, type);
						strcat(fileList, "\n");
					}
					for (i = 0; i < strlen(fileList); i++) {
						files[i] = fileList[i];
					}
					closedir(d);
					sequence_number += 1;
					set_packet_data(&packet, sequence_number, NEW, LIST);
					strcpy(packet.packet_content, files);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0,
							   (struct sockaddr *) &clientaddr, clientlen);
					n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
								 (struct sockaddr *) &clientaddr, &clientlen);
					if (n < 0) {
						printf("List Timeout \n");
					}
					if (packet.type == ACK) {
						printf("Listed directory contents \n");
					}
				}
			}
			else {
				printf("Invalid command received");
			}
		} else {
			printf("Waiting message from client \n");
		}
	}
}
