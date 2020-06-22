#include "../utility/utils.h"


void getRequest(char *command, struct addrinfo *socketInfo, int connectionSocket) {
    // define the structure to store the server information
    socklen_t addr_len;
    struct sockaddr_storage serverAddress;
    addr_len = sizeof serverAddress;

    // buffer and local variable
    int resultSize;
    char buf[MAXBUFLEN];

    // make rrq
    char *message = make_rrq(command);

    // send rrq
    if ((resultSize = sendto(connectionSocket, message, strlen(message), 0,socketInfo->ai_addr, socketInfo->ai_addrlen)) == -1) {
        perror("CLIENT: sendto");
        close(connectionSocket);
        exit(EXIT_FAILURE);
    }
    printf("CLIENT: sent %d bytes\n", resultSize);
    printf("CLIENT: Containing: %.10s\n", message);
    free(message);

    // initialize last received message
    char last_recv_message[MAXBUFLEN];
    strcpy(last_recv_message, "");

    // initialize last ack with the nothing
    char last_sent_ack[5];
    strcpy(last_sent_ack, "");

    // copy filename from command
    char filename[MAX_FILENAME_LEN];
    strcpy(filename, command);
  //  strcat(filename, "_client"); //TODO remove protection

    // open the file
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "CLIENT: error opening file: %s\n", filename);
        close(connectionSocket);
        exit(EXIT_FAILURE);
    }

    // receiving the actual file
    unsigned long size_data = MAX_READ_LEN;

    while (size_data == MAX_READ_LEN) {
        // receiving packet data
        if ((resultSize = recvfrom(connectionSocket, buf, MAXBUFLEN - 1, 0, (struct sockaddr *) &serverAddress, &addr_len)) == -1) {
            perror("CLIENT: recvfrom");
            close(connectionSocket);
            exit(EXIT_FAILURE);
        }

        buf[resultSize] = '\0';
        debugPacket(serverAddress, resultSize, buf, "CLIENT");

        // if the packet is an error one
        if (buf[0] == '0' && buf[1] == '5') {
            fprintf(stderr, "CLIENT: got error packet: %s\n", buf + 4);
            close(connectionSocket);
            exit(EXIT_FAILURE);
        }

        // send ack again is it is the same
        if (strcmp(buf, last_recv_message) == 0) {
            resultSize = sendto(connectionSocket, last_sent_ack, strlen(last_sent_ack), 0,socketInfo->ai_addr, socketInfo->ai_addrlen);
            printf("CLIENT: sent AGAIN %d bytes\n", resultSize);
            printf("CLIENT: Containing: %.10s\n", last_sent_ack);
            continue;
        }

        // writing to file the data packet
        size_data = strlen(buf + 4);
        fwrite(buf + 4, sizeof(char), size_data, fp);
        strcpy(last_recv_message, buf);

        // get the last block id which is at pos 2 and 3 in the packet
        char block[3];
        strncpy(block, buf + 2, 2);
        block[2] = '\0';

        // sending acknowledgement for data packet
        char *ack = make_ack(block);
        if ((resultSize = sendto(connectionSocket, ack, strlen(ack), 0,socketInfo->ai_addr, socketInfo->ai_addrlen)) == -1) {
            perror("CLIENT ACK: sendto");
            close(connectionSocket);
            exit(EXIT_FAILURE);
        }
        printf("CLIENT: sent %d bytes\n", resultSize);
        printf("CLIENT: Containing: %.10s\n", ack);

        // update last ack
        strcpy(last_sent_ack, ack);
        free(ack);
    }
    printf("NEW FILE: %s SUCCESSFULLY MADE\n", filename);
    fclose(fp);
}


void putRequest(char *command, struct addrinfo *socketInfo, int connectionSocket) {
    // define the structure to store the server information
    socklen_t addr_len;
    struct sockaddr_storage serverAddress;
    addr_len = sizeof serverAddress;

    // buffer and local variable
    int resultSize;
    char buf[MAXBUFLEN];

    // make wrq
    char *message = make_wrq(command);
    // send wrq
    if ((resultSize = sendto(connectionSocket, message, strlen(message), 0,socketInfo->ai_addr, socketInfo->ai_addrlen)) == -1) {
        perror("CLIENT: sendto");
        close(connectionSocket);
        exit(EXIT_FAILURE);
    }
    printf("CLIENT: sent %d bytes\n", resultSize);
    printf("CLIENT: Containing: %.10s\n", message);

    // waiting for response of the server
    for (int times = 0; times <= MAX_TRIES; ++times) {
        if (times == MAX_TRIES) {// reached max no. of tries
            printf("CLIENT: MAX NUMBER OF TRIES REACHED\n");
            close(connectionSocket);
            exit(EXIT_FAILURE);
        }

        // checking if timeout has occurred or not
        resultSize = check_timeout(connectionSocket, buf, &serverAddress, addr_len);

        if (resultSize == -1) {         //error
            perror("CLIENT: recvfrom");
            close(connectionSocket);
            exit(EXIT_FAILURE);
        } else if (resultSize == -2) {  //timeout
            printf("CLIENT: try no. %d\n", times + 1);
            int temp_bytes;
            // send ack back again
            if ((temp_bytes = sendto(connectionSocket, message, strlen(message), 0,socketInfo->ai_addr, socketInfo->ai_addrlen)) == -1) {
                perror("CLIENT ACK: sendto");
                close(connectionSocket);
                exit(EXIT_FAILURE);
            }
            printf("CLIENT: sent %d bytes AGAIN\n", temp_bytes);
            printf("CLIENT: Containing: %.10s\n", message);
            continue;
        } else {                        //valid
            break;
        }
    }
    free(message);
    buf[resultSize] = '\0';
    debugPacket(serverAddress, resultSize, buf, "CLIENT");

    if (buf[0] == '0' && buf[1] == ACK) {
        FILE *fp = fopen(command, "rb");
        if (fp == NULL || access(command, F_OK) == -1) {
            fprintf(stderr, "CLIENT: file %s does not exist\n", command);
            close(connectionSocket);
            exit(EXIT_FAILURE);
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
            printf("CLIENT: Read %lu bytes from file\n", read_size);

            // sending data packet
            char *dataPacket = make_data_pack(block, temp);
            if ((resultSize = sendto(connectionSocket, dataPacket, strlen(dataPacket), 0,socketInfo->ai_addr, socketInfo->ai_addrlen)) == -1) {
                perror("CLIENT: sendto");
                close(connectionSocket);
                exit(EXIT_FAILURE);
            }
            printf("CLIENT: sent %d bytes\n", resultSize);
            printf("CLIENT: Containing: %.10s\n", dataPacket);

            // waiting for acknowledgement for data packet
            for (int times = 0; times <= MAX_TRIES; ++times) {
                if (times == MAX_TRIES) {
                    printf("CLIENT: MAX NUMBER OF TRIES REACHED\n");
                    close(connectionSocket);
                    exit(EXIT_FAILURE);
                }

                resultSize = check_timeout(connectionSocket, buf, &serverAddress, addr_len);
                if (resultSize == -1) {                     //error
                    perror("CLIENT: recvfrom");
                    close(connectionSocket);
                    exit(EXIT_FAILURE);
                } else if (resultSize == -2) {              //timeout
                    printf("CLIENT: try no. %d\n", times + 1);
                    int temp_bytes;
                    if ((temp_bytes = sendto(connectionSocket, dataPacket, strlen(dataPacket), 0,socketInfo->ai_addr, socketInfo->ai_addrlen)) == -1) {
                        perror("CLIENT ACK: sendto");
                        close(connectionSocket);
                        exit(EXIT_FAILURE);
                    }
                    printf("CLIENT: sent %d bytes AGAIN\n", temp_bytes);
                    printf("CLIENT: Containing: %.10s\n", dataPacket);
                    continue;
                } else {                                    //valid
                    break;
                }
            }
            free(dataPacket);
            buf[resultSize] = '\0';
            debugPacket(serverAddress, resultSize, buf, "CLIENT");

            if (buf[0] == '0' && buf[1] == ERR) {//if error packet received
                fprintf(stderr, "CLIENT: got error packet: %s\n", buf + 4);
                close(connectionSocket);
                exit(EXIT_FAILURE);
            }

            ++block;
            if (block > MAX_PACKETS) {
                // reset block packet to 1
                block = 1;
            }
        }
        fclose(fp);
    } else {//some bad packed received
        fprintf(stderr, "CLIENT ACK: expecting but got: %s\n", buf);
        close(connectionSocket);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // get the address and the port
    char address[ADDRESS_SIZE], serverPort[PORT_ARRAY_SIZE];
    memset(address, 0, sizeof address);  // Mise à zéro du tampon
    memset(serverPort, 0, sizeof serverPort);  // Mise à zéro du tampon
    if (argc != 3) {
        puts("Entrez le nom du serveur ou son adresse IP : ");
        char *res = inputString(stdin, 20);
        strcpy(address, res);
        if (strcmp(res, address) != 0) {
            printf("You should not use a port outside of the range %d!\n", MAX_PORT);
            free(res);
            return -1;
        }
        free(res);
        puts("Entrez le numéro de port du serveur : ");
        res = inputString(stdin, 20);
        strcpy(serverPort, res);
        if (strcmp(res, serverPort) != 0) {
            printf("You should not use a port outside of the range %d!\n", MAX_PORT);
            free(res);
            return -1;
        }
        free(res);
    } else {
        strcpy(address, argv[1]);
        if (strcmp(argv[1], address) != 0) {
            printf("You should not use an address/host name larger than %d!\n", MAX_ADDRESS);
            return -1;
        }
        strcpy(serverPort, argv[2]);
        if (strcmp(argv[2], serverPort) != 0) {
            printf("You should not use a port with more than %d characters!\n", MAX_PORT);
            return -1;
        }
    }

    // Declare the type of connection: here UDP, IPV6 or IPV4 and unknown address
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;

    // get address information using the provided address for specified port
    int status;
    struct addrinfo *clientInfo;
    if ((status = getaddrinfo(address, serverPort, &hints, &clientInfo)) != 0) {
        fprintf(stderr, "CLIENT: getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    int connectionSocket;
    struct addrinfo *socketInfo;
    // loop through all the results and bind to the first we can (see man getaddrinfo(3))
    for (socketInfo = clientInfo; socketInfo != NULL; socketInfo = socketInfo->ai_next) {
        connectionSocket = socket(socketInfo->ai_family, socketInfo->ai_socktype, socketInfo->ai_protocol);
        if (connectionSocket == -1) {
            perror("CLIENT: cannot open socket");
            continue;
        }

        if (connect(connectionSocket, socketInfo->ai_addr, socketInfo->ai_addrlen) == -1) {
            close(connectionSocket);
            perror("CLIENT: cannot connect");
            continue;
        }
        break;                  /* Success */
    }

    if (socketInfo == NULL) {               /* No address succeeded */
        fprintf(stderr, "No addresses succeeded to connect\n");
        exit(EXIT_FAILURE);
    }

    printf("\nYou are connected to %s:%s \n", address, serverPort);

    puts("You can now enter a GET or PUT command followed by your filename");
    puts("Saisie de la commande : ");
    char *command = inputString(stdin, 20);
    int commandType = parse_command(&command);
    if (strlen(command) >= MAX_FILENAME_LEN) {
        fprintf(stderr, "CLIENT: filename is too long: %s\n", command);
        close(connectionSocket);
        exit(EXIT_FAILURE);
    }
    printf("Using command %s with filename: %s\n", commandType == GET ? "GET" : commandType == PUT ? "PUT" : "UNKNOWN", command);


    if (commandType == GET) {               //GET DATA FROM SERVER
        getRequest(command, socketInfo, connectionSocket);
    } else if (commandType == PUT) {        //WRITE DATA TO SERVER
        putRequest(command, socketInfo, connectionSocket);
    } else {                                //INVALID REQUEST
        fprintf(stderr, "You should only use GET and PUT commands with a valid filename (no whitespace)\n");
        close(connectionSocket);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(clientInfo);
    close(connectionSocket);
    return EXIT_SUCCESS;
}


