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
#include <sys/wait.h>
#include <dirent.h>


#define PORT 8080
#define BUFSIZE 1024
#define CMD_END_MARKER "END_CMD"

// Function prototypes
void prcclient(int client_sock);
void handle_ufile(int client_sock, char *command, char *file_data);
void handle_dfile(int client_sock, char *command);
void handle_rmfile(int client_sock, char *command);
void handle_dtar(int client_sock, char *command);
void handle_display(int client_sock, char *command);
int connect_to_spdf();
int connect_to_stext();
void send_file_to_server(int server_sock, int client_sock, char *command, char *filename, char *destination_path, char *file_data);
int receive_and_save_file(int sock, char *destination_path, char *f_name, char *file_data);
void remove_file_from_server(int sock, int client_sock, char *command, char *destination_path);
void send_file_to_client(int client_sock, const char *file_path, const char *file_name);
int delete_file(const char *file_path);
void send_download_request(int server_sock, int client_sock, char *command, char *file_path);
void get_file_names_from_server(int (*connect_func)(), const char *message, const char *error_prefix, char *response_buffer, size_t buffer_size);


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

        // Separate command and file data
        char *file_data = strstr(buffer, "END_CMD");
        if (file_data) {
            *file_data = '\0';  // Null-terminate the command part
            file_data += strlen("END_CMD");  // Skip "END_CMD"
        }

        // Determine which command to process
        if (strncmp(buffer, "ufile", 5) == 0) {
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
    }
}

int is_valid_path(const char *path) {
    // Check if the path starts with "smain"
    if (strncmp(path, "~/smain",7) != 0) {
        return 0; // Invalid path
    }
    return 1; // Valid path
}

// Function to handle 'ufile' command
void handle_ufile(int client_sock, char *command, char *file_data) {
    char filename[256], destination_path[256];
    int server_sock;


    /// Extract filename and destination path from the command
    if (sscanf(command, "ufile %s %s", filename, destination_path) != 2) {
        printf("Command parsing failed\n");
        send(client_sock, "File upload failed", 18, 0);
        return;
    }

    if (strstr(filename, ".pdf") != NULL) {
        // Forward to Spdf server
        server_sock = connect_to_spdf();
        if (server_sock < 0) {
            printf("Failed to connect to Spdf server\n");
            send(client_sock, "File upload failed", 18, 0);
            return;
        }
        send_file_to_server(server_sock, client_sock, "ufile", filename, destination_path, file_data);
        close(server_sock);

    } else if (strstr(filename, ".txt") != NULL) {
        // Forward to Stext server
        server_sock = connect_to_stext();
        if (server_sock < 0) {
            printf("Failed to connect to Stext server\n");
            send(client_sock, "File upload failed", 18, 0);
            return;
        }
        send_file_to_server(server_sock, client_sock, "ufile", filename, destination_path, file_data);
        close(server_sock);

    } else if (strstr(filename, ".c") != NULL) {
        // Save locally
        if (receive_and_save_file(client_sock, destination_path, filename, file_data) == 0) {
            const char *success_message = "File Uploaded successfully.";
            send(client_sock, success_message, strlen(success_message), 0);
        } else {
            const char *failed_message = "File uploading failed!";
            send(client_sock, failed_message, strlen(failed_message), 0);
        }
    } else {
        printf("Unsupported file type: %s\n", filename);
        send(client_sock, "Unsupported file type", 21, 0);
    }
}

// Function to handle 'dfile' command
void handle_dfile(int client_sock, char *command) {
    int server_sock;
    char file_path[256];
    sscanf(command, "dfile %s", file_path);
    printf("file_path: %s\n", file_path);

    // check if requested doenload file path is valid or not
    if(!is_valid_path(file_path)){
        printf("ERROR: Invalid path!\n");
        // Send error message if the path is invalid
        const char *error_message = "ERROR: Invalid path!";
        send(client_sock, error_message, strlen(error_message), 0);
        return;
    }
    
    // Extract the file name
    char *file_name = strrchr(file_path, '/') + 1;

    printf("file_name: %s\n",file_name);
    if (!file_name) {
        send(client_sock, "Invalid file path", 17, 0);
        return;
    }

    if(strstr(file_name,".c") != NULL){
        printf("c file\n");
        send_file_to_client(client_sock, file_path, file_name);
    }else if(strstr(file_name,".txt") != NULL){
        printf("txt file\n");
        // Forward to Stext server
        server_sock = connect_to_stext();
        if (server_sock < 0) {
            printf("Failed to connect to Stext server\n");
            return;
        }
        send_download_request(server_sock, client_sock, "dfile", file_path);
        close(server_sock);

    }else if(strstr(file_name,".pdf") != NULL){
        printf("pdf file\n");
        // Forward to Stext server
        server_sock = connect_to_spdf();
        if (server_sock < 0) {
            printf("Failed to connect to Stext server\n");
            return;
        }
        send_download_request(server_sock, client_sock, "dfile", file_path);
        close(server_sock);

    }else{
        printf("Invalid file type\n");
        // Send an error message to the client with a specific prefix
        const char *success_message = "ERROR: Invalid file type!";
        send(client_sock, success_message, strlen(success_message), 0);
        return;
    }
}

void send_file_to_client(int client_sock, const char *file_path, const char *file_name) {
    
    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    // Construct the full path for the file
    char full_path[BUFSIZE];
    if (file_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, file_path + 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", file_path);
    }

    // Open the file for reading
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        perror("File open failed");
        // Send rejction to the client
        const char *success_message = "ERROR: File not found!";
        send(client_sock, success_message, strlen(success_message), 0);
        return;
    }

    // send file name
    send(client_sock, file_name, strlen(file_name), 0);
    // Read the file and send its contents to the client
    char buffer_content[BUFSIZE];
    ssize_t bytes_read,bytes_sent;
    while ((bytes_read = read(file_fd, buffer_content, sizeof(buffer_content))) > 0) {
        // Debug print: show the content being sent
        printf("Read %zd bytes: %.*s\n", bytes_read, (int)bytes_read, buffer_content);
        bytes_sent = send(client_sock, buffer_content, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Error sending file");
            break;
        }
    }
    if (bytes_read < 0) {
        perror("Error reading file");
    }
    close(file_fd);
    // Send the end marker
    if (send(client_sock, CMD_END_MARKER, strlen(CMD_END_MARKER), 0) == -1) {
        perror("Failed to send end marker");
    }

}


// Function to handle 'rmfile' command
void handle_rmfile(int client_sock, char *command) {
    
    char file_path[256];
    int server_sock;

    sscanf(command, "rmfile %s", file_path);
    
    // Make a copy of file_path for tokenization
    char file_path_copy[BUFSIZE];
    strncpy(file_path_copy, file_path, BUFSIZE - 1);
    file_path_copy[BUFSIZE - 1] = '\0';

    // Tokenize the file path to get the file name
    char *file_name = NULL;
    char *token = strtok(file_path_copy, "/");
    while (token != NULL) {
        file_name = token;
        token = strtok(NULL, "/");
    }
    printf("file_name: %s\n",file_name);

    // check for the extention and pass it to the server
    if (strstr(file_name, ".pdf") != NULL) {
        // Forward to Spdf server
        server_sock = connect_to_spdf();
        if (server_sock < 0) {
            printf("Failed to connect to Spdf server\n");
            send(client_sock, "File remove failed", 18, 0);
            return;
        }
        remove_file_from_server(server_sock, client_sock, "rmfile", file_path);
        close(server_sock);

    } else if (strstr(file_name, ".txt") != NULL) {
        // Forward to Stext server
        server_sock = connect_to_stext();
        if (server_sock < 0) {
            printf("Failed to connect to Stext server\n");
            send(client_sock, "File remove failed", 18, 0);
            return;
        }
        remove_file_from_server(server_sock, client_sock, "rmfile", file_path);
        close(server_sock);

    } else if (strstr(file_name, ".c") != NULL) {
        // Delete the .c file
        if (delete_file(file_path) == 0) {
            printf("Error deleting file: %s\n", file_path);
            send(client_sock, "File has been removed", 21, 0);
        }else{
            send(client_sock, "File remove failed", 18, 0);
        }

    } else {
        printf("Unsupported file type: %s\n", file_name);
    }
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
    char pathname[256];
    char full_path[BUFSIZE];
    int server_sock;
    struct stat path_stat;
    int server_sock_pdf,server_sock_txt;

    // Parse the pathname from the command
    sscanf(command, "display %s", pathname);

    // Initialize file lists
    char c_files[BUFSIZE] = "";  // List of .c files
    char pdf_files[BUFSIZE] = ""; // List of .pdf files from server
    char txt_files[BUFSIZE] = ""; // List of .txt files from server

    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    // Construct the full path for the directory
    if (pathname[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, pathname + 1);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", pathname);
    }


    int path_exists = 0;
    if (stat(full_path, &path_stat) == 0) {
        if (S_ISDIR(path_stat.st_mode)) {
            path_exists = 1;
            // Step 1: Retrieve the list of .c files from the local directory
            DIR *dir = opendir(full_path);
            struct dirent *entry;
            if (dir != NULL) {
                while ((entry = readdir(dir)) != NULL) {
                    if (strstr(entry->d_name, ".c") != NULL) {
                        strcat(c_files, entry->d_name);
                        strcat(c_files, "\n");
                    }
                }
                closedir(dir);
            }
        } else {
            // Path exists but is not a directory
            printf("ERROR: Not a directory in Smain!\n");
        }
    } else {
        // Error in stat, path might not exist
        printf("ERROR: Invalid path or not a directory in Smain!\n");
    }

    // Check if the response is an error message
    const char *error_prefix = "ERROR:";

    // Construct the message to send
    char message[BUFSIZE];
    snprintf(message, sizeof(message), "display %s", full_path);

     // Retrieve .pdf files from Spdf server
    get_file_names_from_server(connect_to_spdf, message, error_prefix, pdf_files, sizeof(pdf_files));

    // Retrieve .txt files from Stext server
    get_file_names_from_server(connect_to_stext, message, error_prefix, txt_files, sizeof(txt_files));



    // Step 3: Combine the lists
    char combined_list[3 * BUFSIZE] = "";
    if (path_exists) {
        strcat(combined_list, c_files);
    }
    strcat(combined_list, pdf_files);
    strcat(combined_list, txt_files);


    if(strlen(combined_list) == 0){
        printf("Directory doesnot exist or No files, from Spdf and Stext...\n");
        const char *error_message = "ERROR: Directory doesnot exist or No files found!";
        send(client_sock, error_message, strlen(error_message), 0);
    }else{
        printf("All files: \n%s",combined_list);
        send(client_sock,combined_list,strlen(combined_list),0);
    }
    
}

// Function to connect to the Spdf server
int connect_to_spdf() {
    int server_sock;
    struct sockaddr_in server_addr;

    // Create a socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Spdf socket creation failed");
        return -1;
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8081);  // Port for Spdf
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the Spdf server
    if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect to Spdf failed");
        close(server_sock);
        return -1;
    }

    return server_sock;
}

// Function to connect to the Stext server
int connect_to_stext() {
    int server_sock;
    struct sockaddr_in server_addr;

    // Create a socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Stext socket creation failed");
        return -1;
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8082);  // Port for Stext
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the Stext server
    if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect to Stext failed");
        close(server_sock);
        return -1;
    }

    return server_sock;
}


// Function to send a file to a specified server
void send_file_to_server(int server_sock, int client_sock, char *command, char *filename, char *destination_path, char *file_data) {
    char buffer[BUFSIZE];
    ssize_t bytes_sent;
    char recv_buffer[BUFSIZE];

    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    
    // Construct the full path for the file (FilePath + file name)
    char full_path[BUFSIZE];
    if (destination_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s/%s/%s", home_dir, destination_path + 1, filename);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", destination_path, filename);
    }
    
    // Construct the message with the command and the full path
    char message[BUFSIZE];
    snprintf(message, sizeof(message), "%s %s\n", command, full_path);

    // Calculate the total length of the message including file data
    size_t total_length = strlen(message) + strlen(file_data) + 1; // +1 for null terminator
    
    // Allocate buffer for the complete message
    char *complete_message = malloc(total_length);
    if (complete_message == NULL) {
        perror("Memory allocation failed");
        return;
    }

    // Copy the message and file data into the complete_message buffer
    strcpy(complete_message, message);
    strcat(complete_message, file_data);

    // Send the complete message (command, full path, and file data in one go)
    bytes_sent = send(server_sock, complete_message, total_length - 1, 0); // -1 to exclude the null terminator
    if (bytes_sent < 0) {
        perror("Send failed");
    }

    // Receive and display the confirmation message
    ssize_t bytes_received = recv(server_sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
    if (bytes_received > 0) {
        recv_buffer[bytes_received] = '\0';
        // Forward the server response to the client
        if (send(client_sock, recv_buffer, bytes_received, 0) < 0) {
            perror("Send to client failed");
        }
    } else if (bytes_received == 0) {
        printf("Connection closed by server.\n");
        exit(EXIT_SUCCESS);
    } else {
        perror("Error receiving data");
    }

    // Free allocated memory
    free(complete_message);
}


// Function to receive a file from a client and save it to the specified destination
int receive_and_save_file(int sock, char *destination_path, char *f_name, char *file_data) {
    char buffer[BUFSIZE];
    int file_fd;
    ssize_t bytes_received;
    char dir_path[256];
 
    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return -1;
    }
 
    // Create the full path for the file
    char full_path[BUFSIZE];
    if (destination_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, destination_path + 1);
    } else {
        strncpy(full_path, destination_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
 
    // Copy the destination path to dir_path
    strncpy(dir_path, full_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
 
    // Ensure the destination directory exists
    char command[BUFSIZE];
    snprintf(command, sizeof(command), "mkdir -p %s", dir_path);
    system(command);
 
    // Construct the full path for the file (path + file name)
    char final_path[512];
    snprintf(final_path, sizeof(final_path), "%s/%s", full_path, f_name);
 
    printf("Final path: %s\n", final_path);
 
    // Create the file
    file_fd = open(final_path, O_CREAT | O_RDWR | O_TRUNC, 0777);
    if (file_fd < 0) {
        perror("File creation failed");
        return -1;
    }
 
    // Write the received file data
    if (file_data) {
        write(file_fd, file_data, strlen(file_data));
    }
 
    close(file_fd);
    return 0;
}


// Function to remove requested file by client from servers
void remove_file_from_server(int sock, int client_sock, char *command, char *destination_path){

    char recv_buffer[BUFSIZE];

    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    
    // Construct the full path for the file (FilePath + file name)
    char full_path[BUFSIZE];
    if (destination_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, destination_path+1);
    }
    printf("fullpath: .%s.\n",full_path);

    // Construct the message to send
    char message[BUFSIZE];
    snprintf(message, sizeof(message), "%s %s", command, full_path);
    
    // Send the message to the server
    if (send(sock, message, strlen(message), 0) == -1) {
        perror("send");
        return;
    }
    printf("Message sent: %s\n", message);

    // Receive and display the confirmation message
    ssize_t bytes_received = recv(sock, recv_buffer, sizeof(recv_buffer) - 1, 0);
    if (bytes_received > 0) {
        recv_buffer[bytes_received] = '\0';
        // Forward the server response to the client
        if (send(client_sock, recv_buffer, bytes_received, 0) < 0) {
            perror("Send to client failed");
        }
    } else if (bytes_received == 0) {
        printf("Connection closed by server.\n");
        exit(EXIT_SUCCESS);
    } else {
        perror("Error receiving data");
    }
}

// Function to delete a file and handle errors
int delete_file(const char *file_path) {
    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return -1;
    }

    // new file path creation to replace ~
    char full_path[BUFSIZE];
    if (file_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, file_path + 1);
    } else {
        strncpy(full_path, file_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
    
    printf("full_path:%s\n",full_path);

    if (unlink(full_path) == 0) {
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
}


void send_download_request(int server_sock, int client_sock, char *command, char *file_path){
    printf("pdf or txt\n");

    printf("file_path: %s\n",file_path);

    // Replace ~ with the value of the HOME environment variable
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "Failed to get HOME environment variable\n");
        return;
    }
    
    // Construct the full path for the file (FilePath + file name)
    char full_path[BUFSIZE];
    if (file_path[0] == '~') {
        snprintf(full_path, sizeof(full_path), "%s%s", home_dir, file_path+1);
    }
    printf("fullpath: .%s.\n",full_path);

    // Construct the message to send
    char message[BUFSIZE];
    snprintf(message, sizeof(message), "%s %s", command, full_path);
    
    // Send the message to the server
    if (send(server_sock, message, strlen(message), 0) == -1) {
        perror("send");
        return;
    }
    printf("Message sent: %s\n", message);


    // Receive the file name from the server
    char file_name[256];
    ssize_t bytes_received = recv(server_sock, file_name, sizeof(file_name) - 1, 0);
    if (bytes_received <= 0) {
        perror("Error receiving file name");
        return;
    }
    file_name[bytes_received] = '\0'; // Null-terminate the file name
    printf("Received file name: %s\n", file_name);


    //send the file name to the client
    if (send(client_sock, file_name, strlen(file_name), 0) == -1) {
        perror("send");
    }


    // Receive the file content from the server and forward it to the client
    char buffer[BUFSIZE];
    ssize_t content_received;
    while ((content_received = recv(server_sock, buffer, sizeof(buffer), 0)) > 0) {
        // Debug print: Show the content being received
        printf("Received %zd bytes of content\n", content_received);

        // Forward the received content to the client
        ssize_t bytes_sent = send(client_sock, buffer, content_received, 0);
        if (bytes_sent < 0) {
            perror("send");
            break;
        }

        // Check for end marker if the buffer is smaller than BUFSIZE
        if (content_received < BUFSIZE) {
            if (memcmp(buffer + content_received - strlen(CMD_END_MARKER), CMD_END_MARKER, strlen(CMD_END_MARKER)) == 0) {
                // Handle end of content
                break;
            }
        }
    }
    if (content_received < 0) {
        perror("Error receiving file content");
    }    
}


// Function to handle server communication with fixed-size buffer
void get_file_names_from_server(int (*connect_func)(), const char *message, const char *error_prefix, char *response_buffer, size_t buffer_size) {
    int server_sock = connect_func();
    if (server_sock < 0) {
        printf("Failed to connect to server\n");
        response_buffer[0] = '\0'; // Clear the buffer on connection failure
        return;
    }

    send(server_sock, message, strlen(message), 0);

    // Clear the response buffer
    memset(response_buffer, 0, buffer_size);

    // Receive the server response
    recv(server_sock, response_buffer, buffer_size - 1, 0); // Read response, leaving space for null terminator

    // Check if the response starts with the error prefix
    if (strncmp(response_buffer, error_prefix, strlen(error_prefix)) != 0) {
        close(server_sock);
    } else {
        response_buffer[0] = '\0'; // Clear the buffer on error
        close(server_sock);
    }
}
