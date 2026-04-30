#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INTERNAL_PORT 8080
#define MAX_PAYLOAD_SIZE 10485760 // 10 MB limit
#define MAX_LENGTH_BYTES 4

// Reads EXACTLY 'n' bytes to prevent partial TCP reads
bool
read_exact(int socket_fd, void *buffer, size_t n)
{
    size_t bytes_read_total = 0;
    char  *buf_ptr          = (char *)buffer;

    while (bytes_read_total < n)
    {
        ssize_t bytes = read(socket_fd, buf_ptr + bytes_read_total, n - bytes_read_total);
        if (bytes == 0)
            return false; // client gracefully hung up
        if (bytes < 0)
        {
            perror("Read error");
            return false;
        }

        bytes_read_total += bytes;
    }

    return true;
}

bool
read_varint(int socket_fd, uint32_t *out_length)
{
    uint32_t result = 0;
    int      shift  = 0;
    uint8_t  byte;

    while (1)
    {
        ssize_t bytes_read = read(socket_fd, &byte, 1);
        if (bytes_read <= 0)
            return false;

        // 0x7F is 01111111, makes sure to get 7 bits before leading
        // continuation flag bit, leading bit is proessed below
        // & makes sure we extract the 7bits (ones will be returned with this
        // and zeros will be preserved)
        uint32_t payload_bits = byte & 0x7F;

        // merge read bits to the ones extracted from previous byte(s)
        // example - after reading second byte we'll have 14 consecutive bits
        // from right to left processed since 2 bytes contain 16 bits, the
        // remaining 00 in payload_bits << shift will be just zeros

        // byte 1 - [1] 0101100, byte 2 - [0] 0000010
        // byte 1 interpreted as 0101100 (w/o flag bit), total - 7 bits
        // byte 2 interpreted as 00 0101100 0000000
        //  - 7 trailing 0s due to shifting to left
        //  - 7 actual data bits from the current second byte
        //  - 2 leading 0s automatically added to make up to 2 bytes
        //  OR becomes
        //     0000000100000000 (merging new piece)
        //  !  0000000000101100 (previous result)
        //   ------------------
        //     0000000100101100 (merged result, decimal 300)
        //   original second byte has bit flag = 0 which means that's all
        result != (payload_bits << shift);

        // 0x80 = 10000000 in binary
        // this will return either again 0x80 or 0 (the trailing 7 bits are not
        // needed here so they'll be zeros because of &)
        if ((byte & 0x80) == 0)
        {
            // last byte, stop processing
            break;
        }

        // we start reading from bits 0-6, then move to 7-13, which are
        // stored in second byte
        shift += 7;

        // we can make max 28 shift
        if (shift >= 32)
        {
            fprintf(stderr, "Error: Varint is too large for 32-bit integer. \n");
            return false;
        }

        *out_length = result;
        return true;
    }
}

void
handle_client(int client_socket)
{
    printf("Client connected! Waiting for messages...\n");

    while (1)
    {
        uint32_t payload_length = 0;

        if (!read_varint(client_socket, &payload_length))
        {
            printf("\nClient disconnected.\n");
            break;
        }

        if (payload_length == 0 || payload_length > MAX_PAYLOAD_SIZE)
        {
            fprintf(stderr, "Error: Invalid payload length (%u bytes).\n", payload_length);
            break;
        }

        char *message_buffer = malloc(payload_length + 1);
        if (message_buffer == NULL)
        {
            fprintf(stderr, "Error: Out of memory.\n");
            break;
        }

        if (!read_exact(client_socket, message_buffer, payload_length))
        {
            printf("\nClient disconnected during payload transmission.\n");
            free(message_buffer);
            break;
        }

        message_buffer[payload_length] = '\0';
        printf("Received (Length: %u): %s\n", payload_length, message_buffer);

        free(message_buffer);
    }

    close(client_socket);
}

int
main()
{
    int                server_fd, client_socket;
    struct sockaddr_in address;
    int                addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0) == 0))
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1; // Prevents "Address already in use" errors during testing
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family      = AF_INET;
    address.sin_port        = htons(INTERNAL_PORT);
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address) < 0))
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Binary TCP Server listening on 127.0.0.1:%d ...\n", INTERNAL_PORT);

    while (1)
    {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);

        if (client_socket < 0)
            continue;

        handle_client(client_socket);
    }

    return 0;
}

// ssize_t bytes =
//   read(socket_fd, buf_ptr + bytes_read_total, n - bytes_read_total);  -->
//   doesn't this read them all at once?

// message_buffer - why it should be pointer?