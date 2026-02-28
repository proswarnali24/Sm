#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 8888
#define FRAME_SIZE 46

struct Frame {
    int seqNo;
    int length;
    char data[FRAME_SIZE];
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

int main() {
    srand((unsigned)time(NULL));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, 3) < 0) { perror("listen"); return 1; }

    // take inputs from user
    int error_percent, user_delay;
    std::cout << "Enter error percentage (0–100): ";
    std::cin >> error_percent;
    std::cout << "Enter fixed delay per frame (ms): ";
    std::cin >> user_delay;

    std::cout << "Waiting for connection on port " << PORT << "...\n";
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) { perror("accept"); return 1; }
    std::cout << "Client connected.\n";

    std::ofstream outfile("output.txt", std::ios::binary);
    if (!outfile) { std::cerr << "Cannot open output.txt\n"; return 1; }

    std::unordered_map<int, std::vector<char>> buffer;
    int expectedSeq = 0;
    long long total_delay = 0;   // cumulative delay
    int frame_count = 0;

    while (true) {
        Frame frame;
        int r = recv_all(client_fd, (char*)&frame, sizeof(frame));
        if (r <= 0) {
            std::cout << "Connection closed.\n";
            break;
        }

        // apply user-defined delay
        usleep(user_delay * 1000);
        total_delay += user_delay;
        frame_count++;

        // simulate error
        int roll = rand() % 100;
        if (roll < error_percent) {
            frame.data[0] ^= 0xFF; // flip some bits
            std::cout << "Frame " << frame.seqNo << " corrupted intentionally\n";
        }

        if (frame.length < 0 || frame.length > FRAME_SIZE) continue;
        unsigned short cksum = computeChecksum(frame.data, frame.length);
        if (cksum != frame.checksum) {
            std::cout << "Checksum mismatch seq=" << frame.seqNo << " (dropped)\n";
            continue; // drop frame
        }

        int ack = frame.seqNo;
        send_all(client_fd, (char*)&ack, sizeof(ack));
        std::cout << "Received frame " << frame.seqNo << " len=" << frame.length << " -> ACK " << ack << "\n";

        buffer[frame.seqNo] = std::vector<char>(frame.data, frame.data + frame.length);
        while (buffer.count(expectedSeq)) {
            auto &v = buffer[expectedSeq];
            outfile.write(v.data(), v.size());
            buffer.erase(expectedSeq);
            expectedSeq++;
        }
    }

    std::cout << "Total delay introduced: " << total_delay << " ms\n";
    if (frame_count > 0)
        std::cout << "Average delay per frame: " << (total_delay / frame_count) << " ms\n";

    outfile.close();
    close(client_fd);
    close(server_fd);
    return 0;
}
