// client_sr.cpp (POSIX version)
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define PAYLOAD_SIZE 46
#define TIMEOUT_MS 2000

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

uint64_t now_ms() {
    timeval tv; gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec*1000 + tv.tv_usec/1000;
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

    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) { perror("connect"); return 1; }
    std::cout << "Connected to server.\n";

    int WIN_SIZE;
    std::cout << "Window size: "; std::cin >> WIN_SIZE;

    auto payloads = readFileFrames("input.txt");
    int TOTAL = payloads.size();

    std::vector<char> acked(TOTAL, 0), sent(TOTAL, 0);
    std::vector<uint64_t> sent_time(TOTAL, 0);
    int base = 0, nextSeq = 0;

    while (base < TOTAL) {
        while (nextSeq < base + WIN_SIZE && nextSeq < TOTAL) {
            if (!sent[nextSeq]) {
                Frame f{};
                f.seqNo = nextSeq;
                f.length = payloads[nextSeq].size();
                memcpy(f.data, payloads[nextSeq].data(), f.length);
                f.checksum = computeChecksum(f.data, f.length);
                send_all(sock, (char*)&f, sizeof(f));
                sent[nextSeq] = 1;
                sent_time[nextSeq] = now_ms();
                std::cout << "Sent frame " << nextSeq << "\n";
            }
            nextSeq++;
        }

        fd_set readfds; FD_ZERO(&readfds); FD_SET(sock, &readfds);
        timeval tv{0, 200*1000};
        int sel = select(sock+1, &readfds, NULL, NULL, &tv);
        if (sel > 0 && FD_ISSET(sock, &readfds)) {
            int ack;
            int r = recv_all(sock, (char*)&ack, sizeof(ack));
            if (r > 0 && ack >= 0 && ack < TOTAL) {
                acked[ack] = 1;
                std::cout << "ACK " << ack << "\n";
                while (base < TOTAL && acked[base]) base++;
            }
        }

        uint64_t cur = now_ms();
        for (int i = base; i < std::min(TOTAL, base + WIN_SIZE); i++) {
            if (sent[i] && !acked[i] && (cur - sent_time[i] > TIMEOUT_MS)) {
                Frame f{};
                f.seqNo = i;
                f.length = payloads[i].size();
                memcpy(f.data, payloads[i].data(), f.length);
                f.checksum = computeChecksum(f.data, f.length);
                send_all(sock, (char*)&f, sizeof(f));
                sent_time[i] = cur;
                std::cout << "Resent frame " << i << "\n";
            }
        }
    }

    close(sock);
    return 0;
}
