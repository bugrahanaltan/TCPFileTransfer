#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

int main() {
    // Initialize Winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);

    if (WSAStartup(ver, &wsData) != 0) {
        std::cerr << "Error starting Winsock!" << std::endl;
        return -1;
    }

    // Create listener socket
    SOCKET listenerSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenerSock == INVALID_SOCKET) {
        std::cerr << "Error creating listener socket! " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }

    // Setup listener address structure
    sockaddr_in listenerHint;
    listenerHint.sin_family = AF_INET;
    listenerHint.sin_port = htons(27015);
    listenerHint.sin_addr.S_un.S_addr = INADDR_ANY;

    // Bind socket
    if (bind(listenerSock, (sockaddr*)&listenerHint, sizeof(listenerHint)) == SOCKET_ERROR) {
        std::cerr << "Error binding socket! " << WSAGetLastError() << std::endl;
        closesocket(listenerSock);
        WSACleanup();
        return -1;
    }

    // Listen for incoming connections
    if (listen(listenerSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Error listening on socket! " << WSAGetLastError() << std::endl;
        closesocket(listenerSock);
        WSACleanup();
        return -1;
    }

    std::cout << "Server is listening for incoming connections..." << std::endl;

    // Accept incoming connection
    sockaddr_in clientHint;
    int clientSize = sizeof(clientHint);
    SOCKET clientSock = accept(listenerSock, (sockaddr*)&clientHint, &clientSize);
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Error accepting connection! " << WSAGetLastError() << std::endl;
        closesocket(listenerSock);
        WSACleanup();
        return -1;
    }

    std::cout << "Connection accepted." << std::endl;

    // Get client info
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];

    if (getnameinfo((sockaddr*)&clientHint, sizeof(clientHint), host, NI_MAXHOST, serv, NI_MAXSERV, 0) == 0) {
        std::cout << "Host: " << host << " connected on port: " << serv << std::endl;
    }
    else {
        inet_ntop(AF_INET, &clientHint.sin_addr, host, NI_MAXHOST);
        std::cout << "Host: " << host << " connected on port: " << ntohs(clientHint.sin_port) << std::endl;
    }

    // Close listener socket as it's no longer needed
    closesocket(listenerSock);

    // Send welcome message to client
    const char* welcomeMsg = "Welcome to file server.";
    int sendResult = send(clientSock, welcomeMsg, strlen(welcomeMsg), 0);
    if (sendResult <= 0) {
        std::cerr << "Error sending welcome message or connection closed!" << std::endl;
        closesocket(clientSock);
        WSACleanup();
        return -1;
    }

    std::cout << "Welcome message sent: " << welcomeMsg << std::endl;

    // Main loop to handle file requests
    bool clientClose = false;
    char fileRequested[FILENAME_MAX];
    const int fileAvailable = 200;
    const int fileNotfound = 404;
    const int BUFFER_SIZE = 1024;
    char bufferFile[BUFFER_SIZE];
    std::ifstream file;

    do {
        // Receive file request from client
        memset(fileRequested, 0, FILENAME_MAX);
        int byRecv = recv(clientSock, fileRequested, FILENAME_MAX, 0);
        if (byRecv <= 0) {
            std::cerr << "Error receiving file request or connection closed!" << std::endl;
            clientClose = true;
            break;
        }

        std::cout << "Received file request for: " << fileRequested << std::endl;

        // Try to open the requested file
        file.open(fileRequested, std::ios::binary);
        if (file.is_open()) {
            // Send file available status
            if (send(clientSock, (char*)&fileAvailable, sizeof(int), 0) <= 0) {
                std::cerr << "Error sending file available status or connection closed!" << std::endl;
                clientClose = true;
                break;
            }

            // Get file size and send to client
            file.seekg(0, std::ios::end);
            long fileSize = file.tellg();
            if (send(clientSock, (char*)&fileSize, sizeof(long), 0) <= 0) {
                std::cerr << "Error sending file size or connection closed!" << std::endl;
                clientClose = true;
                break;
            }
            file.seekg(0, std::ios::beg);

            // Read and send file data
            while (file.read(bufferFile, BUFFER_SIZE)) {
                int bytesRead = file.gcount();
                if (send(clientSock, bufferFile, bytesRead, 0) <= 0) {
                    std::cerr << "Error sending file data or connection closed!" << std::endl;
                    clientClose = true;
                    break;
                }
            }
            // Send any remaining data in case file size is not a multiple of BUFFER_SIZE
            if (file.gcount() > 0) {
                if (send(clientSock, bufferFile, file.gcount(), 0) <= 0) {
                    std::cerr << "Error sending remaining file data or connection closed!" << std::endl;
                    clientClose = true;
                }
            }
            file.close();
        }
        else {
            // File not found
            if (send(clientSock, (char*)&fileNotfound, sizeof(int), 0) <= 0) {
                std::cerr << "Error sending file not found status or connection closed!" << std::endl;
                clientClose = true;
                break;
            }
        }
    } while (!clientClose);

    // Cleanup
    closesocket(clientSock);
    WSACleanup();
    return 0;
}
