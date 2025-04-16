#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

#include <thread>

#include "ChatClient.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
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

ChatClient::ChatClient(const std::string& nickname, const std::string& pin) 
    : clientSocket(-1), nickname(nickname), pin(pin), connected(false) {
    // Generate key from pin
    key = generateSHA256Bytes(pin.c_str(), pin.length());
    AES256Init(&aesCtx, key.data());
}

ChatClient::~ChatClient() {
    disconnect();
}

std::vector<DisplayMessage> ChatClient::getNewMessages() {
    std::lock_guard<std::mutex> lock(chatMutex);
    std::vector<DisplayMessage> result = chatHistory;
    chatHistory.clear();
    return result;
}

bool ChatClient::connect(const std::string& serverIP, int port) {
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
    #endif

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return false;
    }
    
    // Set up server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address" << std::endl;
        CLOSE_SOCKET(clientSocket);
        return false;
    }
    
    // Connect to server
    if (::connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        CLOSE_SOCKET(clientSocket);
        return false;
    }
    
    // Send authentication info (nickname:pin)
    std::string authInfo = nickname + ":" + pin;
    send(clientSocket, authInfo.c_str(), authInfo.length(), 0);
    
    // Wait for server response
    char buffer[1024];
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        std::cerr << "Connection closed by server" << std::endl;
        CLOSE_SOCKET(clientSocket);
        return false;
    }
    
    buffer[bytesRead] = '\0';
    if (std::string(buffer) != "Connected") {
        std::cerr << "Authentication failed: " << buffer << std::endl;
        CLOSE_SOCKET(clientSocket);
        return false;
    }
    
    connected = true;
    
    receiveThread = std::thread(&ChatClient::receiveMessages, this);
    receiveThread.detach();
    
    std::cout << "Connected to server as " << nickname << std::endl;
    return true;
}

void ChatClient::disconnect() {
    connected = false;
    if (clientSocket >= 0) {
        CLOSE_SOCKET(clientSocket);
        clientSocket = -1;
    }
}

bool ChatClient::sendMessage(const std::string& message) {
    if (!connected) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }
    
    std::time_t now = std::time(nullptr);
    std::string timestamp = std::ctime(&now);
    timestamp.pop_back(); // Remove new line
        
    // Create message structure
    ChatMessage msg;
    static uint32_t messageId = 1;
    msg.messageId = messageId++;
    msg.opcode = MessageOpcode::CHAT_MESSAGE;
    
    if (message.length() > sizeof(msg.data) / 2) {
        std::cerr << "Message too long!" << std::endl;
        return false;
    }
    
    size_t encryptedSize = AES256EncryptMessage(&aesCtx, msg.data, message.c_str(), message.length());
    
    // Update the message length to include original message length AND encrypted size
    // This allows the receiver to know both values
    msg.messageLength = (message.length() << 16) | (encryptedSize & 0xFFFF);
    
    { // Add to local chat history as a message from current user
        std::lock_guard<std::mutex> lock(chatMutex);
        chatHistory.push_back({nickname, message, true, now});
    }
    
    if (send(clientSocket, (char *)&msg, sizeof(msg), 0) < 0) {
        std::cerr << "Error sending message" << std::endl;
        return false;
    }
    
    return true;
}

std::vector<std::string> ChatClient::getOnlineUsers() {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    return onlineUsers;
}

void ChatClient::receiveMessages() {
    while (connected) {
        ChatMessage msg;
        int bytesRead = recv(clientSocket, (char *)&msg, sizeof(msg), 0);
        
        if (bytesRead <= 0) {
            if (connected) {
                std::cerr << "Connection closed by server" << std::endl;
                connected = false;
            }
            break;
        }
        
        // Extract original message length and encrypted size from messageLength field
        uint32_t originalLength = (msg.messageLength >> 16) & 0xFFFF;
        uint32_t encryptedSize = msg.messageLength & 0xFFFF;
        
        std::vector<uint8_t> decryptedData(encryptedSize + 1, 0); // +1 for null terminator
        AES256DecryptMessage(&aesCtx, decryptedData.data(), msg.data, encryptedSize);
        decryptedData[originalLength] = '\0';
        
        std::string decryptedMessage(reinterpret_cast<char*>(decryptedData.data()), originalLength);
        
        switch (msg.opcode) {
            case MessageOpcode::USER_LIST:
                updateOnlineUsers(decryptedMessage);
                break;
                
            case MessageOpcode::SESSION_START: {
                std::lock_guard<std::mutex> lock(chatMutex);
                chatHistory.push_back({ "Server", "Chat session has started! You can now send messages.", false, std::time(nullptr) });
                break;
            }
                
            case MessageOpcode::SESSION_INTERRUPT: {
                std::lock_guard<std::mutex> lock(chatMutex);
                chatHistory.push_back({ "Server", "Chat session paused. Waiting for more users to join.", false, std::time(nullptr) });
                break;
            }
                
            case MessageOpcode::SYSTEM_MESSAGE: {
                std::lock_guard<std::mutex> lock(chatMutex);
                chatHistory.push_back({"Server", decryptedMessage, false, std::time(nullptr)});
                break;
            }
                
            case MessageOpcode::CHAT_MESSAGE:
            default: {
                std::string sender = "Anonymous";
                std::string messageText = decryptedMessage;
                
                std::lock_guard<std::mutex> lock(chatMutex);
                chatHistory.push_back({ sender, messageText, false, std::time(nullptr) });
                break;
            }
        }
    }
}

void ChatClient::updateOnlineUsers(const std::string& userListStr) {
    std::lock_guard<std::mutex> lock(onlineUsersMutex);
    onlineUsers.clear();
    
    size_t start = 0;
    size_t end = userListStr.find(',');
    
    while (end != std::string::npos) {
        onlineUsers.push_back(userListStr.substr(start, end - start));
        start = end + 1;
        end = userListStr.find(',', start);
    }
    
    if (start < userListStr.length()) {
        onlineUsers.push_back(userListStr.substr(start));
    }
}