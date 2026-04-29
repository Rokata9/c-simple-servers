#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INTERNAL_PORT 8080
#define MAX_PAYLOAD_SIZE 10485760 // 10 MB limit

// Reads EXACTLY 'n' bytes to prevent partial TCP reads
bool
read_exact(int socket_fd, void *buffer, size_t n)
{
    size_t bytes_read_total = 0;
    char  *buf_ptr = (char *)buffer;

    while (bytes_read_total < n)
    {
        ssize_t bytes =
            read(socket_fd, buf_ptr + bytes_read_total, n - bytes_read_total);
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
    int      shift = 0;
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
            fprintf(stderr,
                    "Error: Varint is too large for 32-bit integer. \n");
            return false;
        }

        *out_length = result;
    }
}

// ssize_t bytes =
//   read(socket_fd, buf_ptr + bytes_read_total, n - bytes_read_total);  -->
//   doesn't this read them all at once?
