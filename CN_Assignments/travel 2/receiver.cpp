#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "common.h"
#include "frame.h"
#include "inject_error.h"
#include <thread>
#include <unordered_map>
#include <queue>
#include <set>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int 
#define INVALID_SOCKET -1 
#define SOCKET_ERROR -1
#endif

void usage() {
    std::cout << "Receiver usage:\n";
    std::cout << "receiver.exe <listen_port>\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(); return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    int port = std::stoi(argv[1]);
    std::string mode;
    std::cout << "Enter mode (stopwait | gbn | sr): ";
    std::cin >> mode;
    int window;
    std::cout << "Enter window size (1 for stopwait): ";
    if(mode=="stopwait") window=1;
    else std::cin >> window;

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "socket failed with error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }
#else
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
#endif

    sockaddr_in myAddr{};
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(port);
    if (::bind(sockfd, (struct sockaddr*)&myAddr, sizeof(myAddr)) < 0) {
#ifdef _WIN32
        std::cerr << "bind failed with error: " << WSAGetLastError() << "\n";
        closesocket(sockfd);
        WSACleanup();
#else
        perror("bind");
        close(sockfd);
#endif
        return 1;
    }

    std::cout << "Receiver listening on port " << port << ", mode=" << mode << ", window="<<window<<"\n";

    Stats stats;
    int expectedSeq = 0;
    std::unordered_map<int,std::string> buffer;
    int base = 0;

    std::vector<int> framereceived;
    while (true) {
        char buf[MAX_FRAME_BYTES];
        sockaddr_in from{};
#ifdef _WIN32
        int addrlen = sizeof(from);
#else
        socklen_t addrlen = sizeof(from);
#endif
        int n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&from, &addrlen);
        if (n <= 0) continue;
        std::string recbuf(buf, buf + n);
        stats.framesReceived++; stats.bytesReceived += n;

        Frame f;
        bool ok = Frame::fromBytesWithFCS(recbuf, f, DEFAULT_CRC);
        if (!ok) {
            std::cout << "[R] CRC/Checksum failed: discarding frame\n";
            if (mode == "sr") {
                Frame nak;
                nak.srcMac = defaultMAC("receiver");
                nak.dstMac = defaultMAC("sender");
                nak.seqno = base;
                nak.type = 'N';
                std::string bytesWithFcs = nak.toBytesWithFCS(DEFAULT_CRC);
                sendto(sockfd, bytesWithFcs.data(), bytesWithFcs.size(), 0, (struct sockaddr*)&from, addrlen);
                std::cout << "[R-SR] Sent NAK (CRC) for base=" << base << "\n";
            }
            continue;
        }

        if (f.type == 'F') {
            std::cout << "[R] Received FIN frame. Closing receiver.\n";
            break;
        }

        if (f.type == 'D') {
            int seq = f.seqno;
            std::cout << "[R] Received DATA seq=" << seq << " len=" << f.length << "\n";
            if (mode == "stopwait") {
                Frame ack;
                ack.srcMac = defaultMAC("receiver");
                ack.dstMac = defaultMAC("sender");
                ack.seqno = seq;
                ack.type = 'A';
                std::string bytesWithFcs = ack.toBytesWithFCS(DEFAULT_CRC);
                sendto(sockfd, bytesWithFcs.data(), bytesWithFcs.size(), 0, (struct sockaddr*)&from, addrlen);
                framereceived.push_back(seq);
                std::cout << "[R] Sent ACK " << (int)ack.seqno << "\n";
            }
            else if (mode == "gbn") {
                if (seq == expectedSeq) {
                    std::cout << "[R-GBN] In-order frame " << seq << " delivered\n";
                    Frame ack;
                    ack.seqno = seq;
                    ack.type = 'A';
                    std::string bytesWithFcs = ack.toBytesWithFCS(DEFAULT_CRC);
                    sendto(sockfd, bytesWithFcs.data(), bytesWithFcs.size(), 0, (struct sockaddr*)&from, addrlen);
                    std::cout << "[R-GBN] Sent ACK " << seq << "\n";
                    framereceived.push_back(expectedSeq);
                    expectedSeq++;
                } else {
                    std::cout << "[R-GBN] Out-of-order seq="<<seq<<" expected="<<expectedSeq<<" -> discard\n";
                    int last = expectedSeq - 1;
                    if (last >= 0) {
                        Frame ack;
                        ack.seqno = last;
                        ack.type = 'A';
                        std::string bytesWithFcs = ack.toBytesWithFCS(DEFAULT_CRC);
                        sendto(sockfd, bytesWithFcs.data(), bytesWithFcs.size(), 0, (struct sockaddr*)&from, addrlen);
                        std::cout << "[R-GBN] Re-sent ACK " << last << "\n";
                    }
                }
            }
            else if (mode == "sr") {
                int idx = seq;
                if (idx < base) {
                    std::cout << "[R-SR] Duplicate idx=" << idx << " (already delivered). Re-ACKing.\n";
                    Frame ack;
                    ack.seqno = idx;
                    ack.type = 'A';
                    std::string bytesWithFcs = ack.toBytesWithFCS(DEFAULT_CRC);
                    sendto(sockfd, bytesWithFcs.data(), bytesWithFcs.size(), 0, (struct sockaddr*)&from, addrlen);
                    std::cout << "[R-SR] Re-sent ACK " << idx << "\n";
                }
                else if (idx >= base && idx < base + window) {
                    if (buffer.find(idx) == buffer.end()) {
                        buffer[idx] = f.payload;
                        std::cout << "[R-SR] Buffered idx=" << idx << "\n";
                    } else {
                        std::cout << "[R-SR] Duplicate idx=" << idx << " ignored\n";
                    }
                    Frame ack;
                    ack.seqno = idx;
                    ack.type = 'A';
                    std::string bytesWithFcs = ack.toBytesWithFCS(DEFAULT_CRC);
                    sendto(sockfd, bytesWithFcs.data(), bytesWithFcs.size(), 0, (struct sockaddr*)&from, addrlen);
                    framereceived.push_back(idx);
                    std::cout << "[R-SR] Sent ACK " << idx << "\n";

                    while (buffer.find(base) != buffer.end()) {
                        std::cout << "[R-SR] Delivered idx=" << base << "\n";
                        buffer.erase(base);
                        base++;
                    }
                } else {
                    std::cout << "[R-SR] Seq=" << idx << " not in window (base=" << base << ", window=" << window << ") -> out-of-window\n";
                    // Optionally send NAKs for missing frames in the window
                }
            }
        }
    }
#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return 0;
}
