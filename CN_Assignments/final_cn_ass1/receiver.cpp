
#include <iostream>
#include <sstream>
#include <cstring>
#include "error_utils.h"

// Cross-platform socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024

// Cross-platform socket initialization
bool initializeSocket() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed\n";
        return false;
    }
#endif
    return true;
}

// Cross-platform socket cleanup
void cleanupSocket() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Cross-platform socket close
void closeSocket(int socket) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

int main() {
    // Initialize socket system
    if (!initializeSocket()) {
        return 1;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cout << "Socket creation error\n";
        cleanupSocket();
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Get server IP address
    string server_ip;
    cout << "Enter server IP address (use 127.0.0.1 for localhost): ";
    cin >> server_ip;

    // Convert IP address - cross-platform way
    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "Invalid address/ Address not supported\n";
        closeSocket(sock);
        cleanupSocket();
        return 1;
    }

    cout << "\n=== RECEIVER CLIENT STARTED ===" << endl;
    cout << "Connecting to sender at " << server_ip << ":" << PORT << "...\n";

    // Connect to sender
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Connection failed. Make sure the sender is running.\n";
#ifdef _WIN32
        cout << "Windows Firewall might be blocking the connection.\n";
#else
        cout << "Check if the sender is running and firewall settings.\n";
#endif
        closeSocket(sock);
        cleanupSocket();
        return 1;
    }

    cout << "Connected to sender!\n";
    cout << "Waiting for data...\n";

    // Receive data
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    
    if (valread <= 0) {
        cout << "Failed to receive data\n";
        closeSocket(sock);
        cleanupSocket();
        return 1;
    }

    string received(buffer);
    cout << "\n=== DATA RECEIVED ===" << endl;

    // Parse metadata and data
    istringstream ss(received);
    string method, crcType, dataStr;
    
    getline(ss, method, '|');
    if (method == "crc") {
        getline(ss, crcType, '|');
    }
    getline(ss, dataStr);

    vector<int> data = stringToBits(dataStr);
    
    cout << "Method used: " << method << endl;
    if (method == "crc") {
        cout << "CRC type: " << crcType << endl;
    }
    cout << "Received data size: " << data.size() << " bits" << endl;

    // Verify data
    bool result = false;
    string ackMessage;

    if (method == "checksum") {
        result = verifyChecksum(data);
    } else if (method == "crc") {
        vector<int> generator;
        
        if (crcType == "crc8") {
            generator = stringToPolynomial("111010101");
        }
        else if (crcType == "crc10") {
            generator = stringToPolynomial("11000110011");
        }
        else if (crcType == "crc16") {
            generator = stringToPolynomial("11000000000000101");
        }
        else if (crcType == "crc32") {
            generator = stringToPolynomial("100000100110000010001110110110111");
        }
        else {
            cout << "Invalid CRC type received.\n";
            ackMessage = "ERROR: Invalid CRC type";
            send(sock, ackMessage.c_str(), ackMessage.length(), 0);
            closeSocket(sock);
            cleanupSocket();
            return 1;
        }

        result = verifyCRC(data, generator);
    }

    cout << "\n=== VERIFICATION RESULT ===" << endl;
    if (result) {
        cout << "PASS - No Error Detected" << endl;
        cout << "Data integrity verified successfully!" << endl;
        ackMessage = "ACK: Data received successfully - No errors detected";
    } else {
        cout << "FAIL - Error Detected!" << endl;
        cout << "Data corruption detected during transmission!" << endl;
        ackMessage = "NACK: Data received with errors - Retransmission required";
    }

    // Send acknowledgment back to sender
    send(sock, ackMessage.c_str(), ackMessage.length(), 0);
    cout << "\nAcknowledgment sent to sender.\n";

    // Extract original data (remove checksum/CRC bits)
    if (result) {
        cout << "\n=== EXTRACTED DATA ===" << endl;
        size_t originalSize = data.size();
        if (method == "checksum") {
            originalSize = data.size() - 16;
        } else {
            if (crcType == "crc8") originalSize = data.size() - 8;
            else if (crcType == "crc10") originalSize = data.size() - 10;
            else if (crcType == "crc16") originalSize = data.size() - 16;
            else if (crcType == "crc32") originalSize = data.size() - 32;
        }
        
        if (originalSize > 0 && originalSize < data.size()) {
            vector<int> originalData(data.begin(), data.begin() + originalSize);
            cout << "Original message: " << bitsToString(originalData) << endl;
        }
    }

    // Cleanup
    closeSocket(sock);
    cleanupSocket();
    
    cout << "\n=== RECEPTION COMPLETE ===" << endl;
    return 0;
}