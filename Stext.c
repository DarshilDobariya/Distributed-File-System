#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

// Define constants for the port number and buffer size
#define PORT 8082
#define BUFSIZE 1024


void receive_and_save_file(int sock, char *destination_path);

// Function to handle communication with a connected client
void handle_client(int client_sock) {
    // Variables to store paths and filenames
    char destination_path[1024];
    char new_path[1024];
    char filename[256];
    ssize_t bytes_received;
    char *pos;

    // Receive destination path
    bytes_received = recv(client_sock, destination_path, sizeof(destination_path)-1, 0);
    if (bytes_received > 0) {
        // Null-terminate the received destination path
        destination_path[bytes_received] = '\0';
        printf("Destination path received: %s\n", destination_path);
    } else {
        // Handle the case where no data is received
        if (bytes_received == 0) {
            printf("Connection closed by peer\n");
        } else {
            perror("Receive destination path failed");
        }
        close(client_sock);
        return;
    }

    // Find the position of "smain" in the original path
    pos = strstr(destination_path, "smain");
    if (pos != NULL) {
        // Copy the part before "smain" into new_path
        strncpy(new_path, destination_path, pos - destination_path);
        // Null-terminate the string
        new_path[pos - destination_path] = '\0'; 

        // Append "stxt" to new_path
        strcat(new_path, "stxt");

        // Append the rest of the original path after "smain"
        strcat(new_path, pos + strlen("smain"));
    }
    
    printf("new path : %s\n", new_path);

    // Ensure the destination directory exists
    char *last_slash = strrchr(new_path, '/');
    if (last_slash != NULL) {
        // Temporarily remove the last part of the path   
        *last_slash = '\0';
        char command[BUFSIZE];
        // Create the directory if it does not exist
        snprintf(command, sizeof(command), "mkdir -p %s", new_path);
        system(command);
        // Restore the original path
        *last_slash = '/';
    }

    // Save the file
    receive_and_save_file(client_sock, new_path);
    // Close the client socket
    close(client_sock);
}

// Function to receive a file from the client and save it to the specified path
void receive_and_save_file(int sock, char *file_path) {
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_received;

    // Create the file for writing
    file_fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
        perror("File creation failed");
        return;
    }

    // Receive and write the file content
    while ((bytes_received = recv(sock, buffer, BUFSIZE, 0)) > 0) {
        write(file_fd, buffer, bytes_received);
    }

    // close the file
    close(file_fd);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pid_t child_pid;

    // Create a socket for the server
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    // Bind the socket to the specified port
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Stext server is listening on port %d\n", PORT);

    while (1) {
        // Accept a client connection
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Fork a child process to handle the client
        child_pid = fork();
        if (child_pid == 0) {
            // In the child process
            // Close the server socket in the child
            close(server_sock);  
            // Handle communication with the client
            handle_client(client_sock);  
            // Close the client socket
            close(client_sock);  
            // Exit the child process
            exit(0);  
        } else if (child_pid > 0) {
            // In the parent process
            // Close the client socket in the parent
            close(client_sock);  
            // Wait for child processes to terminate
            waitpid(-1, NULL, WNOHANG);  
        } else {
            perror("Fork failed");
        }
    }

    // Close the server socket
    close(server_sock);  
    return 0;
}
