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
#include <sys/wait.h> // Include for waitpid

// Define constants for the port number and buffer size
#define PORT 8081
#define BUFSIZE 1024

// Function prototypes
void handle_client(int client_sock);
void receive_and_save_file(int sock, char *file_path);
char* create_pdf_path(const char *destination_path);
int delete_file(const char *file_path);
void handle_ufile(int client_sock, char *command, char *file_data);
void handle_dfile(int client_sock, char *command);
void handle_rmfile(int client_sock, char *command);
void handle_dtar(int client_sock, char *command);
void handle_display(int client_sock, char *command);

void handle_client(int client_sock) {
    char buffer[BUFSIZE];
    ssize_t bytes_received;
    char *command;
    char *file_data;

    // Receive the combined message from the client
    bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("Combined message received: %s\n", buffer);

        // Determine which command to process
        if (strncmp(buffer, "ufile", 5) == 0) {
            // Find the newline that separates the command/path from the file data
            char *delimiter = strstr(buffer, "\n");
            if (delimiter == NULL) {
                printf("Invalid message format\n");
                return;
            }
            // Null-terminate the command
            *delimiter = '\0';
            // Extract the file data
            file_data = delimiter + 1;
            handle_ufile(client_sock, buffer, file_data);
        } else if (strncmp(buffer, "dfile", 5) == 0) {
            handle_dfile(client_sock, buffer);
        } else if (strncmp(buffer, "rmfile", 6) == 0) {
            handle_rmfile(client_sock, buffer);
        } else if (strncmp(buffer, "dtar", 4) == 0) {
            handle_dtar(client_sock, buffer);
        } else if (strncmp(buffer, "display", 7) == 0) {
            handle_display(client_sock, buffer);
        } else {
            printf("Unknown command: %s\n", buffer);
        }
    } else {
        if (bytes_received == 0) {
            printf("Connection closed by peer\n");
        } else {
            perror("Receive command failed");
        }
    }
    close(client_sock);
}

void handle_ufile(int client_sock, char *command, char *file_data) {
    char destination_path[1024];
    int file_fd;

    // Extract the destination path from the command
    int parsed = sscanf(command, "ufile %s", destination_path);

    if (parsed < 1) {
        printf("Command parsing failed\n");
        send(client_sock, "File upload failed", 18, 0);
        return;
    }

    

    char *new_file_path = create_pdf_path(destination_path);
    if (new_file_path != NULL) {
        // Ensure the destination directory exists
        char *last_slash = strrchr(new_file_path, '/');
        if (last_slash != NULL) {   
            // Temporarily remove the last part of the path
            *last_slash = '\0';
            char command_buf[BUFSIZE];
            // Create the directory if it does not exist
            snprintf(command_buf, sizeof(command_buf), "mkdir -p %s", new_file_path);
            if (system(command_buf) != 0) {
                perror("Directory creation failed");
                send(client_sock, "File upload failed", 18, 0);
                free(new_file_path);
                return;
            }
            // Restore the original path
            *last_slash = '/';  
        }

        // Create the file for writing
        file_fd = open(new_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd < 0) {
            perror("File creation failed");
            send(client_sock, "File upload failed", 18, 0);
            close(file_fd);
            free(new_file_path);
            return;
        }

        // Write the file data to the file
        ssize_t file_data_len = strlen(file_data);
        if (write(file_fd, file_data, file_data_len) < 0) {
            perror("File write failed");
            send(client_sock, "File upload failed", 18, 0);
            close(file_fd);
            free(new_file_path);
        }

        close(file_fd);

        // Send confirmation to the client
        const char *success_message = "File Uploaded successfully.";
        send(client_sock, success_message, strlen(success_message), 0);

        free(new_file_path);
    }else{
        const char *failed_message = "File uploading failed!";
        send(client_sock, failed_message, strlen(failed_message), 0);
    }
}


// Placeholder functions for other commands
void handle_dfile(int client_sock, char *command) {
    char file_path[1024];
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_read;

    // Ensure command string is properly null-terminated
    command[strcspn(command, "\r\n")] = '\0';

    // Extract the file path from the command
    if (sscanf(command, "dfile %s", file_path) != 1) {
        printf("Command parsing failed\n");
        return;
    }

    // Debug prints to ensure correct extraction
    printf("File requested for download: %s\n", file_path);

    // Open the requested file
    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        perror("File open failed");
        return;
    }

    // Send the file content to the client
    while ((bytes_read = read(file_fd, buffer, BUFSIZE)) > 0) {
        if (send(client_sock, buffer, bytes_read, 0) < 0) {
            perror("Send failed");
            close(file_fd);
            return;
        }
    }

    // Check if read failed
    if (bytes_read < 0) {
        perror("File read failed");
    }

    close(file_fd);
}

void handle_rmfile(int client_sock, char *command) {
    // Placeholder for 'rmfile' command
    printf("Handling 'rmfile' command: %s\n", command);

    char file_path[1024];
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_read;

    // Ensure command string is properly null-terminated
    command[strcspn(command, "\r\n")] = '\0';

    // Extract the file path from the command
    if (sscanf(command, "rmfile %s", file_path) != 1) {
        printf("Command parsing failed\n");
        return;
    }
    char *new_file_path = create_pdf_path(file_path);
    if(new_file_path != NULL){
        // check if file exist or not
        if (access(new_file_path, F_OK) == -1) {
            // Send rejction to the client
            const char *success_message = "File not found!";
            send(client_sock, success_message, strlen(success_message), 0);
            return;
        }
        
        // if exist then delete
        if (delete_file(new_file_path) != 0) {
            // Send rejction to the client
            const char *success_message = "File remove Failed!";
            send(client_sock, success_message, strlen(success_message), 0);

        }else{
            // Send rejction to the client
            const char *success_message = "File has been removed.";
            send(client_sock, success_message, strlen(success_message), 0);
        }
    }else{
        // Send rejction to the client
        const char *success_message = "File remove Failed!";
        send(client_sock, success_message, strlen(success_message), 0);
    }
}

void handle_dtar(int client_sock, char *command) {
    // Placeholder for 'dtar' command
    printf("Handling 'dtar' command: %s\n", command);
}

void handle_display(int client_sock, char *command) {
    // Placeholder for 'display' command
    printf("Handling 'display' command: %s\n", command);
}


// Function to delete a file and handle errors
int delete_file(const char *file_path) {
    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }

    // new file path creation to replace ~
    char full_path[BUFSIZE];
    if (file_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, file_path + 1);
    } else {
        strncpy(full_path, file_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    // change smain to spdf
    char *pdf_path = create_pdf_path(full_path);
    if (pdf_path != NULL) {
        if (unlink(pdf_path) == 0) {
            printf("Successfully deleted file: %s\n", full_path);
            return 0;
        } else {
            // Handle error
            switch (errno) {
                case EACCES:
                    fprintf(stderr, "Permission denied: %s\n", full_path);
                    break;
                case ENOENT:
                    fprintf(stderr, "File does not exist: %s\n", full_path);
                    break;
                case EISDIR:
                    fprintf(stderr, "Path is a directory, not a file: %s\n", full_path);
                    break;
                default:
                    fprintf(stderr, "Failed to delete file %s: %s\n", full_path, strerror(errno));
            }
            return -1;
        }
        free(pdf_path); // Remember to free the allocated memory
    }
}

char* create_pdf_path(const char *destination_path) {
    char *pos;
    size_t new_path_size = strlen(destination_path); 
    char *new_path = malloc(new_path_size);

    if (new_path == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    // Find the position of "smain" in the original path
    pos = strstr(destination_path, "smain");
    if (pos != NULL) {
        // Copy the part before "smain" into new_path
        size_t prefix_len = pos - destination_path;
        strncpy(new_path, destination_path, prefix_len);
        new_path[prefix_len] = '\0'; // Null-terminate the string

        // Append "spdf" to new_path
        strcat(new_path, "spdf");

        // Append the rest of the original path after "smain"
        strcat(new_path, pos + strlen("smain"));
    } else {
        // If "smain" not found, use destination_path as new_path
        strcpy(new_path, destination_path);
    }

    return new_path;
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

    printf("Spdf server is listening on port %d\n", PORT);

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
            close(server_sock);  // Close the server socket in the child
            handle_client(client_sock);  // Handle communication with the client
                       close(client_sock);  // Close the client socket in the child
            exit(0);  // Exit the child process
        } else if (child_pid > 0) {
            // In the parent process
            close(client_sock);  // Close the client socket in the parent
            // Wait for terminated child processes to avoid zombie processes
            while (waitpid(-1, NULL, WNOHANG) > 0) {
                // Continue to reap any terminated child processes
            }
        } else {
            perror("Fork failed");
        }
    }

    close(server_sock);  // Close the server socket
    return 0;
}

