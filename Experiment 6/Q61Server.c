#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAXLEN 4096

/*
 * Adds all 8-bit words of data INCLUDING the received checksum word
 * using 1's complement addition with end-around carry.
 * If the final sum is all 1s (0xFF), the data is error-free.
 */
static int verify_checksum(const char *binary_data, const char *received_checksum_bin) {
    char padded[MAXLEN];
    strcpy(padded, binary_data);
    while ((int)strlen(padded) % 8 != 0) strcat(padded, "0");
    strcat(padded, received_checksum_bin);

    int nwords = strlen(padded) / 8;
    unsigned int sum = 0;

    printf("\n--- Checksum Verification (Receiver) ---\n");
    for (int i = 0; i < nwords; i++) {
        char word[9];
        strncpy(word, padded + i * 8, 8);
        word[8] = '\0';
        unsigned int val = (unsigned int)strtol(word, NULL, 2);
        printf("Word %d: %s (decimal %u)\n", i + 1, word, val);

        sum += val;
        if (sum > 0xFF) {
            unsigned int carry = sum >> 8;
            sum = (sum & 0xFF) + carry;
            printf("   Carry generated -> end-around carry added. Intermediate sum = %u\n", sum);
        } else {
            printf("   Intermediate sum = %u\n", sum);
        }
    }

    printf("Final accumulated sum = %u (0x%02X)\n", sum, sum);
    return (sum == 0xFF);   /* all 1s -> no error */
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

    printf("Checksum Receiver listening on port %d...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
    if (client_fd < 0) { perror("accept"); exit(1); }
    printf("Sender connected.\n");

    /* 1. Receive binary data */
    char binary_data[MAXLEN];
    int n = read(client_fd, binary_data, sizeof(binary_data) - 1);
    binary_data[n] = '\0';

    /* 2. Receive checksum (8-bit binary string) */
    char checksum_bin[16];
    n = read(client_fd, checksum_bin, sizeof(checksum_bin) - 1);
    checksum_bin[n] = '\0';

    printf("\nReceived binary data : %s\n", binary_data);
    printf("Received checksum    : %s\n", checksum_bin);

    int ok = verify_checksum(binary_data, checksum_bin);

    printf("\n================ RESULT ================\n");
    printf(ok ? "No error detected. Data is intact.\n"
               : "ERROR DETECTED! Data is corrupted.\n");
    printf("=========================================\n");

    char *reply = ok ? "ACK: No error detected"
                      : "NACK: Error detected in data";
    send(client_fd, reply, strlen(reply), 0);

    close(client_fd);
    close(server_fd);
    return 0;
}