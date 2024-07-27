# TCP File Transfer

This project provides a simple TCP file transfer server and client using Winsock. The server sends requested files to the client upon request.

## Overview

- **Server (`server.cpp`)**: Listens for incoming client connections, handles file requests, and sends requested files to the client.
- **Client (`client.cpp`)**: Connects to the server, requests files by name, and saves the received files locally.

## Requirements

- Windows operating system
- Visual Studio or another C++ compiler that supports Winsock

## Building

To build the server and client applications, use the provided `server.cpp` and `client.cpp` source files. Ensure that you link against the `ws2_32.lib` library.

### Build Instructions (Visual Studio)

1. Open Visual Studio.
2. Create a new project or open an existing one.
3. Add `server.cpp` to the project for the server application and `client.cpp` for the client application.
4. Configure the project to link against `ws2_32.lib` (typically found under project properties -> Linker -> Input).
5. Build the project.

## Usage

### Running the Server

1. Open a command prompt.
2. Navigate to the directory where the server executable is located.
3. Run the server executable. For example:
   ```sh
   server.exe
