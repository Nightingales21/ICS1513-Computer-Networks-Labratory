#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MAXLEN 4096

/* Convert an ASCII string to a binary string (8 bits per char) */
static void text_to_binary(const char *text, char *binout) {
    binout[0] = '\0';
    for (size_t i = 0; i < strlen(text); i++) {
        char byte[9];
        unsigned char c = (unsigned char)text[i];
        for (int b = 7; b >= 0; b--) {
            byte[7 - b] = ((c >> b) & 1) ? '1' : '0';
        }
        byte[8] = '\0';
        strcat(binout, byte);
    }
}

/* Flips one bit in a binary string, for error-injection demo */
static void inject_error(char *binstr, int pos) {
    if (pos < 0 || pos >= (int)strlen(binstr)) return;
    binstr[pos] = (binstr[pos] == '0') ? '1' : '0';
}

/*
 * Splits binary data into 8-bit words, adds them using 1's complement
 * arithmetic with end-around carry, and produces the checksum
 * (1's complement of the final sum) as an 8-bit binary string.
 */
static void compute_checksum(const char *binary_data, char *checksum_out) {
    int len = strlen(binary_data);
    int nwords = (len + 7) / 8;

    char padded[MAXLEN];
    strcpy(padded, binary_data);
    while ((int)strlen(padded) % 8 != 0) strcat(padded, "0");

    printf("\n--- Checksum Computation (Sender) ---\n");
    printf("Total 8-bit words: %d\n", nwords);

    unsigned int sum = 0;
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
            printf("   Carry generated -> added back (end-around carry). Intermediate sum = %u\n", sum);
        } else {
            printf("   Intermediate sum = %u\n", sum);
        }
    }

    unsigned int checksum = (~sum) & 0xFF;

    char sum_bin[9], chk_bin[9];
    for (int b = 7; b >= 0; b--) sum_bin[7 - b] = ((sum >> b) & 1) ? '1' : '0';
    sum_bin[8] = '\0';
    for (int b = 7; b >= 0; b--) chk_bin[7 - b] = ((checksum >> b) & 1) ? '1' : '0';
    chk_bin[8] = '\0';

    printf("Final Sum (bin)  = %s (decimal %u)\n", sum_bin, sum);
    printf("Checksum (1's complement of sum) = %s (decimal %u)\n", chk_bin, checksum);

    strcpy(checksum_out, chk_bin);
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char server_ip[64];
    char input[MAXLEN];
    char binary_data[MAXLEN];

    printf("Enter server IP (e.g. 127.0.0.1): ");
    scanf("%63s", server_ip);
    getchar();

    printf("Enter data to send (text): ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';

    text_to_binary(input, binary_data);
    printf("\nText  : %s\n", input);
    printf("Binary: %s\n", binary_data);

    /* --- connect --- */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("invalid address"); exit(1);
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); exit(1);
    }
    printf("Connected to receiver.\n");

    char checksum_bin[16];
    compute_checksum(binary_data, checksum_bin);

    printf("\nInject a bit error before sending? (1=Yes, 0=No): ");
    int corrupt; scanf("%d", &corrupt);
    char to_send[MAXLEN];
    strcpy(to_send, binary_data);
    if (corrupt) {
        int pos;
        printf("Enter bit position to flip (0-%d): ", (int)strlen(to_send) - 1);
        scanf("%d", &pos);
        inject_error(to_send, pos);
        printf("Corrupted data: %s\n", to_send);
    }

    send(sock, to_send, strlen(to_send), 0);
    send(sock, checksum_bin, strlen(checksum_bin), 0);

    /* --- receive verdict from receiver --- */
    char reply[256];
    int n = read(sock, reply, sizeof(reply) - 1);
    reply[n] = '\0';
    printf("\nReceiver says: %s\n", reply);

    close(sock);
    return 0;
}