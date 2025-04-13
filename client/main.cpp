#define RAYGUI_IMPLEMENTATION
#include "./src/ChatGUI.h"

int main() {
    ChatGUI app;
    app.init(800, 600, "Anonymous Chat");
    app.run();
    
    return 0;
}