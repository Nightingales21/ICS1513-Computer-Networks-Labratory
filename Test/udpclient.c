int main() {
int sock_fd;
struct sockaddr_in server_addr;
socklen_t addr_len = sizeof(server_addr);
char buf[1024];
sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
if (sock_fd < 0) { perror("socket"); exit(1); }
memset(&server_addr, 0, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(PORT);
inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
sendto(sock_fd, "Hello UDP", 9, 0,
(struct sockaddr*)&server_addr, addr_len);
ssize_t n = recvfrom(sock_fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
if (n > 0) { buf[n] = '\0'; printf("Server replied: %s\n", buf); }
close(sock_fd);
return 0;
}