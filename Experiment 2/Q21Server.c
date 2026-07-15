#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

void *handle_client(void *client_void_ptr)
{
    int client_fd = *(int *)client_void_ptr;
    free(client_void_ptr);

    char buffer[1024] = {0};
    char response[1024] = {0};

    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return NULL;
    }

    buffer[strcspn(buffer, "\r\n")] = 0;

    FILE *file = fopen("students.csv", "r");
    if (file == NULL) {
        perror("Failed to open students.csv");
        strcpy(response, "Error: students.csv not found");
        write(client_fd, response, strlen(response));
        close(client_fd);
        return NULL;
    }

    char linebuf[1024];
    int found = 0;

    while (fgets(linebuf, sizeof(linebuf), file) != NULL)
    {
        linebuf[strcspn(linebuf, "\r\n")] = 0;

        char temp_line[1024];
        strcpy(temp_line, linebuf);

        char *roll_no = strtok(temp_line, ",");
        if (roll_no && strcmp(roll_no, buffer) == 0)
        {
            char *name = strtok(NULL, ",");
            if (name) {
                snprintf(response, sizeof(response), "Roll Number: %s\nStudent Name: %s", roll_no, name);
            } else {
                snprintf(response, sizeof(response), "Roll Number: %s\nStudent Name: Unknown", roll_no);
            }
            found = 1;
            break;
        }
    }

    fclose(file);

    if (!found) {
        snprintf(response, sizeof(response), "Student Record Not Found for roll number %s", buffer);
    }

    if (write(client_fd, response, strlen(response)) < 0) {
        perror("Failed to send response");
    }

    close(client_fd);
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

    while (1)
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