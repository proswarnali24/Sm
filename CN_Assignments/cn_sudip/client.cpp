#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define PAYLOAD_SIZE 46
#define TIMEOUT_SEC 2

struct Frame {
    int seqNo;
    int length;
    char data[PAYLOAD_SIZE];
    unsigned short checksum;
};

unsigned short computeChecksum(const char* data, int size) {
    unsigned short sum = 0;
    for (int i = 0; i < size; i++) sum += (unsigned char)data[i];
    return sum;
}

int recv_all(int s, char* buf, int len) {
    int total = 0;
    while (total < len) {
        int r = recv(s, buf + total, len - total, 0);
        if (r <= 0) return r;
        total += r;
    }
    return total;
}

int send_all(int s, const char* buf, int len) {
    int total = 0;
    while (total < len) {
        int r = send(s, buf + total, len - total, 0);
        if (r <= 0) return r;
        total += r;
    }
    return total;
}

std::vector<std::string> readFileFrames(const std::string &filename) {
    std::ifstream fin(filename, std::ios::binary);
    std::vector<std::string> frames;
    if (!fin) {
        std::cerr << "Error: could not open " << filename << "\n";
        return frames;
    }
    char buf[PAYLOAD_SIZE];
    while (fin.read(buf, PAYLOAD_SIZE) || fin.gcount() > 0) {
        frames.emplace_back(buf, fin.gcount());
    }
    return frames;
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    std::cout << "Connected to server.\n";

    auto payloads = readFileFrames("input.txt");
    if (payloads.empty()) {
        std::cerr << "No data to send.\n";
        close(sock);
        return 1;
    }

    int TOTAL = payloads.size();
    for (int i = 0; i < TOTAL; ) {
        Frame f{};
        f.seqNo = i;
        f.length = payloads[i].size();
        memcpy(f.data, payloads[i].data(), f.length);
        f.checksum = computeChecksum(f.data, f.length);

        // send frame
        if (send_all(sock, (char*)&f, sizeof(f)) <= 0) {
            perror("send");
            break;
        }
        std::cout << "Sent frame " << i << "\n";

        // wait for ACK
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval tv{TIMEOUT_SEC, 0};
        int ready = select(sock+1, &readfds, NULL, NULL, &tv);

        if (ready > 0 && FD_ISSET(sock, &readfds)) {
            int ack;
            int r = recv_all(sock, (char*)&ack, sizeof(ack));
            if (r <= 0) {
                std::cerr << "recv error or server closed.\n";
                break;
            }
            if (ack == i) {
                std::cout << "ACK received for frame " << i << "\n";
                i++; // move to next frame
            }
        } else {
            std::cout << "Timeout! Resending frame " << i << "\n";
            // loop continues with same i
        }
    }

    std::cout << "All frames sent and acknowledged.\n";
    close(sock);
    return 0;
}
