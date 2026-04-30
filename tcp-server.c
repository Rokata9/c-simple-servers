#include <arpa/inet.h> // For socket structures and functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For read(), write(), close()

#define INTERNAL_PORT 8080
#define CHUNKS_SIZE 4096 // 4 KB for streaming

int
main()
{
    int                server_fd, client_socket;
    struct sockaddr_in address;
    int                addrlen = sizeof(address);
    char               buffer[CHUNKS_SIZE];

    // 1. CREATE THE SOCKET
    // AF_INET = IPv4, SOCK_STREAM = TCP protocol
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed.");
        exit(EXIT_FAILURE);
    }

    // configure the address
    address.sin_family = AF_INET;
    address.sin_port   = htons(INTERNAL_PORT);
    // listen only on localhost, the outside world cannot reach this
    address.sin_addr.s_addr = inet_addr("127.0.0.1");

    // bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed.");
        exit(EXIT_FAILURE);
    }

    // 10 - backlog; how many connections the OS kernel will hold in the waiting
    // room before turning people away
    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening securely on http://127.0.0.1:%d ...\n", INTERNAL_PORT);
    printf("Press Ctrl+C to stop.\n\n");

    // A server is just a program stuck in an infinite loop, waiting for work
    while (1)
    {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue; // don't crash, skip to the next user (connection waiting)
        }

        // We read a chunk of data from the OS buffer into our RAM.
        ssize_t bytes_read = read(client_socket, buffer, CHUNKS_SIZE - 1);

        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            // In a real app, you'd parse the HTTP request here,
            // or loop the read() function to stream a massive file to disk.
            printf("\n--- Received clean request from Proxy ---\n%s\n", buffer);
        }

        // respond to the proxy (if any) - Nginx is waiting for standard HTTP
        // formatting to send back to the user.

        char *http_response = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 22\r\n"
                              "\r\n"
                              "Hello from the C code!";

        write(client_socket, http_response, strlen(http_response));

        // We processed the request. Close the socket to free up resources
        // and tell Nginx we are done.
        close(client_socket);
    }

    return 0; // never reached
}

// When you call read(), you are pulling 4,096 bytes out of the OS kernel's
// receive buffer. -> whhy CHUNKS_SIZE - 1 then?

// gcc http-server.c -o ./build/http-server && ../build/http-server
