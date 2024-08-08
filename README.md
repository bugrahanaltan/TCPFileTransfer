# TCP File Transfer

This project provides a simple TCP file transfer server and client using Winsock for Windows and POSIX socket APIs for Unix-like systems. The server sends requested files to the client upon request.

## Overview

- **Server (`server.cpp`)**: Listens for incoming client connections, handles file requests, and sends requested files to the client.
- **Client (`client.cpp`)**: Connects to the server, requests files by name, and saves the received files locally.

## Requirements

- **Windows**: Visual Studio or another C++ compiler that supports Winsock.
- **POSIX Systems**: A POSIX-compliant system (Linux, macOS) with a C++ compiler.
- **C++17 or later**: The code uses features from C++17, such as `std::filesystem` and `std::atomic`, which are not available in earlier versions of C++.

## Building

To build the server and client applications, use the provided `server.cpp` and `client.cpp` source files. Ensure that you link against the `ws2_32.lib` library on Windows.

### Build Instructions (Visual Studio)

1. Open Visual Studio.
2. Create a new project or open an existing one.
3. Add `server.cpp` to the project for the server application and `client.cpp` for the client application.
4. Configure the project to link against `ws2_32.lib` (typically found under project properties -> Linker -> Input).
5. Build the project.

### Build Instructions (POSIX Systems)

1. Open a terminal.
2. Navigate to the directory containing `server.cpp` and `client.cpp`.
3. Compile the source files using a C++ compiler. For example:
   ```sh
   g++ -o server server.cpp
   g++ -o client client.cpp
