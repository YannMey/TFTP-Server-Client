#include "../utility/utils.h"


int main(int argc, char *argv[]) {
    // Get port
    char listenPort[PORT_ARRAY_SIZE];
    memset(listenPort, 0, sizeof listenPort);  // Mise à zéro du tampon
    if (argc != 2) {
        puts("Entrez le numéro de port utilisé en écoute (entre 1500 et 65000) : ");
        char *res = inputString(stdin, 20);
        strcpy(listenPort, res);
        if (strcmp(res, listenPort) != 0) {
            printf("You should not use a port outside of the range %d!\n", MAX_PORT);
            free(res);
            return -1;
        }
        free(res);
    } else {
        strcpy(listenPort, argv[1]);
        if (strcmp(argv[1], listenPort) != 0) {
            printf("You should not use a port outside of the range %d!\n", MAX_PORT);
            return -1;
        }
    }

    // Declare the type of connection: here UDP, IPV6 or IPV4 and using current ip
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP


    // get address information for the specified port
    int status;
    struct addrinfo *serverInfo;
    if ((status = getaddrinfo(NULL, listenPort, &hints, &serverInfo)) != 0) {
        fprintf(stderr, "SERVER: getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // open the listening socket and bind it to my address
    struct addrinfo *socketInfo;
    int listenSocket;
    // loop through all the results and bind to the first we can (see man getaddrinfo(3))
    for (socketInfo = serverInfo; socketInfo != NULL; socketInfo = socketInfo->ai_next) {
        listenSocket = socket(socketInfo->ai_family, socketInfo->ai_socktype, socketInfo->ai_protocol);
        if (listenSocket == -1) {
            perror("SERVER: socket");
            continue;
        }
        if (bind(listenSocket, socketInfo->ai_addr, socketInfo->ai_addrlen) == -1) {
            close(listenSocket);
            perror("SERVER: bind");
            continue;
        }
        break;                  /* Success */
    }
    if (socketInfo == NULL) {                   /* No address succeeded */
        fprintf(stderr, "SERVER: failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    printf("\nYou are now listening on port %s\n", listenPort);

    char *buf = (char *) calloc(sizeof(char), MAXBUFLEN);
    char *initial_pointer = buf;
    // loop for multiple request (only work one client)
    while (1) {
        buf = initial_pointer;
        printf("SERVER: waiting to recvfrom...\n");
        //WAITING FOR FIRST REQUEST FROM CLIENT - RRQ/WRQ

        // define the structure to store the client information
        struct sockaddr_storage clientAddress;
        socklen_t addrLen;
        addrLen = sizeof(clientAddress);

        // get the result of the first request
        int resultSize;

        if ((resultSize = recvfrom(listenSocket, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &clientAddress, &addrLen)) == -1) {
            fprintf(stderr, "SERVER: error while receiving first packet\n");
            goto end;
        }

        buf[resultSize] = '\0';
        debugPacket(clientAddress, resultSize, buf, "SERVER");
        char optCode = getOpCode(buf);
        char *mode = parseWRQRRQHeader(&buf);
        printf("mode %s\n", mode);
        free(mode);
        printf("Current file name is: %s\n", buf);

        if (optCode == RRQ) {//READ REQUEST
            // copy the current filename
            char filename[MAX_FILENAME_LEN];
            strcpy(filename, buf);

            // checking if file exists
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL || access(filename, F_OK) == -1) { //SENDING ERROR PACKET - FILE NOT FOUND
                fprintf(stderr, "SERVER: file '%s' does not exist, sending error packet\n", filename);
                char *e_msg = make_err("02", "ERROR_FILE_NOT_FOUND");
                printf("%s\n", e_msg);
                sendto(listenSocket, e_msg, strlen(e_msg), 0, (struct sockaddr *) &clientAddress, addrLen);
                free(e_msg);
                goto end;
            }

            // starting file sending operation

            // get total file size
            int block = 1;
            fseek(fp, 0, SEEK_END);
            long total = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // initialize counter
            long remaining = total;
            if (remaining == 0) {
                ++remaining;
            } else if (remaining % MAX_READ_LEN == 0) {
                --remaining;
            }

            while (remaining > 0) {
                // reading file
                char temp[MAX_READ_LEN + 5];
                size_t read_size;
                if (remaining > MAX_READ_LEN) {
                    read_size = fread(temp, MAX_READ_LEN, sizeof(char), fp);
                    temp[MAX_READ_LEN] = '\0';
                    remaining -= (MAX_READ_LEN);
                } else {
                    read_size = fread(temp, remaining, sizeof(char), fp);
                    temp[remaining] = '\0';
                    remaining = 0;
                }
                printf("SERVER: Read %zu bytes from file\n", read_size*strlen(temp));

                // sending data packet
                char *dataPacket = make_data_pack(block, temp);
                if ((resultSize = sendto(listenSocket, dataPacket, strlen(dataPacket), 0, (struct sockaddr *) &clientAddress, addrLen)) == -1) {
                    perror("SERVER DATA PACKET: sendto");
                    exit(EXIT_FAILURE);
                }
                printf("SERVER: sent %d bytes\n", resultSize);
                printf("CLIENT: Containing: %.10s\n",dataPacket);

                // waiting for acknowledgement for data packet
                for (int times = 0; times <= MAX_TRIES; ++times) {
                    if (times == MAX_TRIES) {
                        printf("SERVER: MAX NUMBER OF TRIES REACHED\n");
                        goto end;
                    }

                    resultSize = check_timeout(listenSocket, buf, &clientAddress, addrLen);
                    if (resultSize == -1) {             //error
                        perror("SERVER: recvfrom");
                        exit(EXIT_FAILURE);
                    } else if (resultSize == -2) {      //timeout
                        printf("SERVER: try no. %d\n", times + 1);
                        int temp_bytes;
                        if ((temp_bytes = sendto(listenSocket, dataPacket, strlen(dataPacket), 0, (struct sockaddr *) &clientAddress, addrLen)) == -1) {
                            perror("SERVER: ACK: sendto");
                            exit(EXIT_FAILURE);
                        }
                        printf("SERVER: sent %d bytes AGAIN\n", temp_bytes);
                        printf("CLIENT: Containing: %.10s\n",dataPacket);
                        continue;
                    } else {                            //valid
                        break;
                    }
                }
                free(dataPacket);
                buf[resultSize] = '\0';
                debugPacket(clientAddress, resultSize, buf, "SERVER");

                ++block;
                if (block > MAX_PACKETS) {
                    // reset block packet to 1
                    block = 1;
                }
            }
            fclose(fp);
        } else if (optCode == WRQ) {//WRITE REQUEST

            //SENDING ACKNOWLEDGEMENT
            char last_sent_ack[5];
            char *message = make_ack("00"); // current block is 00
            strcpy(last_sent_ack, message);

            if ((resultSize = sendto(listenSocket, message, strlen(message), 0,(struct sockaddr *) &clientAddress, addrLen)) == -1) {
                perror("SERVER ACK: sendto");
                free(message);
                exit(EXIT_FAILURE);
            }
            free(message);
            printf("SERVER: sent %d bytes\n", resultSize);
            printf("CLIENT: Containing: %.10s\n",message);

            char filename[MAX_FILENAME_LEN];
            strcpy(filename, buf);
            //strcat(filename, "_server"); // TODO remove protection for own file

            // if file exist already
            if (access(filename, F_OK) != -1) { //SENDING ERROR PACKET - DUPLICATE FILE
                fprintf(stderr, "SERVER: file %s already exists, sending error packet\n", filename);
                char *e_msg = make_err("06", "ERROR_FILE_ALREADY_EXISTS");
                sendto(listenSocket, e_msg, strlen(e_msg), 0, (struct sockaddr *) &clientAddress, addrLen);
                free(e_msg);
                goto end;
            }

            // if no permissions
            FILE *fp = fopen(filename, "wb");
            if (fp == NULL || access(filename, W_OK) == -1) { //SENDING ERROR PACKET - ACCESS DENIED
                fprintf(stderr, "SERVER: file %s access denied, sending error packet\n", filename);
                char *e_msg = make_err("02", "ERROR_ACCESS_DENIED");
                sendto(listenSocket, e_msg, strlen(e_msg), 0, (struct sockaddr *) &clientAddress, addrLen);
                free(e_msg);
                goto end;
            }

            char last_recv_message[MAXBUFLEN];
            strcpy(last_recv_message, buf);
            unsigned long size_data = MAX_READ_LEN;
            while (size_data == MAX_READ_LEN) {
                // receive file, read data
                if ((resultSize = recvfrom(listenSocket, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &clientAddress, &addrLen)) == -1) {
                    perror("SERVER: recvfrom");
                    exit(EXIT_FAILURE);
                }
                buf[resultSize] = '\0';
                debugPacket(clientAddress, resultSize, buf, "SERVER");

                // sending last ack as it was not received
                if (strcmp(buf, last_recv_message) == 0) {
                    sendto(listenSocket, last_sent_ack, strlen(last_sent_ack), 0, (struct sockaddr *) &clientAddress, addrLen);
                    continue;
                }

                //WRITING FILE
                size_data = strlen(buf + 4);
                fwrite(buf + 4, sizeof(char), size_data, fp);
                strcpy(last_recv_message, buf);

                //SENDING ACKNOWLEDGEMENT - PACKET DATA
                char block[3];
                strncpy(block, buf + 2, 2); // buf+2 contains block id which is related to that ack
                block[2] = '\0';

                char *ack = make_ack(block);
                if ((resultSize = sendto(listenSocket, ack, strlen(ack), 0, (struct sockaddr *) &clientAddress, addrLen)) == -1) {
                    perror("SERVER ACK: sendto");
                    exit(EXIT_FAILURE);
                }

                printf("SERVER: sent %d bytes\n", resultSize);
                printf("CLIENT: Containing: %.10s\n",ack);
                strcpy(last_sent_ack, ack);
                free(ack);
            }
            printf("NEW FILE: %s SUCCESSFULLY MADE\n", filename);
            fclose(fp);
        } else {
            fprintf(stderr, "INVALID REQUEST\n");
            goto end;
        }
        end:;
    }
    // restore the pointer
    buf = initial_pointer;
    free(buf);

    // this is actually never executed since the while loop is never broken for better usage

    freeaddrinfo(serverInfo);
    close(listenSocket);

    return EXIT_SUCCESS;
}