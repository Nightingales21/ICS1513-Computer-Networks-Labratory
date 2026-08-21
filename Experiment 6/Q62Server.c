#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAXLEN 4096

/* XOR two binary strings of equal length */
static void xor_bits(const char *a, const char *b, char *result, int len) {
    for (int i = 0; i < len; i++)
        result[i] = (a[i] == b[i]) ? '0' : '1';
    result[len] = '\0';
}

/* Modulo-2 division of dividend by generator; returns remainder */
static void mod2_divide(const char *dividend, const char *generator, char *remainder_out) {
    int dlen = strlen(dividend);
    int glen = strlen(generator);

    char *temp = (char *)malloc(dlen + 1);
    strcpy(temp, dividend);

    for (int i = 0; i <= dlen - glen; ) {
        if (temp[i] == '1') {
            char part[MAXLEN], xored[MAXLEN];
            strncpy(part, temp + i, glen);
            part[glen] = '\0';
            xor_bits(part, generator, xored, glen);
            memcpy(temp + i, xored, glen);
        }
        i++;
    }

    strncpy(remainder_out, temp + (dlen - (glen - 1)), glen - 1);
    remainder_out[glen - 1] = '\0';
    free(temp);
}

/* Divides received frame (data+CRC) by generator; remainder all-zero => no error */
static int verify_crc(const char *received_frame, const char *generator) {
    char remainder[MAXLEN];
    printf("\n--- CRC Verification (Receiver) ---\n");
    printf("Received frame (data+CRC): %s\n", received_frame);

    mod2_divide(received_frame, generator, remainder);
    printf("Remainder after division  : %s\n", remainder);

    for (size_t i = 0; i < strlen(remainder); i++)
        if (remainder[i] != '0') return 0;
    return 1;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, 1) < 0) { perror("listen"); exit(1); }

    printf("CRC Receiver listening on port %d...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
    if (client_fd < 0) { perror("accept"); exit(1); }
    printf("Sender connected.\n");

    /* 1. Receive generator */
    char generator[32];
    int n = read(client_fd, generator, sizeof(generator) - 1);
    generator[n] = '\0';

    /* 2. Receive frame (data + CRC bits), possibly corrupted */
    char frame[MAXLEN];
    n = read(client_fd, frame, sizeof(frame) - 1);
    frame[n] = '\0';

    printf("\nGenerator received : %s\n", generator);
    printf("Frame received      : %s\n", frame);

    int ok = verify_crc(frame, generator);

    printf("\n================ RESULT ================\n");
    printf(ok ? "Frame is ERROR-FREE.\n"
               : "Frame CONTAINS ERROR(S)!\n");
    printf("=========================================\n");

    char *reply = ok ? "ACK: Frame error-free"
                      : "NACK: Frame contains errors";
    send(client_fd, reply, strlen(reply), 0);

    close(client_fd);
    close(server_fd);
    return 0;
}