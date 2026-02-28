
#include <iostream>
#include <fstream>
#include <cstring>
#include "error_utils.h"

// Cross-platform socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

// Cross-platform socket close
void closeSocket(int socket) {
    #ifdef _WIN32
        closesocket(socket);
    #else
        close(socket);
    #endif
}

// Initialize Winsock on Windows
bool initializeSocket() {
    #ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            cout << "WSAStartup failed: " << result << endl;
            return false;
        }
    #endif
    return true;
}

// Cleanup Winsock on Windows
void cleanupSocket() {
    #ifdef _WIN32
        WSACleanup();
    #endif
}

int main() {
    // Initialize socket system
    if (!initializeSocket()) {
        return 1;
    }

    string filename, method, errorType;
    cout << "Enter input file name: ";
    cin >> filename;

    ifstream infile(filename);
    if (!infile.is_open()) {
        cout << "Error opening file.\n";
        cleanupSocket();
        return 1;
    }

    string input;
    infile >> input;
    infile.close();

    vector<int> data = stringToBits(input);

    cout << "Choose method (checksum/crc): ";
    cin >> method;
    cout << "Error type (single/double/odd/burst/none): ";
    cin >> errorType;

    vector<int> codeword;
    string metadata = method + "|";  // Store method for receiver
    
    if (method == "checksum") {
        vector<int> checksum = computeChecksum(data);
        codeword = data;
        codeword.insert(codeword.end(), checksum.begin(), checksum.end());
    } else if (method == "crc") {
        string crcType;
        cout << "Enter CRC type (crc8/crc10/crc16/crc32): ";
        cin >> crcType;
        metadata += crcType;  // Add CRC type to metadata
        
        vector<int> generator;
        if (crcType == "crc8") generator = stringToPolynomial("111010101");  
        else if (crcType == "crc10") generator = stringToPolynomial("11000110011");  
        else if (crcType == "crc16") generator = stringToPolynomial("11000000000000101");  
        else if (crcType == "crc32") generator = stringToPolynomial("100000100110000010001110110110111"); 
        else {
            cout << "Invalid CRC type.\n";
            cleanupSocket();
            return 1;
        }

        vector<int> crc = computeCRC(data, generator);
        codeword = data;
        codeword.insert(codeword.end(), crc.begin(), crc.end());
    } else {
        cout << "Invalid method.\n";
        cleanupSocket();
        return 1;
    }

    if (errorType != "none") {
        injectError(codeword, errorType);
        cout << "Error injected: " << errorType << "\n";
    }

    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        cout << "Socket creation failed\n";
        cleanupSocket();
        return 1;
    }

    // Allow socket reuse - cross-platform way
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cout << "Setsockopt SO_REUSEADDR failed\n";
        closeSocket(server_fd);
        cleanupSocket();
        return 1;
    }

    // Set SO_REUSEPORT on Unix systems (not available on Windows)
    #ifndef _WIN32
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            cout << "Setsockopt SO_REUSEPORT failed (this is optional)\n";
            // Continue execution as this is not critical
        }
    #endif

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket
    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        cout << "Bind failed. Port might be in use.\n";
        closeSocket(server_fd);
        cleanupSocket();
        return 1;
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        cout << "Listen failed\n";
        closeSocket(server_fd);
        cleanupSocket();
        return 1;
    }

    cout << "\n=== SENDER SERVER STARTED ===" << endl;
    cout << "Waiting for receiver to connect on port " << PORT << "...\n";

    // Accept connection
    socklen_t addrlen = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket < 0) {
        cout << "Accept failed\n";
        closeSocket(server_fd);
        cleanupSocket();
        return 1;
    }

    cout << "Receiver connected!\n";

    // Prepare message: metadata|data
    string message = metadata + "|" + bitsToString(codeword);
    
    // Send data
    ssize_t sent = send(new_socket, message.c_str(), message.length(), 0);
    if (sent < 0) {
        cout << "Send failed\n";
    } else {
        cout << "\n=== TRANSMISSION DETAILS ===" << endl;
        cout << "Method: " << method << endl;
        cout << "Original data size: " << data.size() << " bits" << endl;
        cout << "Transmitted data size: " << codeword.size() << " bits" << endl;
        cout << "Data successfully sent to receiver.\n";
    }

    // Wait for acknowledgment
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread = recv(new_socket, buffer, BUFFER_SIZE, 0);
    if (valread > 0) {
        cout << "\nReceiver response: " << buffer << endl;
    }

    // Cleanup
    closeSocket(new_socket);
    closeSocket(server_fd);
    cleanupSocket();
    
    cout << "\n=== TRANSMISSION COMPLETE ===" << endl;
    return 0;
}