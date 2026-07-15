#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void search_student(const char *roll_no, char *response)
{
    FILE *file = fopen("students.csv", "r");
    char line[1024];
    int found = 0;

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = 0;
        char templine(1024);
        strcpy(templine, line);

        char *token = strtok(templine, ",");
        if (token && strcmp(token, roll_no) == 0)
        {
            char *name = strtok(NULL, ",");
            if (name)
            {
                snprintf(response, BUFFER_SIZE, "Roll Number: %s\nStudent Name: %s", token, name);
            } 
            else 
            {
                snprintf(response, BUFFER_SIZE, "Roll Number: %s\nStudent Name: Unknown", token);
            }
            found = 1;
            break;
        }
    }

    fclose(file);
}

int main()
{
    int sockfd;
    char buffer[1024];
    char sockaddr_in server_addr,
}