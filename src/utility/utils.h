#ifndef MYTFTP_UTILS_H
#define MYTFTP_UTILS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>

#define MAX_PORT 5
#define PORT_ARRAY_SIZE (MAX_PORT+1)
#define MAX_ADDRESS 39
#define ADDRESS_SIZE (MAX_ADDRESS+1)
// Utilisation d'une constante x dans la définition
// du format de saisie
#define str(x) # x
#define xstr(x) str(x)
#define MAXBUFLEN 550 // get sockaddr, IPv4 or IPv6: 512+39 +1 -2
#define MAX_READ_LEN 512 // maximum data size that can be sent on one packet
#define MAX_FILENAME_LEN 300 // maximum length of file name supported
#define MAX_PACKETS 99 // maximum number of file packets
#define MAX_TRIES 5 // maximum number of tries if packet times out
#define TIME_OUT 5 // in seconds

typedef enum {
    RRQ = '1', WRQ = '2', DATA = '3', ACK = '4', ERR = '5', NONE = '6'
} packetFormat;

typedef enum {
    GET = 0, PUT = 1, UNKNOWN = 2
} requestType;

void string_to_int(char *f, int n);

char *make_rrq(char *filename);

char *make_wrq(char *filename);

char *make_data_pack(int block, char *data);

char *make_ack(char *block);

char *make_err(char *errcode, char *errmsg);

// return either GET, PUT or UNKNOWN and modify the command accordingly
requestType parse_command(char **command);

packetFormat getOpCode(const char *buf);

// return the mode and modify the msg accordingly
char *parseWRQRRQHeader(char **msg);

void *get_in_addr(struct sockaddr *sa);

int check_timeout(int sockFd, char *buf, struct sockaddr_storage *address, socklen_t addr_len);

char *inputString(FILE *fp, size_t size);

void debugPacket(struct sockaddr_storage address, int resultSize, char *buf, char *TYPE);

ssize_t recvfromRandom(int s, void *buf, size_t len, int flags,struct sockaddr *from, socklen_t *fromlen);

#endif //MYTFTP_UTILS_H
