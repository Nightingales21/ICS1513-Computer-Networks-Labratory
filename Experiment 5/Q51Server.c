#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main()
{
    int sockfd;
    char buffer[1024];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr));

    printf("UDP Server is Listening...\n");

   ssize_t recv_bytes = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                              (struct sockaddr *)&client_addr, 
                              &addr_len);

    if (recv_bytes < 0) {
        perror("Receive failed");
    } else {
        buffer[recv_bytes] = '\0';
        printf("Server reply: %s\n", buffer);
    }

    
}