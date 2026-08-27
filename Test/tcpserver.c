#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

int main()
{
    int lfd, cfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buf[1024];

    lfd = socket(AF_INET,SOCK_STREAM ,0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    printf("Listening");

    while(1) 
    {
        cfd = accept(lfd, (struct sockaddr *)&client_addr, &addr_len);
        printf("Connected");

        ssize_t n = recv(cfd, buf, sizeof(buf)-1, 0);
        if (n > 0)
        {
            buf[n] = '\0';
            send(cfd, "ACK", 3, 0);   
        }

        close(cfd);
    }

    close(lfd);
    return 0;
}