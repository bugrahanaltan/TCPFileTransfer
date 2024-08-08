#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <WinSock2.h>  // Include Windows-specific Winsock header
#include <WS2tcpip.h>  // Include Windows-specific TCP/IP header
#pragma comment(lib, "ws2_32.lib")  // Link with ws2_32.lib for Winsock library
#else
#include <sys/types.h>    // Include POSIX system types
#include <sys/socket.h>   // Include POSIX socket API
#include <netinet/in.h>   // Include POSIX Internet address structures
#include <arpa/inet.h>    // Include POSIX IP address conversion functions
#include <unistd.h>       // Include POSIX UNIX standard functions
#include <cstring>        // Include POSIX string handling functions
#endif

const int BUFFER_SIZE = 1024;  // Buffer size for file data transfer
const int FILE_AVAILABLE = 200;  // Status code indicating file is available
const int FILE_NOT_FOUND = 404;  // Status code indicating file was not found

std::atomic<bool> clientRunning(true);  // Flag to control the client's running state

// Function to initialize Winsock for Windows systems
#ifdef _WIN32
void initializeWinsock() {
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);
    if (WSAStartup(ver, &wsData) != 0) {
        std::cerr << "Error starting Winsock!" << std::endl;
        exit(-1);  // Exit if Winsock initialization fails
    }
}
#else
// Function to initialize network resources for POSIX systems
void initializeWinsock() {
    // No action needed for POSIX systems
}
#endif

// Function to clean up Winsock resources for Windows systems
#ifdef _WIN32
void cleanupWinsock() {
    WSACleanup();  // Clean up Winsock resources
}
#else
// Function to clean up network resources for POSIX systems
void cleanupWinsock() {
    // No action needed for POSIX systems
}
#endif

// Function to receive a file from the server
void receiveFile(int clientSock, const std::string& fileRequested) {
    // Send file request to the server
    if (send(clientSock, fileRequested.c_str(), fileRequested.size() + 1, 0) <= 0) {
        std::cerr << "Error sending file request or connection closed!" << std::endl;
        clientRunning.store(false);  // Stop client if sending fails
        return;
    }

    // Receive file availability code from the server
    int codeAvailable = 0;
    if (recv(clientSock, reinterpret_cast<char*>(&codeAvailable), sizeof(codeAvailable), 0) <= 0) {
        std::cerr << "Error receiving file availability code or connection closed!" << std::endl;
        clientRunning.store(false);  // Stop client if receiving fails
        return;
    }

    // If file is available, proceed to receive the file
    if (codeAvailable == FILE_AVAILABLE) {
        std::int64_t fileRequestedSize = 0;
        if (recv(clientSock, reinterpret_cast<char*>(&fileRequestedSize), sizeof(fileRequestedSize), 0) <= 0) {
            std::cerr << "Error receiving file size or connection closed!" << std::endl;
            clientRunning.store(false);  // Stop client if receiving file size fails
            return;
        }

        // Open file for writing
        std::ofstream file(fileRequested, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            std::cerr << "Error opening file for writing!" << std::endl;
            clientRunning.store(false);  // Stop client if file cannot be opened
            return;
        }

        // Buffer to receive file data
        std::vector<char> bufferFile(BUFFER_SIZE);
        std::int64_t fileDownloaded = 0;

        // Receive file data in chunks
        while (fileDownloaded < fileRequestedSize) {
            int bytesReceived = recv(clientSock, bufferFile.data(), BUFFER_SIZE, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Error receiving file data or connection closed!" << std::endl;
                clientRunning.store(false);  // Stop client if receiving file data fails
                break;
            }

            // Write received data to the file
            file.write(bufferFile.data(), bytesReceived);
            fileDownloaded += bytesReceived;
        }

        file.close();  // Close the file after writing
        std::cout << "File received and saved as: " << fileRequested << std::endl;
    }
    else {
        std::cout << "File not found on server!" << std::endl;
    }
}

int main() {
    // Initialize Winsock for Windows systems
    initializeWinsock();

    // Create socket for communication
#ifdef _WIN32
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Error creating socket! " << WSAGetLastError() << std::endl;
        cleanupWinsock();
        return -1;
    }
#else
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) {
        std::cerr << "Error creating socket!" << std::endl;
        cleanupWinsock();
        return -1;
    }
#endif

    // Get server address from user input
    std::string serverAddress;
    std::cout << "Enter server address: ";
    std::getline(std::cin, serverAddress);

    // Setup server address structure
    sockaddr_in hint = {};
    hint.sin_family = AF_INET;
    hint.sin_port = htons(27015);  // Port number for communication
    if (inet_pton(AF_INET, serverAddress.c_str(), &hint.sin_addr) <= 0) {
        std::cerr << "Invalid server address!" << std::endl;
#ifdef _WIN32
        closesocket(clientSock);  // Close socket for Windows systems
#else
        close(clientSock);  // Close socket for POSIX systems
#endif
        cleanupWinsock();
        return -1;
    }

    // Connect to the server
#ifdef _WIN32
    if (connect(clientSock, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR) {
        std::cerr << "Error connecting to server! " << WSAGetLastError() << std::endl;
        closesocket(clientSock);  // Close socket for Windows systems
#else
    if (connect(clientSock, (sockaddr*)&hint, sizeof(hint)) < 0) {
        std::cerr << "Error connecting to server!" << std::endl;
        close(clientSock);  // Close socket for POSIX systems
#endif
        cleanupWinsock();
        return -1;
    }

    std::cout << "Connected to server. You can now request files." << std::endl;

    // Main loop to request files from the server
    while (clientRunning.load()) {
        std::string fileRequested;
        std::cout << "Enter file name (or 'exit' to quit): ";
        std::getline(std::cin, fileRequested);

        if (fileRequested == "exit") {
            clientRunning.store(false);  // Stop client if user types 'exit'
            break;
        }

        receiveFile(clientSock, fileRequested);  // Request and receive the file from the server
    }

    // Cleanup resources
#ifdef _WIN32
    closesocket(clientSock);  // Close socket for Windows systems
#else
    close(clientSock);  // Close socket for POSIX systems
#endif
    cleanupWinsock();  // Clean up Winsock resources

    std::cout << "Client terminated." << std::endl;
    return 0;
}
