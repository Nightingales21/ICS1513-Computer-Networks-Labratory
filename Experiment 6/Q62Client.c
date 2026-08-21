#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define GENERATOR "10011"   /* G(x) = x^4 + x + 1 */
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

/* Computes CRC for given binary data using the generator; prints steps */
static void compute_crc(const char *binary_data, const char *generator, char *crc_out) {
    int glen = strlen(generator);
    char appended[MAXLEN];
    strcpy(appended, binary_data);
    for (int i = 0; i < glen - 1; i++) strcat(appended, "0");

    printf("\n--- CRC Computation (Sender) ---\n");
    printf("Data              : %s\n", binary_data);
    printf("Generator         : %s\n", generator);
    printf("Data + %d zero bits: %s\n", glen - 1, appended);

    mod2_divide(appended, generator, crc_out);
    printf("Computed CRC (remainder) = %s\n", crc_out);
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

    send(sock, GENERATOR, strlen(GENERATOR), 0);

    char crc[32];
    compute_crc(binary_data, GENERATOR, crc);

    char frame[MAXLEN];
    snprintf(frame, sizeof(frame), "%s%s", binary_data, crc);
    printf("Transmitted frame (data+CRC) = %s\n", frame);

    printf("\nInject a bit error before sending? (1=Yes, 0=No): ");
    int corrupt; scanf("%d", &corrupt);
    if (corrupt) {
        int pos;
        printf("Enter bit position to flip (0-%d): ", (int)strlen(frame) - 1);
        scanf("%d", &pos);
        inject_error(frame, pos);
        printf("Corrupted frame: %s\n", frame);
    }

    send(sock, frame, strlen(frame), 0);

    /* --- receive verdict from receiver --- */
    char reply[256];
    int n = read(sock, reply, sizeof(reply) - 1);
    reply[n] = '\0';
    printf("\nReceiver says: %s\n", reply);

    close(sock);
    return 0;
}