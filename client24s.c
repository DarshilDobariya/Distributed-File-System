#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// Define constants for the port number and buffer size
#define PORT 8080
#define BUFSIZE 1024

// Function to check if the file has a valid extension
int is_valid_extension(const char *filename) {
    // Find the last occurrence of a dot in the filename
    const char *ext = strrchr(filename, '.');
    // If there is no dot, the file has no extension
    if (ext == NULL) {
        return 0;
    }
    // Check if the extension is .c, .pdf, or .txt
    return strcmp(ext, ".c") == 0 || strcmp(ext, ".pdf") == 0 || strcmp(ext, ".txt") == 0;
}

// Function to send a file to the server
void send_file(int sock, char *filename, char *destination_path) {
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_read;

    // Send the command with the filename and destination path to the server
    snprintf(buffer, sizeof(buffer), "ufile %s %s", filename, destination_path);
    send(sock, buffer, strlen(buffer) + 1, 0);

    // Open the file
    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("File open failed");
        return;
    }

    // Read the file and send its content to the server
    while ((bytes_read = read(file_fd, buffer, BUFSIZE)) > 0) {
        send(sock, buffer, bytes_read, 0);
    }

    close(file_fd);  // Close the file
}

int main() {
    int client_sock;
    struct sockaddr_in server_addr;
    char buffer[BUFSIZE];
    char command[256], filename[256], destination_path[256];

    // Create a socket for the client
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    // Set the server IP address to localhost
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // Zero out the rest of the structure
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    // Connect to the server
    if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server\n");

    // Infinite loop to keep the client running
    while (1) {
        // Get the command from the user
        printf("client24s$ ");
        // Read the user's input
        fgets(buffer, sizeof(buffer), stdin);
        // Parse the input into command, filename, and destination path
        sscanf(buffer, "%s %s %s", command, filename, destination_path);

        // Validate file extension
        if (!is_valid_extension(filename)) {
            printf("Error: Invalid file extension.\n");
            continue;
        }

        // If the command is 'ufile'
        if (strcmp(command, "ufile") == 0) {
            // Check if the file exists
            if (access(filename, F_OK) != -1) {
                // Check if the destination path starts with '~/smain/'
                if (strncmp(destination_path, "~/smain/", 8) != 0) {
                    printf("Error: Destination path must start with '~/smain/'\n");
                    continue;
                }
                // Send the file to the server
                send_file(client_sock, filename, destination_path);
            } else {
                printf("File does not exist\n");
            }
        } else {
            // Print an error message for an invalid command
            printf("Invalid command\n");
        }
    }

    close(client_sock);  // Close the client socket
    return 0;
}


