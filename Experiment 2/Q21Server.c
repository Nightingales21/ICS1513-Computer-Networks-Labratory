#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

struct sockaddr_in server_addr;

void *handle_client (int client_fd, char buffer[1024]) 
{
    read(client_fd, buffer, sizeof(buffer));
    
    FILE *file;
    char linebuf[1024];
    int lineno = 0;
    int found = 0;

    file = fopen("students.csv", "r");

    while (fgets(linebuf, sizeof(linebuf), file) != NULL)
    {
        lineno++;
        if (strstr(linebuf, buffer) != NULL)
        {
            printf("Match found at line %d: %s", lineno, linebuf); 
            found = 1;
        }

    }
     if (!found) {
        printf("The string '%s' was not found in the file.\n", buffer);
        
    }

    fclose(file);
    retun NULL;
}

int main()
{
    int server_fd, client_fd;
    char buffer[1024] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;

    server_addr.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    listen(server_fd, 5);

    printf("Server is waiting:");

    while(1)
    {
        int *client_fd = malloc(sizeof(int));
        client_fd = accept(server_fd, NULL, NULL);

        pthread_create(&thread_id, NULL, handle_client(client_fd, buffer), (void*)new_sock);
    
        pthread_detach(thread_id);
    }

}

    

    close(client_fd);
    close(server_fd);

    return 0;
}

