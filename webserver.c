#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_PATH "index.html"

void send_file_response(int client_socket) {
    int file_fd = open(FILE_PATH, O_RDONLY);
    if (file_fd == -1) {
        // ファイルが見つからない場合、404エラーメッセージを送信
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

    // ファイルサイズを取得
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    // HTTPレスポンスヘッダー
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %lld\r\n"
             "Connection: close\r\n"
             "\r\n",
             file_size);
    
    // ヘッダーを送信
    send(client_socket, header, strlen(header), 0);

    // ファイルの内容を送信
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(file_fd);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    buffer[bytes_received] = '\0'; // 受信データを文字列として処理
    printf("Received request:\n%s\n", buffer);

    // GETリクエストならHTMLファイルを返す
    if (strncmp(buffer, "GET / ", 6) == 0) {
        send_file_response(client_socket);
    } else {
        // 未対応のリクエストには400エラーを返す
        const char *bad_request_response =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>400 Bad Request</h1></body></html>";

        send(client_socket, bad_request_response, strlen(bad_request_response), 0);
    }

    close(client_socket);
    exit(0); // 子プロセスを終了
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // ソケットの作成
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // ソケットの設定
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // バインド
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // リスニング
    if (listen(server_socket, 5) < 0) {
        perror("listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // クライアント接続の待機
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }

        // 子プロセスを作成
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            close(client_socket);
            continue;
        }

        if (pid == 0) {
            // 子プロセス: クライアントを処理
            close(server_socket); // 子プロセスではサーバーソケットは不要
            handle_client(client_socket);
        } else {
            // 親プロセス: クライアントソケットを閉じて新しい接続を待つ
            close(client_socket);
        }

        // 終了した子プロセスを回収 (ゾンビプロセス防止)
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    // サーバーソケットを閉じる
    close(server_socket);
    return 0;
}

