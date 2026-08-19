/*
 * dns_server.c
 * Simulated DNS server over UDP.
 *
 * - Holds a hardcoded hostname -> IP dictionary (the "DNS table").
 * - Waits for a domain name query from a client.
 * - Validates the query format.
 * - Looks the domain up and replies with:
 *      "OK:<ip>"          on success
 *      "ERR:NXDOMAIN"     if the domain isn't in the table
 *      "ERR:INVALID"      if the query is malformed
 * - Randomly drops a fraction of incoming requests (no reply sent at all)
 *   to simulate real-world UDP packet loss, so the client's timeout/retry
 *   logic actually gets exercised.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT            8080
#define BUF_SIZE        512
#define DROP_PERCENT    25   /* percentage chance of "losing" a request */

typedef struct {
    const char *domain;
    const char *ip;
} dns_record_t;

/* --- DNS Database (dictionary: hostname -> IP) --- */
static dns_record_t dns_table[] = {
    { "www.google.com",   "142.250.190.4"   },
    { "www.example.com",  "93.184.216.34"   },
    { "www.github.com",   "140.82.121.3"    },
    { "www.wikipedia.org","208.80.154.224"  },
    { "localhost",        "127.0.0.1"       },
};
#define DNS_TABLE_SIZE (sizeof(dns_table) / sizeof(dns_table[0]))

/* Basic sanity check on the query string:
 * - not empty, not too long
 * - only letters, digits, '.', '-' allowed
 * - must contain at least one '.' (very rough "looks like a domain" check)
 */
int is_valid_domain(const char *s, size_t len)
{
    if (len == 0 || len >= BUF_SIZE)
        return 0;

    int has_dot = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '.') {
            has_dot = 1;
            continue;
        }
        if (!isalnum((unsigned char)c) && c != '-')
            return 0;
    }
    return has_dot;
}

const char *lookup_domain(const char *domain)
{
    for (size_t i = 0; i < DNS_TABLE_SIZE; i++) {
        if (strcmp(dns_table[i].domain, domain) == 0)
            return dns_table[i].ip;
    }
    return NULL; /* NXDOMAIN */
}

int main(void)
{
    int sockfd;
    char buffer[BUF_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    srand((unsigned int)time(NULL));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Simulated DNS server listening on UDP port %d...\n", PORT);
    printf("Known domains:\n");
    for (size_t i = 0; i < DNS_TABLE_SIZE; i++)
        printf("  %-20s -> %s\n", dns_table[i].domain, dns_table[i].ip);
    printf("\n");

    while (1) {
        ssize_t recv_bytes = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                       (struct sockaddr *)&client_addr, &addr_len);
        if (recv_bytes < 0) {
            perror("Receive failed");
            continue;
        }
        buffer[recv_bytes] = '\0';

        printf("Query received: \"%s\" from %s:%d\n",
               buffer,
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        /* Simulate packet loss: silently drop this request sometimes */
        if ((rand() % 100) < DROP_PERCENT) {
            printf("  -> [simulated packet loss] no reply sent\n\n");
            continue;
        }

        char response[BUF_SIZE];

        if (!is_valid_domain(buffer, (size_t)recv_bytes)) {
            snprintf(response, sizeof(response), "ERR:INVALID");
            printf("  -> invalid format\n\n");
        } else {
            const char *ip = lookup_domain(buffer);
            if (ip == NULL) {
                snprintf(response, sizeof(response), "ERR:NXDOMAIN");
                printf("  -> NXDOMAIN\n\n");
            } else {
                snprintf(response, sizeof(response), "OK:%s", ip);
                printf("  -> resolved to %s\n\n", ip);
            }
        }

        sendto(sockfd, response, strlen(response), 0,
               (const struct sockaddr *)&client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}   