#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    char roll_no[BUFFER_SIZE];
    struct sockaddr_in server_addr;

    // 1. Create UDP socket (SOCK_DGRAM)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Localhost

    // 2. User Input
    printf("Enter Roll Number to search: ");
    if (fgets(roll_no, sizeof(roll_no), stdin) == NULL) {
        close(sockfd);
        return 0;
    }
    // Remove trailing newline character
    roll_no[strcspn(roll_no, "\n")] = 0;

    // 3. Send Roll Number to Server
    sendto(sockfd, roll_no, strlen(roll_no), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr));

    // 4. Receive Response from Server
    socklen_t addr_len = sizeof(server_addr);
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&server_addr, &addr_len);
    
    if (n >= 0) {
        buffer[n] = '\0';
        printf("\n--- Server Response ---\n%s\n-----------------------\n", buffer);
    } else {
        perror("Failed to receive data");
    }

    // 5. Terminate client program
    close(sockfd);
    return 0;
}