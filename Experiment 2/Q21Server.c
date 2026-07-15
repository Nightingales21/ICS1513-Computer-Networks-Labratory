#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h> 

void *handle_client(void *client_void_ptr) 
{
    // Retrieve the socket file descriptor and free the allocated memory wrapper
    int client_fd = *(int *)client_void_ptr;
    free(client_void_ptr);

    char buffer[1024] = {0};
    
    // Read data from the client safely
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return NULL;
    }

    // Strip trailing newlines or carriage returns that often come from network sockets
    buffer[strcspn(buffer, "\r\n")] = 0;
    
    FILE *file = fopen("students.csv", "r");
    if (file == NULL) {
        perror("Failed to open students.csv");
        close(client_fd);
        return NULL;
    }

    char linebuf[1024];
    int lineno = 0;
    int found = 0;

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
    close(client_fd); // Close the socket connection when done
    return NULL;
}

int main()
{
    int server_fd;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is waiting on port 8080...\n");

    while(1)
    {
        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror("Malloc failed");
            continue;
        }

        *new_sock = accept(server_fd, NULL, NULL);
        if (*new_sock < 0) {
            perror("Accept failed");
            free(new_sock);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) == 0) {
            pthread_detach(thread_id);
        } else {
            perror("Could not create thread");
            close(*new_sock);
            free(new_sock);
        }
    }

    close(server_fd);
    return 0;
}