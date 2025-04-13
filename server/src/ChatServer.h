#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <map>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define SOCKET_ERROR_VAL INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
    #define SOCKET_ERROR_VAL -1
#endif

struct ChatMessage {
    uint32_t messageId;
    uint32_t messageLength;
    uint8_t data[1024];
};

class ChatServer {
    private:
        int serverSocket;
        struct sockaddr_in serverAddr;
        int port;
        std::string serverPin;
        std::vector<uint8_t> serverKey;
        int requiredClientsToStart;
        bool chatSessionActive;

        std::mutex clientsMutex;
        std::map<int, std::string> clients; // socket fd -> nickname
        std::atomic<bool> running;
        std::atomic<uint32_t> nextMessageId;

        void acceptClients();

        void checkAndActivateSession();
        void checkAndDeactivateSession();

        void handleClientAuth(int clientSocket);
        void handleClientMessages(int clientSocket);

        void broadcastMessage(int senderSocket, const ChatMessage &msg);
        void broadcastUserList();

    public:
        ChatServer(int port, const std::string &pin, int requiredClients);
        ~ChatServer();

        bool start();
        void stop();
};