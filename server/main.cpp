#include "src/ChatServer.h"

#include <iostream>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cout << "Usage: <port> <pin>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string pin = argv[2];

    ChatServer server(port, pin);
    if (!server.start()) {
        std::cerr << "Failed to Start Server" << std::endl;
        return 1;
    }

    std::cout << "Server started with PIN: " << pin << std::endl;
    std::cout << "Press Enter to Stop the Server" << std::endl;
    std::cin.get();

    server.stop();
    return 0;
}