cmake_minimum_required(VERSION 3.15)
project(ChatApp)

set(CMAKE_CXX_STANDARD 17)

# Shared include directory
include_directories(shared)

# Platform-specific flags
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-maes)
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Add subdirectories
add_subdirectory(server)
add_subdirectory(client)