#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mutex>
#include <thread>
#include <algorithm>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define PORT 12138
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 5

static int thread_num = 0;
static int client_id = 0;  // 用于为每个客户端生成唯一ID
vector<SOCKET> client_sockets;   // 用于保存所有客户端套接字
mutex client_sockets_mutex;      // 用于保护client_sockets的互斥访问

// 初始化 WinSock
bool init_winsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "Failed to initialize WinSock." << endl;
        return false;
    }
    return true;
}

// 创建套接字
SOCKET create_socket() {
    SOCKET socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd == INVALID_SOCKET) {
        cerr << "Failed to create socket." << endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    return socket_fd;
}

// 绑定地址并开始监听
void setup_server(SOCKET server_socket) {
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cerr << "Failed to bind socket." << endl;
        closesocket(server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == SOCKET_ERROR) {
        cerr << "Failed to listen on socket." << endl;
        closesocket(server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    cout << "Server is listening on port " << PORT << endl;
}
// 安全接收指定长度的数据
bool recv_all(SOCKET client_socket, char* buffer, int length) {
    int total_received = 0;
    while (total_received < length) {
        int bytes_received = recv(client_socket, buffer + total_received, length - total_received, 0);
        if (bytes_received <= 0) {
            return false; // 连接关闭或出错
        }
        total_received += bytes_received;
    }
    return true;
}
// 广播消息给其他客户端
void broadcast_message(const string& message, const string& sender_name, SOCKET sender_socket) {
    lock_guard<mutex> lock(client_sockets_mutex);
    string full_message = sender_name + ": " + message;
    int full_message_length = htonl(full_message.size());

    for (SOCKET client_socket : client_sockets) {
        if (client_socket != sender_socket) {
            if (send(client_socket, reinterpret_cast<const char*>(&full_message_length), 4, 0) == SOCKET_ERROR) {
                cerr << "Failed to send message length. Error: " << WSAGetLastError() << endl;
                continue;
            }
            if (send(client_socket, full_message.c_str(), full_message.size(), 0) == SOCKET_ERROR) {
                cerr << "Failed to send message to client. Error: " << WSAGetLastError() << endl;
                continue;
            }
        }
    }
}

// 处理客户端连接
void handle_client(SOCKET client_socket, int client_id) {
    char buffer[BUFFER_SIZE];
    string client_name = "client" + to_string(client_id);

    // 若已达到最大连接数，发送 "FULL" 消息并断开连接
    {
        lock_guard<mutex> lock(client_sockets_mutex);
        if (thread_num >= MAX_CLIENTS) {
            string reject_msg = "FULL";
            send(client_socket, reject_msg.c_str(), reject_msg.size(), 0);
            closesocket(client_socket);
            return;
        }

        // 增加连接并添加到列表
        client_sockets.push_back(client_socket);
        thread_num++;
    }

    while (true) {
        // 读取消息长度
        if (!recv_all(client_socket, buffer, 4)) {
            cout << client_name << " disconnected or error occurred." << endl;
            break;
        }

        int message_length = ntohl(*(reinterpret_cast<int*>(buffer)));
        if (message_length > BUFFER_SIZE) {
            cerr << client_name << " sent a message that exceeds buffer size." << endl;
            break;
        }

        // 读取消息内容
        memset(buffer, 0, sizeof(buffer));
        if (!recv_all(client_socket, buffer, message_length)) {
            cout << client_name << " disconnected or error occurred while receiving message." << endl;
            break;
        }

        string message_to_broadcast(buffer, message_length);
        if (message_to_broadcast == "exit" || message_to_broadcast == "EXIT") {
            cout << client_name << " exited the chat." << endl;
            broadcast_message(client_name + " has left the chat.", "Server", client_socket);
            break;
        }

        cout << client_name << " sent: " << message_to_broadcast << endl;
        broadcast_message(message_to_broadcast, client_name, client_socket);
    }

    // 清理连接
    {
        lock_guard<mutex> lock(client_sockets_mutex);
        client_sockets.erase(remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
        thread_num--;
    }
    closesocket(client_socket);
}



int main() {
    if (!init_winsock()) {
        return EXIT_FAILURE;
    }

    SOCKET server_socket = create_socket();
    setup_server(server_socket);

    while (true) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (SOCKADDR*)&client_addr, &addr_len);

        if (client_socket == INVALID_SOCKET) {
            cerr << "Failed to accept client connection. Error: " << WSAGetLastError() << endl;
            continue;
        }

        int current_client_id = ++client_id;
        cout << "Client connected: client" << current_client_id << " (" << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << ")" << endl;

        // 创建线程来处理客户端
        thread(handle_client, client_socket, current_client_id).detach();
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}

