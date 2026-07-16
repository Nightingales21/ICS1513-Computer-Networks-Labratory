#include <stdio.h>
#include <string.h>
#include <netdb.h>

void parseURL(char *url, char *hostname, char *path)
{
    strcpy(path, "/");

    char *url_ptr = url;
    if (strncmp(url_ptr, "http://", 7) == 0)
    {
        url_ptr += 7;
    }
    else if (strncmp(url_ptr, "https://", 8) == 0)
    {
        url_ptr += 8;
    }

    char *slash = strchr(url_ptr, '/');
    int host_len = slash - url_ptr;

    strncpy(hostname, url_ptr, host_len);
    hostname[host_len] = '\0';
    strcpy(path, slash);
}

int main()
{

    char url[512];
    char hostname[256];
    char path[256];


    printf("Enter URL: ");
    scanf("%s", &url);

    parseURL(url, hostname, path);

    printf("%s\n", hostname);
    printf("%s", path);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET


}