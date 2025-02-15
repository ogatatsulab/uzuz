#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_PATH "index.html"

void send_file_response(int client_socket) {
    int file_fd = open(FILE_PATH, O_RDONLY);
    if (file_fd == -1) {
        const char *not_found_response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>404 Not Found</h1></body></html>";
        send(client_socket, not_found_response, strlen(not_found_response), 0);
        close(client_socket);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %lld\r\n"
             "Connection: close\r\n"
             "\r\n",
             file_size);
    
    send(client_socket, header, strlen(header), 0);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(file_fd);
}

void* handle_client(void* client_socket_ptr) {
    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr); // 動的メモリ確保していたら解放

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(client_socket);
        pthread_exit(NULL);
    }

    buffer[bytes_received] = '\0'; // 受信データを文字列として処理
    printf("Received request:\n%s\n", buffer);

    if (strncmp(buffer, "GET / ", 6) == 0) {
        send_file_response(client_socket);
    } else {
        const char *bad_request_response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>400 Bad Request</h1></body></html>";
        send(client_socket, bad_request_response, strlen(bad_request_response), 0);
    }

    close(client_socket);
    pthread_exit(NULL); // スレッドの終了
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }

        // クライアントソケットを動的確保（スレッドの安全性のため）
        int* client_sock_ptr = malloc(sizeof(int));
        if (client_sock_ptr == NULL) {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }

        *client_sock_ptr = client_socket;
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_sock_ptr) != 0) {
            perror("pthread_create failed");
            close(client_socket);
            free(client_sock_ptr);
        } else {
            pthread_detach(thread); // スレッドのリソースを自動回収
        }
    }

    close(server_socket);
    return 0;
}

