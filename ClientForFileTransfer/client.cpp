#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
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

    // Create socket
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Error creating socket! " << WSAGetLastError() << std::endl;
        WSACleanup();
        return -1;
    }

    // Get server address from user
    char serverAddress[NI_MAXHOST];
    memset(serverAddress, 0, NI_MAXHOST);

    std::cout << "Enter server address: ";
    std::cin.getline(serverAddress, NI_MAXHOST);

    // Setup server address structure
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(27015);
    inet_pton(AF_INET, serverAddress, &hint.sin_addr);

    // Connect to server
    if (connect(clientSock, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        std::cerr << "Error connecting to server! " << WSAGetLastError() << std::endl;
        closesocket(clientSock);
        WSACleanup();
        return -1;
    }

    // Receive welcome message from server
    char welcomeMsg[255];
    int byRecv = recv(clientSock, welcomeMsg, sizeof(welcomeMsg), 0);
    if (byRecv <= 0) {
        std::cerr << "Error receiving welcome message or connection closed!" << std::endl;
        closesocket(clientSock);
        WSACleanup();
        return -1;
    }

    // Main loop to request files
    bool clientClose = false;
    const int BUFFER_SIZE = 1024;
    char bufferFile[BUFFER_SIZE];
    char fileRequested[FILENAME_MAX];
    std::ofstream file;

    do {
        // Get file name from user
        memset(fileRequested, 0, FILENAME_MAX);
        std::cout << "Enter file name: " << std::endl;
        std::cin.getline(fileRequested, FILENAME_MAX);

        // Send file request to server
        byRecv = send(clientSock, fileRequested, FILENAME_MAX, 0);
        if (byRecv <= 0) {
            std::cerr << "Error sending file request or connection closed!" << std::endl;
            clientClose = true;
            break;
        }

        // Receive file availability code
        int codeAvailable;
        byRecv = recv(clientSock, (char*)&codeAvailable, sizeof(int), 0);
        if (byRecv <= 0) {
            std::cerr << "Error receiving file availability code or connection closed!" << std::endl;
            clientClose = true;
            break;
        }

        // If file is available, receive the file
        if (codeAvailable == 200) {
            long fileRequestedSize;
            byRecv = recv(clientSock, (char*)&fileRequestedSize, sizeof(long), 0);
            if (byRecv <= 0) {
                std::cerr << "Error receiving file size or connection closed!" << std::endl;
                clientClose = true;
                break;
            }

            file.open(fileRequested, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                std::cerr << "Error opening file for writing!" << std::endl;
                clientClose = true;
                break;
            }

            int fileDownloaded = 0;
            while (fileDownloaded < fileRequestedSize) {
                byRecv = recv(clientSock, bufferFile, BUFFER_SIZE, 0);
                if (byRecv <= 0) {
                    std::cerr << "Error receiving file data or connection closed!" << std::endl;
                    clientClose = true;
                    break;
                }

                file.write(bufferFile, byRecv);
                fileDownloaded += byRecv;
            }

            file.close();
        }
        else {
            std::cout << "Can't open file or file not found!" << std::endl;
        }
    } while (!clientClose);

    // Cleanup
    closesocket(clientSock);
    WSACleanup();
    return 0;
}
