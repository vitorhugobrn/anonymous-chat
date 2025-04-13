#pragma once

#include <cstdint>
#include <mutex>
#include <vector>
#include <thread>
#include <string>
#include <atomic>

#include "../../shared/aes256.h"
#include "../../shared/sha256.h"

struct ChatMessage {
    uint32_t messageId;       // Unique message identifier
    uint32_t messageLength;   // Length of the encrypted message
    uint8_t data[1024];       // Encrypted message data
};

struct DisplayMessage {
    std::string sender;
    std::string text;
    bool isFromMe;
    std::time_t timestamp;
};

class ChatClient {
    private:
        std::vector<DisplayMessage> chatHistory;
        std::mutex chatMutex;

        int clientSocket;
        std::string nickname;
        std::string pin;
        std::vector<uint8_t> key;
        AES256 aesCtx;
        
        std::thread receiveThread;
        std::atomic<bool> connected;
        std::vector<std::string> onlineUsers;
        std::mutex onlineUsersMutex;

        void receiveMessages();
        void updateOnlineUsers(const std::string& userListStr);

    public:
        ChatClient(const std::string &nickname, const std::string &pin);
        ~ChatClient();

        bool connect(const std::string &serverIP, int port);
        void disconnect();            

        bool sendMessage(const std::string &message);

        std::vector<DisplayMessage> getNewMessages();
        std::vector<std::string> getOnlineUsers();
};