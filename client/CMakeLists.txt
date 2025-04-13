find_package(raylib REQUIRED)

add_executable(ChatClient
    main.cpp
    src/ChatClient.cpp
    src/ChatClient.h
    src/ChatGUI.cpp
    src/ChatGUI.h
)

target_include_directories(ChatClient PRIVATE include ../shared)
target_link_libraries(ChatClient PRIVATE raylib OpenSSL::SSL OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(ChatClient PRIVATE ws2_32)
endif()