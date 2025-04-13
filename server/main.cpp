#include "src/ChatServer.h"

#include <iostream>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cout << "Usage: <port> <pin> <required_users>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string pin = argv[2];
    int requiredUsers = std::stoi(argv[3]);

    ChatServer server(port, pin, requiredUsers);
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