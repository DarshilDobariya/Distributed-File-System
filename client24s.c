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

#define PORT 8080
#define BUFSIZE 1024
#define MAX_TOKENS 10
#define CMD_END_MARKER "END_CMD" // Marker to indicate the end of the command

// Function prototypes
int is_valid_extension(const char *filename);
void send_file(int sock, char *filename, char *destination_path);
void process_command(int sock, char *input);
void handle_ufile(int sock, char *tokens[]);
void handle_dfile(int sock, char *tokens[]);
void handle_rmfile(int sock, char *tokens[]);
void handle_dtar(int sock, char *tokens[]);
void handle_display(int sock, char *tokens[]);

int main() {
    int client_sock;
    struct sockaddr_in server_addr;
    char buffer[BUFSIZE];

    // Create a socket for the client
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
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
        printf("client24s$ ");
        // Read the user's input
        fgets(buffer, sizeof(buffer), stdin);
        // Process the user's input
        process_command(client_sock, buffer);
    }

    close(client_sock);
    return 0;
}

// Function to check if the file has a valid extension
int is_valid_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return 0;
    }
    return strcmp(ext, ".c") == 0 || strcmp(ext, ".pdf") == 0 || strcmp(ext, ".txt") == 0;
}

// Function to send a file to the server along with the command
void send_file(int sock, char *filename, char *destination_path) {
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_read;
    size_t total_size;

    // Open the file
    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        perror("File open failed");
        return;
    }

    // Determine the file size
    total_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    // Allocate memory for the entire message
    char *message = malloc(total_size + BUFSIZE + strlen(CMD_END_MARKER) + 1);
    if (message == NULL) {
        perror("Memory allocation failed");
        close(file_fd);
        return;
    }

    // Build the command string
    snprintf(message, total_size + BUFSIZE + strlen(CMD_END_MARKER) + 1, "ufile %s %s %s", filename, destination_path, CMD_END_MARKER);

    // Append the file content
    ssize_t message_len = strlen(message);
    while ((bytes_read = read(file_fd, buffer, BUFSIZE)) > 0) {
        memcpy(message + message_len, buffer, bytes_read);
        message_len += bytes_read;
    }

    // Send the entire message to the server
    send(sock, message, message_len, 0);

    // Clean up
    free(message);
    close(file_fd);
}

// Function to process the user's input
void process_command(int sock, char *input) {
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    // Tokenize the input
    char *token = strtok(input, " \n");
    while (token != NULL && token_count < MAX_TOKENS) {
        tokens[token_count++] = token;
        token = strtok(NULL, " \n");
    }

    if (token_count == 0) {
        printf("Invalid command\n");
        return;
    }

    // Determine command and handle accordingly
    if (strcmp(tokens[0], "ufile") == 0) {
        handle_ufile(sock, tokens);
    } else if (strcmp(tokens[0], "dfile") == 0) {
        handle_dfile(sock, tokens);
    } else if (strcmp(tokens[0], "rmfile") == 0) {
        handle_rmfile(sock, tokens);
    } else if (strcmp(tokens[0], "dtar") == 0) {
        handle_dtar(sock, tokens);
    } else if (strcmp(tokens[0], "display") == 0) {
        handle_display(sock, tokens);
    } else {
        printf("Invalid command\n");
    }

    
}

// Handle ufile command
void handle_ufile(int sock, char *tokens[]) {
    char buffer[BUFSIZE];

    if (!tokens[1] || !tokens[2]) {
        printf("Error: Missing filename or destination path.\n");
        return;
    }
    char *filename = tokens[1];
    char *destination_path = tokens[2];

    if (!is_valid_extension(filename)) {
        printf("Error: Invalid file extension.\n");
        return;
    }
    else if (access(filename, F_OK) == -1) {
        printf("Error: File does not exist.\n");
        return;
    }
    else if (strncmp(destination_path, "~/smain/", 8) != 0) {
        printf("Error: Destination path must start with '~/smain/'\n");
        return;
    }else{
        send_file(sock, filename, destination_path);

        // Receive and display the confirmation message
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Server: %s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Connection closed by server.\n");
            exit(EXIT_SUCCESS);
        } else {
            perror("Error receiving data");
        }
    }
}

// Handle dfile command
void handle_dfile(int sock, char *tokens[]) {
    if (!tokens[1]) {
        printf("Error: Missing filename for dfile.\n");
        return;
    }
    
    char *file_path = tokens[1];
    char command[BUFSIZE];
    snprintf(command, sizeof(command), "dfile %s", file_path);

    // Send the command to the server
    if (send(sock, command, strlen(command), 0) < 0) {
        perror("Send failed");
        return;
    }

    // Receive the file name
    char buff_name[BUFSIZE];
    ssize_t bytes_received = recv(sock, buff_name, sizeof(buff_name) - 1, 0);
    if (bytes_received < 0) {
        perror("Error receiving file name");
        return;
    }
    buff_name[bytes_received] = '\0';

    // Check if the response is an error message
    const char *error_prefix = "ERROR:";
    if (strncmp(buff_name, error_prefix, strlen(error_prefix)) == 0) {
        printf("Server: %s\n", buff_name);
        return;
    }

    // Create a file to save the received content
    FILE *fp = fopen(buff_name, "wb");
    if (!fp) {
        perror("Error opening file for writing");
        return;
    }

     // Receive the file content
    char buffer_content[BUFSIZE];
    size_t marker_len = strlen(CMD_END_MARKER);
    int download_successful = 0;

    while ((bytes_received = recv(sock, buffer_content, sizeof(buffer_content), 0)) > 0) {
        // Check for end marker
        if (bytes_received >= marker_len && 
            memcmp(buffer_content + bytes_received - marker_len, CMD_END_MARKER, marker_len) == 0) {
            // Write up to the end marker and break the loop
            fwrite(buffer_content, 1, bytes_received - marker_len, fp);
            download_successful = 1;
            break;
        }
        
        size_t written = fwrite(buffer_content, 1, bytes_received, fp);
        if (written < (size_t)bytes_received) {
            perror("Error writing to file");
            break;
        }
    }

    if (bytes_received < 0) {
        perror("Error receiving file");
    }

    fclose(fp);

    if (download_successful) {
        printf("  Your file has been downloaded.\n");
    } else {
        printf("\tFailed: Download interupted.!\n");
    }

}

// Handle rmfile command
void handle_rmfile(int sock, char *tokens[]) {
    char recv_buffer[BUFSIZE];
    char buffer[BUFSIZE];

    if (!tokens[1]) {
        printf("Error: Missing filename for rmfile.\n");
        return;
    }

    char *file_path = tokens[1];

    // check path
    char *token_path = tokens[1];
    if (strncmp(token_path, "~/smain/", 8) != 0) {
        printf("Error: Path must start with '~/smain/'\n");
        return;
    }

    // Make a copy of file_path for tokenization
    char file_path_copy[BUFSIZE];
    strncpy(file_path_copy, file_path, BUFSIZE - 1);
    file_path_copy[BUFSIZE - 1] = '\0';

    // Tokenize the file path to get the file name
    char *last_token = NULL;
    char *token = strtok(file_path_copy, "/");
    while (token != NULL) {
        last_token = token;
        token = strtok(NULL, "/");
    }
    // check extension is valid or not
    if (last_token == NULL || !is_valid_extension(last_token)) {
        printf("Error: Invalid file extension.\n");
        return;
    }

    // Send the rmfile command to the server
    snprintf(buffer, sizeof(buffer), "rmfile %s", file_path);
    send(sock, buffer, strlen(buffer) + 1, 0);

    // Receive and display the confirmation message
    ssize_t bytes_received = recv(sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
    if (bytes_received > 0) {
        recv_buffer[bytes_received] = '\0';
        printf("Server: %s\n", recv_buffer);
    } else if (bytes_received == 0) {
        printf("Connection closed by server.\n");
        exit(EXIT_SUCCESS);
    } else {
        perror("Error receiving data");
    }
}

// Handle dtar command
void handle_dtar(int sock, char *tokens[]) {
    if (!tokens[1]) {
        printf("Error: Missing filename for dtar.\n");
        return;
    }
    printf("dtar command received for %s\n", tokens[1]);
    // Implement dtar logic here
}

// Handle display command
void handle_display(int sock, char *tokens[]) {
    printf("display command received\n");
    // Implement display logic here
}
