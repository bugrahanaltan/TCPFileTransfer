#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>  // For filesystem operations
#include <csignal>     // For signal handling

#ifdef _WIN32
#include <WinSock2.h>  // Windows-specific socket functions
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")  // Link with ws2_32.lib for Windows
#else
#include <sys/socket.h>  // POSIX socket functions
#include <arpa/inet.h>
#include <unistd.h>       // For close() on Unix-like systems
#include <netdb.h>
#endif

const int BUFFER_SIZE = 1024;           // Buffer size for file transfer
const int FILE_AVAILABLE = 200;         // HTTP-like status code for file available
const int FILE_NOT_FOUND = 404;         // HTTP-like status code for file not found

std::atomic<bool> serverRunning(true);  // Flag to control server operation

// Function to handle client connections
void handleClient(SOCKET clientSock) {
    char fileRequested[FILENAME_MAX] = { 0 };  // Buffer to store the file name requested by client
    char bufferFile[BUFFER_SIZE];              // Buffer to store file data
    std::ifstream file;                        // File stream for reading requested files

    while (serverRunning.load()) {
        // Receive file request from client
        memset(fileRequested, 0, FILENAME_MAX);  // Clear the buffer
        int byRecv = recv(clientSock, fileRequested, FILENAME_MAX, 0);
        if (byRecv <= 0) {
            std::cerr << "Error receiving file request or connection closed!" << std::endl;
            break;  // Exit the loop if error or connection closed
        }

        std::cout << "Received file request for: " << fileRequested << std::endl;

        // Sanitize the file path to prevent path traversal attacks
        std::filesystem::path requestedPath(fileRequested);  // Create path object from received string
        std::filesystem::path basePath = std::filesystem::current_path();  // Get current working directory

        requestedPath = std::filesystem::weakly_canonical(requestedPath);  // Resolve any symbolic links and relative paths
        if (requestedPath.string().find(basePath.string()) != 0) {
            std::cerr << "Invalid file path requested!" << std::endl;
            int invalidPath = FILE_NOT_FOUND;
            send(clientSock, (char*)&invalidPath, sizeof(int), 0);  // Notify client of invalid path
            continue;
        }

        // Try to open the requested file
        file.open(requestedPath, std::ios::binary);
        if (file.is_open()) {
            // Send file available status
            if (send(clientSock, (char*)&FILE_AVAILABLE, sizeof(int), 0) <= 0) {
                std::cerr << "Error sending file available status or connection closed!" << std::endl;
                break;
            }

            // Get file size and send to client
            file.seekg(0, std::ios::end);  // Move to end to get file size
            std::int64_t fileSize = file.tellg();  // Get file size
            if (send(clientSock, (char*)&fileSize, sizeof(std::int64_t), 0) <= 0) {
                std::cerr << "Error sending file size or connection closed!" << std::endl;
                break;
            }
            file.seekg(0, std::ios::beg);  // Rewind to start for reading

            // Read and send file data in chunks
            while (file.read(bufferFile, BUFFER_SIZE)) {
                std::streamsize bytesRead = file.gcount();  // Get number of bytes read
                if (send(clientSock, bufferFile, static_cast<int>(bytesRead), 0) <= 0) {
                    std::cerr << "Error sending file data or connection closed!" << std::endl;
                    break;
                }
            }
            // Send any remaining data if file size is not a multiple of BUFFER_SIZE
            if (file.gcount() > 0) {
                if (send(clientSock, bufferFile, static_cast<int>(file.gcount()), 0) <= 0) {
                    std::cerr << "Error sending remaining file data or connection closed!" << std::endl;
                }
            }
            file.close();  // Close the file stream
        }
        else {
            // File not found
            if (send(clientSock, (char*)&FILE_NOT_FOUND, sizeof(int), 0) <= 0) {
                std::cerr << "Error sending file not found status or connection closed!" << std::endl;
                break;
            }
        }
    }

    // Cleanup
    closesocket(clientSock);  // Close the client socket
}

// Signal handler to shut down server gracefully
void shutdownServer(int) {
    serverRunning.store(false);  // Set server running flag to false
    std::cout << "Server is shutting down..." << std::endl;
}

int main() {
#ifdef _WIN32
    // Windows-specific code for initializing Winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);
    if (WSAStartup(ver, &wsData) != 0) {
        std::cerr << "Error starting Winsock!" << std::endl;
        return -1;
    }
#else
    // POSIX-specific code does not require any initialization
#endif

    // Create listener socket
    SOCKET listenerSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenerSock == INVALID_SOCKET) {
        std::cerr << "Error creating listener socket!" << std::endl;
#ifdef _WIN32
        WSACleanup();  // Clean up Winsock
#endif
        return -1;
    }

    // Setup listener address structure
    sockaddr_in listenerHint;
    listenerHint.sin_family = AF_INET;
    listenerHint.sin_port = htons(27015);  // Port number
    listenerHint.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP address

    // Bind socket
    if (bind(listenerSock, (sockaddr*)&listenerHint, sizeof(listenerHint)) == SOCKET_ERROR) {
        std::cerr << "Error binding socket!" << std::endl;
        closesocket(listenerSock);
#ifdef _WIN32
        WSACleanup();  // Clean up Winsock
#endif
        return -1;
    }

    // Listen for incoming connections
    if (listen(listenerSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Error listening on socket!" << std::endl;
        closesocket(listenerSock);
#ifdef _WIN32
        WSACleanup();  // Clean up Winsock
#endif
        return -1;
    }

    std::cout << "Server is listening for incoming connections..." << std::endl;

#ifdef _WIN32
    // Set up signal handling for Windows
    signal(SIGINT, shutdownServer);
#else
    // Set up signal handling for POSIX systems
    struct sigaction sa;
    sa.sa_handler = shutdownServer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
#endif

    std::vector<std::thread> clientThreads;  // Vector to hold client handler threads

    while (serverRunning.load()) {
        sockaddr_in clientHint;
        int clientSize = sizeof(clientHint);
        SOCKET clientSock = accept(listenerSock, (sockaddr*)&clientHint, &clientSize);
        if (clientSock == INVALID_SOCKET) {
            std::cerr << "Error accepting connection!" << std::endl;
            break;
        }

        std::cout << "Connection accepted." << std::endl;

        // Handle client in a new thread
        clientThreads.emplace_back(handleClient, clientSock);
    }

    // Close listener socket
    closesocket(listenerSock);

    // Wait for all client threads to finish
    for (auto& thread : clientThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

#ifdef _WIN32
    WSACleanup();  // Clean up Winsock
#endif

    return 0;
}
