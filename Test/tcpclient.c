#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>


int main()
{
    int sfd;
    struct sockaddr_in server_addr;
    char buf[1024];

    sfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, 1127.0.0.1, &server_addr.sin_addr);

    send(sock_fd, "Hello Server", 12, 0);
    
    ssize_t n = recv(sock_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) { buf[n] = '\0'; printf("Server replied: %s\n", buf); }
    close(sock_fd);
    return 0;
}

