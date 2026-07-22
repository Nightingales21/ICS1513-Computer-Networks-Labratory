#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 8192

// Helper function to convert timespec difference to milliseconds
double get_time_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + 
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

// Fixed and complete URL Parsing Function
void parseURL(char *url, char *hostname, int *port, char *path)
{
    strcpy(path, "/");
    *port = 8000; // Defaulting to port 8000 for your Python server

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

    // Extract port from hostname if ':' is present (e.g., "localhost:8000")
    char *colon = strchr(hostname, ':');
    if (colon != NULL) {
        *port = atoi(colon + 1);
        *colon = '\0'; // Truncate hostname to separate it from the port
    }
}

int main() 
{
    char url[512];
    char hostname[256];
    char path[256];
    int port;

    printf("Enter URL: ");
    if (scanf("%511s", url) != 1) {
        fprintf(stderr, "Invalid input.\n");
        return 1;
    }

    parseURL(url, hostname, &port, path);

    printf("\n--- Parsed Details ---\n");
    printf("Hostname : %s\n", hostname);
    printf("Port     : %d\n", port);
    printf("Path     : %s\n\n", path);

    struct timespec start_total, start_resp, end_resp, end_total;
    clock_gettime(CLOCK_MONOTONIC, &start_total);

    // 1. Resolve Hostname to IP Address
    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "Error: Could not resolve host %s\n", hostname);
        return 1;
    }

    // 2. Create Socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        return 1;
    }

    // 3. Setup Server Address
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // 4. Connect to Server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting to server");
        close(sockfd);
        return 1;
    }

    // 5. Build and Send HTTP GET Request
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Connection: close\r\n\r\n",
             path, hostname, port);

    clock_gettime(CLOCK_MONOTONIC, &start_resp);
    send(sockfd, request, strlen(request), 0);

    // Determine output file name
    char *filename = strrchr(path, '/');
    char out_filename[300];
    if (filename && strlen(filename) > 1) {
        snprintf(out_filename, sizeof(out_filename), "downloaded_%s", filename + 1);
    } else {
        strcpy(out_filename, "downloaded_index.html");
    }

    FILE *fp = fopen(out_filename, "wb");
    if (!fp) {
        perror("Error creating file");
        close(sockfd);
        return 1;
    }

    // 6. Receive Data & Separate Headers from Payload
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    size_t total_payload_bytes = 0;
    int headers_ended = 0;

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        if (!headers_ended) {
            // First byte returned -> measure response time (TTFB)
            clock_gettime(CLOCK_MONOTONIC, &end_resp);

            // Find the delimiter (\r\n\r\n) between headers and payload
            char *header_end_ptr = strstr(buffer, "\r\n\r\n");
            if (header_end_ptr) {
                headers_ended = 1;
                header_end_ptr += 4; // Skip past "\r\n\r\n"
                
                size_t header_len = header_end_ptr - buffer;
                size_t payload_len = bytes_received - header_len;

                if (payload_len > 0) {
                    fwrite(header_end_ptr, 1, payload_len, fp);
                    total_payload_bytes += payload_len;
                }
            }
        } else {
            // Write remaining raw binary payload
            fwrite(buffer, 1, bytes_received, fp);
            total_payload_bytes += bytes_received;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_total);
    fclose(fp);
    close(sockfd);

    // 7. Calculate Performance Metrics
    double response_time = get_time_ms(start_resp, end_resp);
    double total_time_ms = get_time_ms(start_total, end_total);
    double total_time_sec = total_time_ms / 1000.0;

    double throughput_kbps = (total_time_sec > 0) ? 
        ((double)total_payload_bytes / 1024.0) / total_time_sec : 0.0;

    // Output Performance Analysis
    printf("===========================================\n");
    printf("        PERFORMANCE METRICS SUMMARY        \n");
    printf("===========================================\n");
    printf(" Saved File Name  : %s\n", out_filename);
    printf(" Data Size Saved  : %zu bytes (%.2f KB)\n", total_payload_bytes, total_payload_bytes / 1024.0);
    printf(" Response Time    : %.3f ms\n", response_time);
    printf(" Total Download   : %.3f ms\n", total_time_ms);
    printf(" Throughput       : %.2f KB/s\n", throughput_kbps);
    printf("===========================================\n");

    return 0;
}