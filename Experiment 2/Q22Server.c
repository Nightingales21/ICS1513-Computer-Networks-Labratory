#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define CSV_FILE "students.csv"

// Function to search for the student in the CSV file
void search_student(const char *roll_no, char *response) {
    FILE *file = fopen(CSV_FILE, "r");
    if (!file) {
        strcpy(response, "Error: Server database missing.");
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline character
        line[strcspn(line, "\n")] = 0;

        // Duplicate line to parse since strtok modifies the string
        char temp_line[BUFFER_SIZE];
        strcpy(temp_line, line);

        char *token = strtok(temp_line, ",");
        if (token && strcmp(token, roll_no) == 0) {
            // Roll number matched! Get the name (second column)
            char *name = strtok(NULL, ",");
            if (name) {
                snprintf(response, BUFFER_SIZE, "Roll Number: %s\nStudent Name: %s", token, name);
            } else {
                snprintf(response, BUFFER_SIZE, "Roll Number: %s\nStudent Name: Unknown", token);
            }
            found = 1;
            break;
        }
    }

    fclose(file);

    if (!found) {
        strcpy(response, "Student Record Not Found");
    }
}

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // 1. Create UDP socket (SOCK_DGRAM)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 2. Bind the socket to the port
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Server is running on port %d...\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);

        // 3. Receive Roll Number from client
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) continue;
        
        buffer[n] = '\0';
        printf("Received request for Roll Number: %s\n", buffer);

        // 4. Search and prepare response
        search_student(buffer, response);

        // 5. Send response back to the client
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&client_addr, addr_len);
        printf("Response sent back to client.\n\n");
    }

    close(sockfd);
    return 0;
}