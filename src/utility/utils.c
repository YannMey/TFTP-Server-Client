#include "utils.h"
#ifndef RANDOM
#define RANDOM 0
#endif

void debugPacket(struct sockaddr_storage address, int resultSize, char *buf, char *TYPE) {
    char currentAddress[INET6_ADDRSTRLEN];
    puts("<<<<<<<<<<<<");
    printf("%s: got packet from %s\n", TYPE, inet_ntop(address.ss_family, get_in_addr((struct sockaddr *) &address), currentAddress, sizeof currentAddress));
    printf("%s: packet is %d bytes long\n", TYPE, resultSize);
    printf("%s: packet contains \"%.50s\" (truncated to 50 characters)\n", TYPE, buf);
    puts(">>>>>>>>>>");
}

// converts block number to length-2 string
void string_to_int(char *f, int n) {
    if (n == 0) {
        f[0] = '0', f[1] = '0', f[2] = '\0';
    } else if (n % 10 > 0 && n / 10 == 0) {
        char c = (char) (n + '0');
        f[0] = '0', f[1] = c, f[2] = '\0';
    } else if (n % 100 > 0 && n / 100 == 0) {
        char c2 = (char) ((n % 10) + '0');
        char c1 = (char) ((n / 10) + '0');
        f[0] = c1, f[1] = c2, f[2] = '\0';
    } else {
        f[0] = '9', f[1] = '9', f[2] = '\0';
    }
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) { // if IPV4
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

//CHECKS FOR TIMEOUT
int check_timeout(int sockFd, char *buf, struct sockaddr_storage *address, socklen_t addr_len) {
    // set up the file descriptor set
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sockFd, &fds);

    // set up the struct timeval for the timeout
    struct timeval tv;
    tv.tv_sec = TIME_OUT;
    tv.tv_usec = 0;

    // wait until timeout or data received
    int n = select(sockFd + 1, &fds, NULL, NULL, &tv);
    if (n == 0) {
        printf("timeout\n");
        return -2; // timeout!
    } else if (n == -1) {
        printf("error\n");
        return -1; // error
    }

    return recvfromRandom(sockFd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) address, &addr_len);
}

char *inputString(FILE *fp, size_t size) {
    //The size is extended by the input with the value of the provisional
    char *str;
    int ch;
    size_t len = 0;
    str = realloc(NULL, sizeof(char) * size);//size is start size
    if (!str)return str;
    while (EOF != (ch = fgetc(fp)) && ch != '\n') {
        str[len++] = (char) ch;
        if (len == size) {
            str = realloc(str, sizeof(char) * (size += 16));
            if (!str)return str;
        }
    }
    str[len++] = '\0';

    return realloc(str, sizeof(char) * len);
}

// return either 0 for GET, 1 for PUT or 2 for UNKNOWN
requestType parse_command(char **command) {
    requestType status = UNKNOWN;
    char *token = strtok(*command, " ");
    if (token == NULL) {
        return status;
    }
    for (int i = 0; token[i]; i++) {
        token[i] = (char) tolower(token[i]);
    }
    if (strcmp(token, "get") == 0) {
        status = GET;
    } else if (strcmp(token, "put") == 0) {
        status = PUT;
    } else {
        return status;
    }
    // check if next token aka the filename is not NULL
    token = strtok(NULL, " ");
    if (token == NULL) {
        return UNKNOWN;
    }
    // offset the command by the lenght of get/put + 1 whitespace
    *command = *command + 4;
    // check that there was no other arguments afterwards, filename with whitespace are forbidden with that check
    if (strtok(NULL, " ") != NULL) {
        return UNKNOWN;
    }
    return status;
}

// makes RRQ packet
char *make_rrq(char *filename) {
    char *packet;
    packet = (char *) calloc(sizeof(char), 2 + strlen(filename) + 8 + 1);
    memset(packet, 0, sizeof(*packet));
    strcat(packet, "01");//opcode
    strcat(packet, filename);
    strcat(packet, "0octect0");
    return packet;
}

// makes WRQ packet
char *make_wrq(char *filename) {
    char *packet;
    packet = (char *) calloc(sizeof(char), 2 + strlen(filename) + 8 + 1);
    memset(packet, 0, sizeof(*packet));
    strcat(packet, "02");//opcode
    strcat(packet, filename);
    strcat(packet, "0octect0");
    return packet;
}

// makes data packet
char *make_data_pack(int block, char *data) {
    char *packet;
    char temp[3];
    string_to_int(temp, block);
    packet = (char *) calloc(sizeof(char), 4 + strlen(data) + 1);
    memset(packet, 0, sizeof(*packet));
    strcat(packet, "03");//opcode
    strcat(packet, temp);
    strcat(packet, data);
    return packet;
}

// makes ACK packet
char *make_ack(char *block) {
    char *packet;
    packet = malloc(2 + strlen(block) + 1);
    memset(packet, 0, sizeof(*packet));
    strcat(packet, "04");//opcode
    strcat(packet, block);
    return packet;
}

// makes ERR packet
char *make_err(char *errcode, char *errmsg) {
    char *packet;
    packet = (char *) calloc(sizeof(char), 4 + strlen(errmsg) + 1);
    memset(packet, 0, sizeof(*packet));
    strcat(packet, "05");//opcode
    strcat(packet, errcode);
    strcat(packet, errmsg);
    return packet;
}

// return the mode
char *parseWRQRRQHeader(char **msg) {
    // remove the 2 first byte and remove the mode at the end
    *msg = *msg + 2;
    unsigned long len = strlen(*msg) - 2;
    if (*(*msg + len + 1) != '0') {
        return NULL;
    }
    char *mode = (char *) calloc(512, sizeof(char));
    int bufferIndex = 0;
    while (len > 0) {
        if (*(*msg + len) == '0') {
            mode[bufferIndex] = '\0';
            mode = realloc(mode, sizeof(char) * bufferIndex);
            break;
        }
        mode[bufferIndex] = *(*msg + len);
        bufferIndex++;
        len--;
    }
    // reverse mode
    char *tail = mode + bufferIndex - 1;
    char *head = mode;
    for (; head < tail; ++head, --tail) {
        char h = *head, t = *tail;
        *head = t;
        *tail = h;
    }
    // terminate filename
    *(*msg + len) = '\0';
    return mode;
}

packetFormat getOpCode(const char *buf) {
    if (buf[0] == '0') {
        switch (buf[1]) {
            case RRQ:
                return RRQ;
            case WRQ:
                return WRQ;
            case DATA:
                return DATA;
            case ACK:
                return ACK;
            case ERR:
                return ERR;
            default:
                return NONE;
        }
    }
    return NONE;
}

ssize_t recvfromRandom(int s, void *buf, size_t len, int flags,struct sockaddr *from, socklen_t *fromlen){
    size_t res=recvfrom(s,buf,len,flags,from,fromlen);
    if (rand()%MAX_TRIES==0 && RANDOM){
        return -2; // fake timeout
    }
    return res;
}

