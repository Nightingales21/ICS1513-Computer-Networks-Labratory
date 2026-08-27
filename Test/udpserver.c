
int main() 
{
    int sfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buf[1024];

    sfd = socket(AF_INET, SOCK_DGRAM, 0); 

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

printf("UDP server waiting ");
while (1) {
ssize_t n = recvfrom(sfd, buf, sizeof(buf) - 1, 0,(struct sockaddr*)&client_addr, &addr_len);
    if (n > 0) {
    buf[n] = '\0';
    printf("Received: %s from %s:%d\n", buf,
    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    sendto(sfd, "ACK", 3, 0,
    (struct sockaddr*)&client_addr, addr_len);
}
}
close(sfd);
return 0;
}