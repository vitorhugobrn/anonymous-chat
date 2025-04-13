add_executable(ChatServer
    main.cpp
    src/ChatServer.cpp
    src/ChatServer.h
)

target_include_directories(ChatServer PRIVATE ../shared)
target_link_libraries(ChatServer PRIVATE OpenSSL::SSL OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(ChatServer PRIVATE ws2_32)
endif()