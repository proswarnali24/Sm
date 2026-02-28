#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 1234

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket failed\n";
        return -1;
    }

    // Bind socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed\n";
        return -1;
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed\n";
        return -1;
    }

    std::cout << "Waiting for connection on port " << PORT << "...\n";

    // Accept connection
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        std::cerr << "Accept failed\n";
        return -1;
    }

    // Open file to write
    std::ofstream outfile("received.txt");
    if (!outfile) {
        std::cerr << "Error creating file\n";
        return -1;
    }

    int valread;
    while ((valread = read(new_socket, buffer, 1024)) > 0) {
        buffer[valread] = '\0';  // Null terminate received string
        outfile << buffer;
        std::cout << "Received: " << buffer << std::endl;
    }

    outfile.close();
    close(new_socket);
    close(server_fd);

    return 0;
}
