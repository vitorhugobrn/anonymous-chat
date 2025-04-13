#include "ChatGUI.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

ChatGUI::ChatGUI() 
    : isLoggedIn(false), 
      showMessageBox(false), 
      chatClient(nullptr) {
    
    memset(nickname, 0, sizeof(nickname));
    memset(pin, 0, sizeof(pin));
    strcpy(ipAddress, "127.0.0.1");
    strcpy(port, "8080");
}

ChatGUI::~ChatGUI() {
    shutdown();
}

void ChatGUI::init(int width, int height, const char* title) {
    InitWindow(width, height, title);
    SetTargetFPS(60);
}

void ChatGUI::run() {
    while (!WindowShouldClose()) {
        if (isLoggedIn) {
            updateMessages();
            drawChat();
        } else {
            drawLoginScreen();
        }
    }
}

void ChatGUI::shutdown() {
    if (chatClient) {
        delete chatClient;
        chatClient = nullptr;
    }

    CloseWindow();
}

void ChatGUI::updateMessages() {
    if (!chatClient) return;
    
    std::vector<DisplayMessage> newMessages = chatClient->getNewMessages();
    
    allMessages.insert(allMessages.end(), newMessages.begin(), newMessages.end());
    
    if (allMessages.size() > MAX_MESSAGES) {
        allMessages.erase(
            allMessages.begin(), 
            allMessages.begin() + (allMessages.size() - MAX_MESSAGES)
        );
    }
}

void ChatGUI::drawLoginScreen() {
    Vector2 mousePos = GetMousePosition();

    // Login panel positioning
    Rectangle loginPanel = {
        ((float)GetScreenWidth() - 400) / 2, 
        ((float)GetScreenHeight() - 320) / 2, 
        400, 320
    };

    // Input field dimensions
    float inputWidth  = 300;
    float inputHeight = 30;
    float spacing     = 50;
    float inputX      = loginPanel.x + (loginPanel.width - inputWidth) / 2;
    float inputY      = loginPanel.y + 50;

    // Edit mode states for textboxes
    static bool nicknameEditMode  = false;
    static bool pinEditMode       = false;
    static bool ipAddressEditMode = false;
    static bool portEditMode      = false;

    // Text box rectangles
    Rectangle nicknameRec  = { inputX, inputY + 10, inputWidth, inputHeight };
    Rectangle pinRec       = { inputX, inputY + spacing + 10, inputWidth, inputHeight }; 
    Rectangle ipAddressRec = { inputX, inputY + spacing * 2 + 10, inputWidth, inputHeight };
    Rectangle portRec      = { inputX, inputY + spacing * 3 + 10, inputWidth, inputHeight };

    // Check mouse clicks on text boxes
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        nicknameEditMode  = CheckCollisionPointRec(mousePos, nicknameRec);
        pinEditMode       = CheckCollisionPointRec(mousePos, pinRec);
        ipAddressEditMode = CheckCollisionPointRec(mousePos, ipAddressRec);
        portEditMode      = CheckCollisionPointRec(mousePos, portRec);
    }

    BeginDrawing();
    ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

    // Draw login panel
    Color panelColor = ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BASE_COLOR_NORMAL)), 0.4f);
    DrawRectangleRec(loginPanel, panelColor);
    DrawRectangleLinesEx(loginPanel, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)));
    
    // Draw header
    Font font = GuiGetFont();
    int fontSize = font.baseSize;
    const char *headerText = "Chat Login";
    Vector2 textSize = MeasureTextEx(font, headerText, fontSize * 1.5f, 1.5f);
    DrawTextEx(
        font, headerText, { loginPanel.x + (loginPanel.width - textSize.x) / 2, loginPanel.y + 20 },
        fontSize * 1.5f, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
    );
    
    // Draw input fields with labels
    DrawTextEx(
        font, "Nickname:", { inputX, inputY },
        fontSize, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
    );
    GuiTextBox(nicknameRec, nickname, 64, nicknameEditMode);
    
    DrawTextEx(
        font, "PIN:", { inputX, inputY + spacing },
        fontSize, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
    );
    GuiTextBox(pinRec, pin, 32, pinEditMode);
    
    DrawTextEx(
        font, "IP Address:", { inputX, inputY + spacing * 2 },
        fontSize, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
    );
    GuiTextBox(ipAddressRec, ipAddress, 64, ipAddressEditMode);
    
    DrawTextEx(
        font, "Port:", { inputX, inputY + spacing * 3 },
        fontSize, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
    );
    GuiTextBox(portRec, port, 16, portEditMode);
    
    // Connect button
    if (GuiButton({ inputX, inputY + spacing * 4, inputWidth, inputHeight * 1.5f }, "Connect")) {
        if (
            strlen(nickname) > 0 && strlen(pin) > 0 && 
            strlen(ipAddress) > 0 && strlen(port) > 0
        ) {
            chatClient = new ChatClient(nickname, pin);
            if (chatClient->connect(ipAddress, atoi(port))) {
                isLoggedIn = true;
                allMessages.clear();
            }
        }
    }
        
    EndDrawing();
}

void ChatGUI::drawChat() {
    if (!chatClient) return;

    bool sessionActive = false;
    for (auto msg : allMessages) {
        if (msg.sender == "Server") {
            if (msg.text.find("Chat session has started") != std::string::npos) {
                sessionActive = true;
            } else if (msg.text.find("Chat session paused") != std::string::npos) {
                sessionActive = false;
            }
        }
    }
    
    // Chat input text box
    Rectangle textboxRec = { 185, 555, 535, 35 };
    static char text[MAX_TEXT_LENGTH] = {0};
    static bool textEditMode = false;
    
    Vector2 mousePos = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        textEditMode = CheckCollisionPointRec(mousePos, textboxRec);
    }
    
    BeginDrawing();
    ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
    
    Rectangle headerRect = {0, 0, 175, 40};
    DrawRectangleRec(headerRect, GetColor(GuiGetStyle(DEFAULT, BASE_COLOR_PRESSED)));

    Rectangle leftPanel = {0, 0, 175, 600};
    Color panelColor = ColorAlpha(GetColor(GuiGetStyle(DEFAULT, BASE_COLOR_NORMAL)), 0.4f);

    DrawRectangleRec(leftPanel, panelColor);
    DrawRectangleLinesEx(leftPanel, 1, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)));
    
    // Draw "Online Users" header text
    Font font = GuiGetFont();
    int fontSize = font.baseSize;
    const char *headerText = "Online Users";
    Vector2 textSize = MeasureTextEx(font, headerText, fontSize, 1.5f);
    DrawTextEx(
        font, headerText, { headerRect.x + (headerRect.width - textSize.x) / 2, headerRect.y + (headerRect.height - textSize.y) / 2 },
        fontSize, 1.5f, WHITE
    );
        
    std::vector<std::string> users = chatClient->getOnlineUsers();

    for (size_t i = 0; i < users.size(); i++) {
        float y = 40 + i * 30;
        
        // Skip if outside visible area
        if (y < 40 || y > 600) 
            continue;
        
        DrawCircle(15, y + 15, 5, GREEN);
                
        DrawTextEx(
            font, users[i].c_str(), { 35, y + 10 },
            fontSize, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
        );
        
        DrawLine(0, y + 30, 175, y + 30, GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL)));
    }
    
    Rectangle connectedRect = { 185, 10, 535, 30 };
    std::string connectedText = "Connected as: " + std::string(nickname);
    DrawTextEx(
        font, connectedText.c_str(), { connectedRect.x, connectedRect.y + 5 },
        fontSize, 1.5f, GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL))
    );
    
    // Logout button
    if (GuiButton({ 730, 10, 60, 30 }, "Logout")) {
        delete chatClient;
        chatClient = nullptr;
        isLoggedIn = false;
        return;
    }

    // Draw chat messages
    float bubbleY = textboxRec.y - BUBBLE_SPACING - 12.5f;
    float maxTextWidth = textboxRec.width - BUBBLE_PADDING * 4 - 20;
    
    for (int i = allMessages.size() - 1; i >= 0; i--) {
        const auto& msg = allMessages[i];
        
        std::vector<std::string> textLines;
        std::string currentLine;
        std::string remainingText = msg.text;
        
        while (!remainingText.empty()) {
            size_t spacePos = remainingText.find(' ');
            std::string word;
            
            if (spacePos == std::string::npos) {
                word = remainingText;
                remainingText.clear();
            } else {
                word = remainingText.substr(0, spacePos + 1);
                remainingText = remainingText.substr(spacePos + 1);
            }
            
            // Check if adding this word would exceed the max width
            Vector2 testSize = MeasureTextEx(font, (currentLine + word).c_str(), fontSize, 1.5f);
            
            if (testSize.x > maxTextWidth && !currentLine.empty()) {
                textLines.push_back(currentLine);
                currentLine = word;
            } else {
                currentLine += word;
            }
        }
        
        if (!currentLine.empty())
            textLines.push_back(currentLine);
        
        // Calculate message bubble dimensions
        float textHeight = textLines.size() * (fontSize + 2);
        Vector2 senderSize = MeasureTextEx(font, msg.sender.c_str(), fontSize, 1.5f);
        
        float maxWidth = senderSize.x;
        for (const auto& line : textLines) {
            Vector2 lineSize = MeasureTextEx(font, line.c_str(), fontSize, 1.5f);
            maxWidth = std::max(maxWidth, lineSize.x);
        }
        
        float bubbleX = msg.isFromMe 
            ? textboxRec.x + 605 - maxWidth  - BUBBLE_PADDING * 2
            : textboxRec.x;

        Rectangle bubbleRect = {
            bubbleX,
            bubbleY - textHeight - BUBBLE_PADDING * 2 - 12.5f,
            maxWidth + BUBBLE_PADDING * 2,
            textHeight + BUBBLE_PADDING * 2 + 12.5f
        };

        if (bubbleRect.y < 50) 
            break;

        // Define colors based on message sender
        Color borderColorPressed = GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_PRESSED));
        Color textColorPressed   = GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_PRESSED));
        Color baseColorPressed   = GetColor(GuiGetStyle(DEFAULT, BASE_COLOR_PRESSED));
        Color textColorNormal    = GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL));
        Color borderColorNormal  = GetColor(GuiGetStyle(DEFAULT, BORDER_COLOR_NORMAL));
        Color baseColorNormal    = GetColor(GuiGetStyle(DEFAULT, BASE_COLOR_NORMAL));

        Color textColor   = msg.isFromMe ? textColorPressed : textColorNormal;
        Color baseColor   = msg.isFromMe ? baseColorPressed : baseColorNormal;
        Color borderColor = msg.isFromMe ? borderColorPressed : borderColorNormal;

        DrawRectangleRec(bubbleRect, baseColor);
        DrawRectangleLinesEx(bubbleRect, 1.0f, borderColor);
        
        DrawTextEx(
            font, msg.sender.c_str(), { bubbleRect.x + BUBBLE_PADDING, bubbleRect.y + BUBBLE_PADDING },
            fontSize, 1.5f, BLACK
        );
        
        float lineY = bubbleRect.y + BUBBLE_PADDING + 12.5f;
        for (const auto &line : textLines) {
            DrawTextEx(
                font, line.c_str(), { bubbleRect.x + BUBBLE_PADDING, lineY },
                fontSize, 1.5f, textColor
            );

            lineY += fontSize + 2;
        }

        bubbleY = bubbleRect.y - BUBBLE_SPACING;
    }

    if (GuiButton({ 730, 555, 60, 35 }, "Send") || GuiTextBox(textboxRec, text, MAX_TEXT_LENGTH, textEditMode)) {
        if (strlen(text) > 0) {
            chatClient->sendMessage(text);
            memset(text, 0, MAX_TEXT_LENGTH);
        }
    }
    
    if (!sessionActive) {
        Rectangle waitRect = { 185, 50, 535, 30 };
        DrawRectangleRec(waitRect, ColorAlpha(RED, 0.2f));
        DrawTextEx(
            font, "Waiting for more users to join the session...", { waitRect.x + 10, waitRect.y + 5 },
            fontSize, 1.5f, RED
        );
    }

    EndDrawing();
}