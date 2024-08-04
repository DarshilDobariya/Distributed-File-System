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

// Function to send a file to the server
void send_file(int sock, char *filename, char *destination_path) {
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_read;

    // Send the command with the filename and destination path to the server
    snprintf(buffer, sizeof(buffer), "ufile %s %s", filename, destination_path);
    send(sock, buffer, strlen(buffer) + 1, 0);

    // // Open the file
    // file_fd = open(filename, O_RDONLY);
    // if (file_fd < 0) {
    //     perror("File open failed");
    //     return;
    // }

    // // Read the file and send its content to the server
    // while ((bytes_read = read(file_fd, buffer, BUFSIZE)) > 0) {
    //     send(sock, buffer, bytes_read, 0);
    // }

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
    if (access(filename, F_OK) == -1) {
        printf("Error: File does not exist.\n");
        return;
    }
    if (strncmp(destination_path, "~/smain/", 8) != 0) {
        printf("Error: Destination path must start with '~/smain/'\n");
        return;
    }
    send_file(sock, filename, destination_path);
}

// Handle dfile command
void handle_dfile(int sock, char *tokens[]) {
    if (!tokens[1]) {
        printf("Error: Missing filename for dfile.\n");
        return;
    }
    printf("dfile command received for %s\n", tokens[1]);
    // Implement dfile logic here
}

// Handle rmfile command
void handle_rmfile(int sock, char *tokens[]) {
    if (!tokens[1]) {
        printf("Error: Missing filename for rmfile.\n");
        return;
    }
    printf("rmfile command received for %s\n", tokens[1]);
    // Implement rmfile logic here
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
