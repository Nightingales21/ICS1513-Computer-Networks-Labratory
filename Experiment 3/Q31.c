#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

void parseURL(char *url, char *hostname, char *path)
{
    strcpy(path, "/");

    char *url_ptr = url;
    if (strncmp(url_ptr, "http://", 7) == 0)
    {
        url_ptr += 7;
    }
    else if (strncmp(url_ptr, "https://", 8) == 0)
    {
        url_ptr += 8;
    }

    char *slash = strchr(url_ptr, '/');
    if (slash != NULL) {
        int host_len = slash - url_ptr;
        strncpy(hostname, url_ptr, host_len);
        hostname[host_len] = '\0';
        strcpy(path, slash);
    } else {
        strcpy(hostname, url_ptr);
    }
}

int main()
{
    char url[512];
    char hostname[256];
    char path[256];

    printf("Enter URL: ");
    scanf("%511s", url); 

    parseURL(url, hostname, path);

    printf("Hostname: %s\n", hostname);
    printf("Path: %s\n\n", path);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, "80", &hints, &res);
    if (status != 0) {
        fprintf(stderr, "DNS Error: %s\n", gai_strerror(status));
        return 1;
    }
    
    int client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket < 0) {
        perror("Socket creation failed");
        freeaddrinfo(res);
        return 1;
    }

    if (connect(client_socket, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Connection failed");
        close(client_socket);
        freeaddrinfo(res);
        return 1;
    }

    freeaddrinfo(res);

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: Raw-C-Client\r\n"
             "Connection: close\r\n\r\n",
             path, hostname);

    if (send(client_socket, request, strlen(request), 0) < 0) {
        perror("Failed to send HTTP request");
        close(client_socket);
        return 1;
    }

    FILE *output_file = fopen("dc.txt", "wb");
    if (!output_file) {
        perror("Failed to open output file");
        close(client_socket);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    int header_parsed = 0;

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0'; 
        if (!header_parsed) {
            char *body_start = strstr(buffer, "\r\n\r\n");
            if (body_start != NULL) {
                body_start += 4; 
                
                int header_len = body_start - buffer;
                printf("%.*s", header_len, buffer);
                printf("\nBody Content Saved\n");

                size_t body_bytes = bytes_received - header_len;
                if (body_bytes > 0) {
                    fwrite(body_start, 1, body_bytes, output_file);
                }
                header_parsed = 1;
            } else {
                printf("%s", buffer);
            }
        } else {
            fwrite(buffer, 1, bytes_received, output_file);
        }
    }

    if (bytes_received < 0) {
        perror("Error reading from socket");
    }

    fclose(output_file);
    close(client_socket);

    return 0;
}