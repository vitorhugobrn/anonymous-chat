#include "ChatServer.h"

#include "../../shared/sha256.h"
#include "../../shared/aes256.h"

#include <iostream>
#include <thread>

ChatServer::ChatServer(int port, const std::string &pin, int requiredClients) 
    : port(port), serverPin(pin), running(false), nextMessageId(1), 
    requiredClientsToStart(requiredClients), chatSessionActive(false)  
{
    serverKey = generateSHA256Bytes(pin.c_str(), pin.length());
}

ChatServer::~ChatServer() {
    stop();
}

bool ChatServer::start() {
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
    #endif

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == SOCKET_ERROR_VAL) {
        std::cerr << "Error creating socket" << std::endl;
        return false;
    }

    int opt = true;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }

    // Set up server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error binding socket" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }

    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Error listening on socket" << std::endl;
        CLOSE_SOCKET(serverSocket);
        return false;
    }

    running = true;
    std::cout << "Server started on port " << port << std::endl;

    std::thread acceptThread(&ChatServer::acceptClients, this);
    acceptThread.detach();

    return true;
}

void ChatServer::stop() {
    running = false;
    if (serverSocket >= 0) {
        CLOSE_SOCKET(serverSocket);
        serverSocket = -1;
    }

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto &client : clients) {
        CLOSE_SOCKET(client.first);
    }
    clients.clear();
}

void ChatServer::acceptClients() {
    while (running) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrlen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrlen);
        if (clientSocket < 0 && running) {
            std::cerr << "Error accepting client connection" << std::endl;
            continue;
        }

        std::cout << "New connection from " 
                  << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) 
                  << std::endl;

        std::thread clientThread(&ChatServer::handleClientAuth, this, clientSocket);
        clientThread.detach();
    }
}

void ChatServer::handleClientAuth(int clientSocket) {
    char buffer[1024];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        CLOSE_SOCKET(clientSocket);
        return;
    }

    buffer[bytesRead] = '\0';

    std::string data(buffer);
    size_t delimPos = data.find(':');
    if (delimPos == std::string::npos) {
        CLOSE_SOCKET(clientSocket);
        return;
    }

    std::string nickname = data.substr(0, delimPos);
    std::string pin = data.substr(delimPos + 1);

    if (pin != serverPin) { // TODO: Maybe handle multiple pins?
        send(clientSocket, "Invalid PIN", 11, 0);
        CLOSE_SOCKET(clientSocket);
        return;
    }

    send(clientSocket, "Connected", 9, 0); // FIXME

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients[clientSocket] = nickname;
    }

    broadcastUserList();
    checkAndActivateSession();
    handleClientMessages(clientSocket);
}

void ChatServer::handleClientMessages(int clientSocket) {
    AES256 aesCtx; // TODO: Remove from here
    AES256Init(&aesCtx, serverKey.data());

    std::vector<uint8_t> buffer(1024);

    while (running) {
        ChatMessage encryptedMsg;
        int bytesRead = recv(clientSocket, (char *)&encryptedMsg, sizeof(encryptedMsg), 0);
        if (bytesRead <= 0) // Client disconnected
            break;

        broadcastMessage(clientSocket, encryptedMsg);
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(clientSocket);
    }

    CLOSE_SOCKET(clientSocket);
    checkAndDeactivateSession();
    broadcastUserList();
}

void ChatServer::broadcastMessage(int senderSocket, const ChatMessage &msg) {
    std::lock_guard<std::mutex> lock(clientsMutex);

    AES256 aesCtx;
    AES256Init(&aesCtx, serverKey.data());

    if (!chatSessionActive) {
        std::string waitMessage = "SYSTEM:Waiting for more users to join.";
        
        ChatMessage waitMsg;
        waitMsg.messageId = nextMessageId++;
        
        size_t waitEncryptedSize = AES256EncryptMessage(&aesCtx, waitMsg.data, waitMessage.c_str(), waitMessage.length());
        waitMsg.messageLength = (waitMessage.length() << 16 | (waitEncryptedSize & 0xFFFF));
        
        send(senderSocket, (char *)&waitMsg, sizeof(waitMsg), 0);
        return;
    }

    for (const auto &client : clients) {
        if (client.first != senderSocket)
            send(client.first, (char *)&msg, sizeof(msg), 0);
    }
}

void ChatServer::broadcastUserList() {
    // This should've been an OPCODE, but due lack of time
    // we're going to keep this approach as a workaround
    std::string userList = "USERLIST:";
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto &client : clients) {
            userList += client.second + ",";
        }
    }

    ChatMessage msg;
    msg.messageId = nextMessageId++;

    AES256 aesCtx;
    AES256Init(&aesCtx, serverKey.data()); // FIXME: Remove from here;

    size_t encryptedSize = AES256EncryptMessage(&aesCtx, msg.data, userList.c_str(), userList.length());
    msg.messageLength = (userList.length() << 16 | (encryptedSize & 0xFFFF));
    
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto &client : clients) {
        send(client.first, (char *)&msg, sizeof(msg), 0);
    }
}

void ChatServer::checkAndActivateSession() {
    std::lock_guard<std::mutex> lock(clientsMutex);

    if (chatSessionActive || clients.size() < requiredClientsToStart)
        return;

    chatSessionActive = true;

    std::string startMessage = "SESSION_START:Chat session has started!";

    ChatMessage msg;
    msg.messageId = nextMessageId++;

    AES256 aesCtx;
    AES256Init(&aesCtx, serverKey.data());

    size_t encryptedSize = AES256EncryptMessage(&aesCtx, msg.data, startMessage.c_str(), startMessage.length());
    msg.messageLength = (startMessage.length() << 16 | (encryptedSize & 0xFFFF));

    for (const auto &client : clients) {
        send(client.first, (char *)&msg, sizeof(msg), 0);
    }

    std::cout << "Chat session activated with " << clients.size() << " clients" << std::endl;
}

void ChatServer::checkAndDeactivateSession() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    
    if (chatSessionActive && clients.size() < requiredClientsToStart) {
        chatSessionActive = false;
        
        std::string interruptMessage = "SESSION_INTERRUPT:Chat session paused. Waiting for more users to join.";
        
        ChatMessage msg;
        msg.messageId = nextMessageId++;

        AES256 aesCtx;
        AES256Init(&aesCtx, serverKey.data());

        size_t encryptedSize = AES256EncryptMessage(&aesCtx, msg.data, interruptMessage.c_str(), interruptMessage.length());
        msg.messageLength = (interruptMessage.length() << 16 | (encryptedSize & 0xFFFF));
        
        for (const auto &client : clients) {
            send(client.first, (char *)&msg, sizeof(msg), 0);
        }
    }
}

