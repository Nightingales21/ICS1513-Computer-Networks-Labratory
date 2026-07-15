#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char msg[1024];
    char response[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    printf("Enter a roll number: ");
    if (fgets(msg, sizeof(msg), stdin) == NULL) {
        close(sock);
        return 0;
    }
    msg[strcspn(msg, "\n")] = 0;

    if (write(sock, msg, strlen(msg)) < 0) {
        perror("Failed to send request");
        close(sock);
        return 1;
    }

    ssize_t bytes_read = read(sock, response, sizeof(response) - 1);
    if (bytes_read > 0) {
        response[bytes_read] = '\0';
        printf("\nServer response:\n%s\n", response);
    } else {
        perror("Failed to receive response");
    }

    close(sock);
    return 0;
}

