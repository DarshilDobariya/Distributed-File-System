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
#define PORT 8080
#define BUFSIZE 1024

// Function prototypes
void prcclient(int client_sock);
void handle_ufile(int client_sock, char *command);
void handle_dfile(int client_sock, char *command);
void handle_rmfile(int client_sock, char *command);
void handle_dtar(int client_sock, char *command);
void handle_display(int client_sock, char *command);
void send_file_to_server(int server_sock, char *filename, char *destination_path);
void receive_and_save_file(int sock, char *destination_path, char *f_name);

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

    printf("Smain server is listening on port %d\n", PORT);

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
            prcclient(client_sock);  // Handle communication with the client
            close(client_sock);  // Close the client socket
            exit(0);  // Exit the child process
        } else if (child_pid > 0) {
            // In the parent process
            close(client_sock);  // Close the client socket in the parent
            waitpid(-1, NULL, WNOHANG);  // Wait for child processes to terminate
        } else {
            perror("Fork failed");
        }
    }

    close(server_sock);  // Close the server socket
    return 0;
}

// Function to handle communication with a connected client
void prcclient(int client_sock) {
    char buffer[BUFSIZE];
    int bytes_read;

    // Read messages from the client
    while ((bytes_read = recv(client_sock, buffer, BUFSIZE, 0)) > 0) {
        buffer[bytes_read] = '\0';  // Null-terminate the received string
        printf("Received command: %s\n", buffer);  // Print the received command

        // Determine which command to process
        if (strncmp(buffer, "ufile", 5) == 0) {
            handle_ufile(client_sock, buffer);
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
    }
}

// Function to handle 'ufile' command
void handle_ufile(int client_sock, char *command) {
    char filename[256], destination_path[256];
    int server_sock;
    struct sockaddr_in server_addr;

    // Extract filename and destination path from the command
    sscanf(command, "ufile %s %s", filename, destination_path);

    if (strstr(filename, ".pdf") != NULL) {
        // Forward to Spdf server
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            perror("Spdf socket creation failed");
            return;
        }
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8081);  // Port for Spdf
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connect to Spdf failed");
            close(server_sock);
            return;
        }
        printf("destination_path:%s\n\n",destination_path);
        send_file_to_server(server_sock, filename, destination_path);
        close(server_sock);

    } else if (strstr(filename, ".txt") != NULL) {
        // Forward to Stext server
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            perror("Stext socket creation failed");
            return;
        }
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8082);  // Port for Stext
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connect to Stext failed");
            close(server_sock);
            return;
        }
        send_file_to_server(server_sock, filename, destination_path);
        close(server_sock);

    } else if (strstr(filename, ".c") != NULL) {
        // Store locally
        receive_and_save_file(client_sock, destination_path, filename);
    } else {
        printf("Unsupported file type: %s\n", filename);
    }
}

// Function to handle 'dfile' command
void handle_dfile(int client_sock, char *command) {
    char filename[256];
    sscanf(command, "dfile %s", filename);
    printf("Downloading file: %s\n", filename);
    // Implement logic to download file
}

// Function to handle 'rmfile' command
void handle_rmfile(int client_sock, char *command) {
    char filename[256];
    sscanf(command, "rmfile %s", filename);
    printf("Removing file: %s\n", filename);
    // Implement logic to remove file
}

// Function to handle 'dtar' command
void handle_dtar(int client_sock, char *command) {
    char filename[256];
    sscanf(command, "dtar %s", filename);
    printf("Creating tarball: %s\n", filename);
    // Implement logic to create tarball
}

// Function to handle 'display' command
void handle_display(int client_sock, char *command) {
    printf("Displaying files\n");
    // Implement logic to display files
}

// Function to send a file to a specified server
void send_file_to_server(int server_sock, char *filename, char *destination_path) {
    char buffer[BUFSIZE];
    ssize_t bytes_sent,bytes_read;
    int file_fd;

    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    
    // new file path creation to replace ~
    char full_path[BUFSIZE];
    if (destination_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, destination_path + 1);
    } else {
        strncpy(full_path, destination_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
    
    printf("spdf. full path: %s\n",full_path);

    // Construct the full path for the file(FilePath + file name    )
    char final_path[512];
    snprintf(final_path, sizeof(final_path), "%s/%s", full_path, filename);
    printf("spdf. final path: %s\n",final_path);


    // Send destination path
    bytes_sent = send(server_sock, final_path, strlen(final_path) + 1, 0);
    if (bytes_sent < 0) {
        perror("Send destination path failed");
        return;
    }

    // Open the file
    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("File open failed");
        return;
    }

    // Read and send the file content
    while ((bytes_read = read(file_fd, buffer, BUFSIZE)) > 0) {
        send(server_sock, buffer, bytes_read, 0);
    }

    close(file_fd);
}

// Function to receive a file from a client and save it to the specified destination
void receive_and_save_file(int sock, char *destination_path, char *f_name) {
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_received;
    char dir_path[256];


    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    // new file path creation to replace ~
    char full_path[BUFSIZE];
    if (destination_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, destination_path + 1);
    } else {
        strncpy(full_path, destination_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    // Copy the destination path to dir_path
    strncpy(dir_path, destination_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    // Ensure the destination directory exists
    char command[BUFSIZE];
    snprintf(command, sizeof(command), "mkdir -p %s", dir_path);
    system(command);

    // Construct the full path for the file(path + file name)
    char final_path[512];
    snprintf(final_path, sizeof(final_path), "%s/%s", full_path, f_name);

    printf("Final path: %s\n", final_path);

    // Create the file
    file_fd = open(final_path, O_CREAT | O_RDWR | O_TRUNC, 0777);
    if (file_fd < 0) {
        perror("File creation failed");
        return;
    }

    // Receive and write the file content
    while ((bytes_received = recv(sock, buffer, BUFSIZE, 0)) > 0) {
        write(file_fd, buffer, bytes_received);
    }

    close(file_fd);
}
