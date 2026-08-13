/*
 * dns_client.c
 * Simulated DNS client over UDP.
 *
 * - Runs in a loop, repeatedly prompting for a domain name to resolve.
 *   Type "exit" or "quit" to stop.
 * - Checks a small local cache (max 3 entries, FIFO eviction) first.
 * - If not cached, sends a UDP query to the DNS server.
 * - Uses a socket receive timeout; if no reply arrives, retries once.
 * - Handles server responses:
 *      "OK:<ip>"       -> success, cache it, print IP
 *      "ERR:NXDOMAIN"  -> domain not found
 *      "ERR:INVALID"   -> malformed query
 * - Also validates the domain format locally before sending, so obviously
 *   bad input never even goes over the network.
 *
 * Because the process stays alive across lookups, the cache persists
 * between queries within the same run -- so looking up the same domain
 * twice in a row will show a cache hit the second time.
 *
 * Usage:
 *   ./dns_client
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define SERVER_IP       "127.0.0.1"
#define SERVER_PORT     8080
#define BUF_SIZE        512
#define TIMEOUT_SEC     2
#define CACHE_SIZE      3

typedef struct {
    char domain[256];
    char ip[64];
    int  used; /* 0 = empty slot */
} cache_entry_t;

/* Simple FIFO cache: index 0 is oldest, we shift left on insert when full */
static cache_entry_t cache[CACHE_SIZE];

const char *cache_lookup(const char *domain)
{
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].used && strcmp(cache[i].domain, domain) == 0)
            return cache[i].ip;
    }
    return NULL;
}

void cache_insert(const char *domain, const char *ip)
{
    /* If already present, just refresh it in place */
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].used && strcmp(cache[i].domain, domain) == 0) {
            strncpy(cache[i].ip, ip, sizeof(cache[i].ip) - 1);
            return;
        }
    }

    /* Find an empty slot first */
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (!cache[i].used) {
            cache[i].used = 1;
            strncpy(cache[i].domain, domain, sizeof(cache[i].domain) - 1);
            strncpy(cache[i].ip, ip, sizeof(cache[i].ip) - 1);
            return;
        }
    }

    /* Cache full: evict the oldest (slot 0), shift everything left,
       insert new entry at the end (slot CACHE_SIZE - 1). */
    for (int i = 0; i < CACHE_SIZE - 1; i++)
        cache[i] = cache[i + 1];

    cache[CACHE_SIZE - 1].used = 1;
    strncpy(cache[CACHE_SIZE - 1].domain, domain, sizeof(cache[CACHE_SIZE - 1].domain) - 1);
    strncpy(cache[CACHE_SIZE - 1].ip, ip, sizeof(cache[CACHE_SIZE - 1].ip) - 1);
}

void print_cache_state(void)
{
    printf("[cache state: ");
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (cache[i].used)
            printf("%s->%s ", cache[i].domain, cache[i].ip);
        else
            printf("(empty) ");
    }
    printf("]\n");
}

int is_valid_domain(const char *s)
{
    size_t len = strlen(s);
    if (len == 0 || len >= 256)
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

/* Sends the query and waits for a reply with the socket's configured
 * timeout. Returns number of bytes received (>0) on success, 0 on
 * timeout, -1 on other error. */
ssize_t send_and_wait(int sockfd, const struct sockaddr_in *server_addr,
                       const char *domain, char *response, size_t resp_size)
{
    sendto(sockfd, domain, strlen(domain), 0,
           (const struct sockaddr *)server_addr, sizeof(*server_addr));

    ssize_t n = recvfrom(sockfd, response, resp_size - 1, 0, NULL, NULL);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0; /* timeout */
        return -1;    /* real error */
    }
    response[n] = '\0';
    return n;
}

int main(void)
{
    /* --- Set up UDP socket once, reused for every query --- */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    printf("DNS client started. Type a domain to resolve, or \"exit\" to quit.\n");

    char input[256];

    while (1) {
        printf("\nEnter domain name to resolve: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            /* EOF (e.g. Ctrl+D) -- treat as exit */
            printf("\n");
            break;
        }

        /* Strip trailing newline (and any trailing whitespace) */
        size_t len = strlen(input);
        while (len > 0 && (input[len - 1] == '\n' || input[len - 1] == '\r' || isspace((unsigned char)input[len - 1]))) {
            input[--len] = '\0';
        }7

        const char *domain = input;

        if (len == 0)
            continue; /* ignore blank lines, re-prompt */

        if (strcmp(domain, "exit") == 0 || strcmp(domain, "quit") == 0)
            break;

        /* --- Local format validation before touching the network --- */
        if (!is_valid_domain(domain)) {
            printf("Error: \"%s\" is not a validly formatted domain name.\n", domain);
            continue;
        }

        /* --- Check client-side cache first --- */
        const char *cached_ip = cache_lookup(domain);
        if (cached_ip != NULL) {
            printf("Cache hit: %s -> %s\n", domain, cached_ip);
            print_cache_state();
            continue;
        }
        printf("Cache miss for \"%s\", querying server...\n", domain);

        char response[BUF_SIZE];
        ssize_t result;
        int attempt;
        const int max_attempts = 2; /* 1 initial try + 1 retry */

        for (attempt = 1; attempt <= max_attempts; attempt++) {
            printf("Sending query (attempt %d of %d)...\n", attempt, max_attempts);
            result = send_and_wait(sockfd, &server_addr, domain, response, sizeof(response));

            if (result > 0)
                break; /* got a reply */

            if (result == 0)
                printf("  -> timed out waiting for response.\n");
            else
                perror("  -> receive error");

            if (attempt < max_attempts)
                printf("  -> retrying...\n");
        }

        if (result <= 0) {
            printf("Error: request timed out after %d attempt(s). Server unreachable or overloaded.\n",
                   max_attempts);
            continue;
        }

        /* --- Interpret the server's response --- */
        if (strncmp(response, "OK:", 3) == 0) {
            const char *ip = response + 3;
            printf("Resolved: %s -> %s\n", domain, ip);
            cache_insert(domain, ip);
            print_cache_state();
        } else if (strcmp(response, "ERR:NXDOMAIN") == 0) {
            printf("Error: NXDOMAIN - \"%s\" does not exist.\n", domain);
        } else if (strcmp(response, "ERR:INVALID") == 0) {
            printf("Error: server rejected the query as invalid format.\n");
        } else {
            printf("Error: unrecognized response from server: \"%s\"\n", response);
        }
    }

    close(sockfd);
    printf("Client exiting.\n");
    return EXIT_SUCCESS;
}