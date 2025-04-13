#pragma once 

#include "raygui.h"
#include <string>
#include <vector>

#include "./ChatClient.h"

#define MAX_MESSAGES 100
#define MAX_TEXT_LENGTH 1024
#define BUBBLE_PADDING 12
#define BUBBLE_SPACING 8

class ChatGUI {
    public:
        ChatGUI();
        ~ChatGUI();

        void init(int width, int height, const char* title);
        void run();        
        void shutdown();

    private:
        // Application state
        bool isLoggedIn;
        bool showMessageBox;
        std::string messageBoxContent;
        
        // User credentials
        char nickname[64];
        char pin[32];
        char ipAddress[64];
        char port[16];
        
        ChatClient *chatClient;        
        std::vector<DisplayMessage> allMessages;
        
        void updateMessages();
        
        void drawLoginScreen();
        void drawChat();
};