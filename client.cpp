#define _WIN32_WINNT 0x0601
#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "10.128.3.106"
#define PORT 12138
#define BUFFER_SIZE 1024

// 初始化 WinSock
bool init_winsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "Failed to initialize WinSock. Error: " << result << endl;
        return false;
    }
    return true;
}

// 创建套接字并连接到服务器
SOCKET connect_to_server() {
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        cerr << "Failed to create socket. Error: " << WSAGetLastError() << endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        cerr << "Invalid IP address. Error: " << WSAGetLastError() << endl;
        closesocket(client_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Failed to connect to server. Error: " << WSAGetLastError() << endl;
        closesocket(client_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    cout << "Connected to server." << endl;
    return client_socket;
}

// 接收消息
void receive_messages(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(client_socket, buffer, sizeof(buffer), 0);

        if (len > 0) {
            string received_message(buffer, len);
            if (received_message == "FULL") {
                cout << "The server is full. Unable to connect." << endl;
                closesocket(client_socket);
                return;
            }
            cout << received_message<<endl ;
        } else {
            cerr << "Connection closed or error occurred. Error: " << WSAGetLastError() << endl;
            break;
        }
    }
}

// 发送消息
void send_messages(SOCKET client_socket) {
    string message;
    while (true) {
        getline(cin, message);
        int message_length = htonl(message.size());
        if (send(client_socket, reinterpret_cast<const char*>(&message_length), 4, 0) == SOCKET_ERROR) {
            cerr << "Failed to send message length. Error: " << WSAGetLastError() << endl;
            break;
        }

        if (send(client_socket, message.c_str(), message.size(), 0) == SOCKET_ERROR) {
            cerr << "Failed to send message. Error: " << WSAGetLastError() << endl;
            break;
        }

        if (message == "exit") {
            break;
        }
    }
}

int main() {
    if (!init_winsock()) {
        return EXIT_FAILURE;
    }

    SOCKET client_socket = connect_to_server();
    
    // 启动接收消息的线程
    thread recv_thread(receive_messages, client_socket);

    // 主线程用于发送消息
    send_messages(client_socket);

    // 等待接收线程结束
    if (recv_thread.joinable()) {
        recv_thread.join();
    }

    closesocket(client_socket);
    WSACleanup();
    return 0;
}

