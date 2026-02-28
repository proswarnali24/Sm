#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "common.h"
#include "frame.h"
#include "inject_error.h"
#include <thread>
#include <random>
#include <fstream>
#include <chrono>
#include <cstdint>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int 
#define INVALID_SOCKET -1 
#define SOCKET_ERROR -1
#endif

// Simple random generator
static std::mt19937 rng((unsigned)time(nullptr));
static std::uniform_real_distribution<> uni(0.0, 1.0);

void usage() {
    std::cout << "Sender usage:\n";
    std::cout << "sender.exe <receiver_ip> <receiver_port> <input_file>\n";
}

// read input file and split into PAYLOAD_SIZE chunks
std::vector<std::string> readDataPackets(const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary);
    std::vector<std::string> packets;
    if (!ifs.is_open()) {
        std::cerr << "Failed to open input file\n";
        return packets;
    }
    while (!ifs.eof()) {
        std::string buf(PAYLOAD_SIZE, 0);
        ifs.read(&buf[0], PAYLOAD_SIZE);
        std::streamsize got = ifs.gcount();
        if (got <= 0) break;
        buf.resize(got);
        // pad payload
        if (got < PAYLOAD_SIZE) buf.append(PAYLOAD_SIZE - got, 0);
        packets.push_back(buf);
    }
    return packets;
}

// Channel(): injects random delay and bit errors
std::string channelSimulate(std::string frameBytes, double errProb, double delayProb, int maxDelayMs) {
    std::string bits = bytesToBitString(frameBytes);
    if (uni(rng) < errProb) {
        singlebit_random_error(bits);
    }
    if (uni(rng) < delayProb) {
        if (maxDelayMs > 0) {
            int d = rand() % maxDelayMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(d));
        }
    }
    return bitStringToBytes(bits);
}

// recv with timeout
bool recvWithTimeout(SOCKET sockfd, std::string &outData, int timeoutMs, sockaddr_in &fromAddr) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
#ifdef _WIN32
    int rv = select(0, &rfds, NULL, NULL, &tv); // nfds is ignored on Windows
#else
    int rv = select(sockfd + 1, &rfds, NULL, NULL, &tv);
#endif
    if (rv > 0 && FD_ISSET(sockfd, &rfds)) {
        char buf[MAX_FRAME_BYTES];
#ifdef _WIN32
        int addrlen = sizeof(fromAddr);
#else
        socklen_t addrlen = sizeof(fromAddr);
#endif
        int n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&fromAddr, &addrlen);
        if (n > 0) {
            outData.assign(buf, buf + n);
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        usage();
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::string recvIp = argv[1];
    int recvPort = std::stoi(argv[2]);
    std::string inputFile = argv[3];

    std::string mode;
    std::cout<<"Enter mode (stopwait | gbn | sr): ";
    std::cin>>mode;

    int windowSize;
    std::cout<<"Enter window size : ";
    if(mode=="stopwait") windowSize=1;
    else std::cin>>windowSize;

    double errProb;
    std::cout<<"Enter bit error probability (0.0-1.0): ";
    std::cin>>errProb;
    double delayProb;
    std::cout<<"Enter delay probability (0.0-1.0): ";
    std::cin>>delayProb;
    int maxDelay;
    std::cout<<"Enter max delay (ms): ";
    std::cin>>maxDelay;

    std::cout << "Mode: " << mode << ", window: " << windowSize
         << ", errProb=" << errProb << ", delayProb=" << delayProb
         << ", maxDelay=" << maxDelay << "ms\n";

    auto packets = readDataPackets(inputFile);
    if (packets.empty()) {
        std::cerr << "No packets to send.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "socket failed\n";
        WSACleanup();
        return 1;
    }
#else
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
#endif

    sockaddr_in recvAddr{};
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(recvPort);
    recvAddr.sin_addr.s_addr = inet_addr(recvIp.c_str());
#ifdef _WIN32
    if (recvAddr.sin_addr.s_addr == INADDR_NONE) {
        std::cerr << "Invalid receiver IP address\n";
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
#endif

    sockaddr_in myAddr{};
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = INADDR_ANY;
    myAddr.sin_port = 0; // ephemeral
    if (::bind(sockfd, (struct sockaddr*)&myAddr, sizeof(myAddr)) < 0) {
#ifdef _WIN32
        std::cerr << "bind failed\n";
        closesocket(sockfd);
        WSACleanup();
#else
        perror("bind");
        close(sockfd);
#endif
        return 1;
    }

    Stats stats;
    std::vector<int> framesent;
    int totalPackets = packets.size();

    if (mode == "stopwait") {
        std::cout << "Running Stop-and-Wait\n";
        int seq = 0;
        for (size_t i = 0; i < packets.size(); ) {
            Frame f;
            f.seqno = seq;
            f.type = 'D';
            f.payload = packets[i];
            f.length = f.payload.size();

            std::string bytesWithFcs = f.toBytesWithFCS(DEFAULT_CRC);
            std::string finalSend = channelSimulate(bytesWithFcs, errProb, delayProb, maxDelay);

            int s = sendto(sockfd, finalSend.data(), finalSend.size(), 0,
                           (struct sockaddr*)&recvAddr, sizeof(recvAddr));
            if (s > 0) { stats.framesSent++; stats.bytesSent += s; }

            auto sendTime = std::chrono::steady_clock::now();
            std::string buf;
            sockaddr_in from{};
            bool got = recvWithTimeout(sockfd, buf, 1000, from);
            framesent.push_back(seq);
            if (got) {
                Frame ack;
                bool ok = Frame::fromBytesWithFCS(buf, ack, DEFAULT_CRC);
                if (ok && ack.type == 'A' && ack.seqno == seq) {
                    auto recvTime = std::chrono::steady_clock::now();
                    double rtt = std::chrono::duration_cast<std::chrono::microseconds>(recvTime - sendTime).count() / 1000.0;
                    stats.rtts.push_back(rtt);
                    std::cout << "[SW] received ACK seq=" << (int)ack.seqno
                         << " Time Taken=" << rtt << " ms\n";
                    i++; seq ^= 1;
                } else {
                    std::cout << "[SW] Bad ACK or wrong seq — retransmit\n";
                }
            } else {
                std::cout << "[SW] Timeout — retransmit seq=" << seq << "\n";
            }
        }
        std::cout << "Stop-and-Wait done.\n";
    }
    else if (mode == "gbn") {
        std::cout << "Running Go-Back-N (N=" << windowSize << ")\n";
        int base = 0;
        int nextSeqNum = 0;
        const int MOD = 256;
        std::vector<std::string> sentFrames(totalPackets);
        auto timerStart = std::chrono::steady_clock::now();
        bool timerRunning = false;
        const int TIMEOUT_MS = 1000;
        
        // Vector to store send times for RTT calculation
        std::vector<std::chrono::steady_clock::time_point> sendTimes(totalPackets);

        while (base < totalPackets) {
            while (nextSeqNum < base + windowSize && nextSeqNum < totalPackets) {
                Frame f;
                f.seqno = nextSeqNum % MOD;
                f.type = 'D';
                f.payload = packets[nextSeqNum];
                f.length = f.payload.size();
                std::string bytesWithFcs = f.toBytesWithFCS(DEFAULT_CRC);
                sentFrames[nextSeqNum] = bytesWithFcs;
                std::string finalSend = channelSimulate(bytesWithFcs, errProb, delayProb, maxDelay);
                
                // Record the send time for the packet
                sendTimes[nextSeqNum] = std::chrono::steady_clock::now();

                sendto(sockfd, finalSend.data(), finalSend.size(), 0, (struct sockaddr*)&recvAddr, sizeof(recvAddr));
                stats.framesSent++;
                framesent.push_back(nextSeqNum);
                std::cout << "[GBN] Sent packet with seq=" << (int)f.seqno << " (index " << nextSeqNum << ")\n";
                if (base == nextSeqNum) {
                    timerStart = std::chrono::steady_clock::now();
                    timerRunning = true;
                }
                nextSeqNum++;
            }

            std::string ackBuf;
            sockaddr_in from{};
            bool gotAck = recvWithTimeout(sockfd, ackBuf, 100, from);

            if (gotAck) {
                Frame ackFrame;
                bool ok = Frame::fromBytesWithFCS(ackBuf, ackFrame, DEFAULT_CRC);
                if (ok && ackFrame.type == 'A') {
                    int ackIndex = -1;
                    for (int i = base; i < nextSeqNum; ++i) {
                        if ((i % MOD) == ackFrame.seqno) {
                            ackIndex = i;
                        }
                    }
                    if (ackIndex >= base) {
                        // Calculate RTT for the acknowledged packet
                        auto recvTime = std::chrono::steady_clock::now();
                        double rtt = std::chrono::duration_cast<std::chrono::microseconds>(recvTime - sendTimes[ackIndex]).count() / 1000.0;
                        stats.rtts.push_back(rtt);

                        base = ackIndex + 1;
                        std::cout << "[GBN] Received ACK for seq=" << (int)ackFrame.seqno
                                  << ". New base is " << base << ". RTT=" << rtt << " ms\n";
                        if (base == nextSeqNum) {
                            timerRunning = false;
                        } else {
                            timerStart = std::chrono::steady_clock::now();
                        }
                    }
                }
            }

            if (timerRunning) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timerStart).count();
                if (elapsed >= TIMEOUT_MS) {
                    std::cout << "[GBN] Timeout! Retransmitting from base=" << base << "\n";
                    for (int i = base; i < nextSeqNum; ++i) {
                        std::string bytesToResend = sentFrames[i];
                        std::string finalSend = channelSimulate(bytesToResend, errProb, delayProb, maxDelay);
                        
                        // Also update the send time on retransmission
                        sendTimes[i] = std::chrono::steady_clock::now();
                        
                        sendto(sockfd, finalSend.data(), finalSend.size(), 0, (struct sockaddr*)&recvAddr, sizeof(recvAddr));
                        stats.framesSent++;
                        framesent.push_back(i);
                        std::cout << "[GBN] Retransmitted packet with seq=" << (i % MOD) << " (index " << i << ")\n";
                    }
                    timerStart = std::chrono::steady_clock::now();
                }
            }
        }
        std::cout << "Go-Back-N done.\n";
    }
    else if (mode == "sr") {
        std::cout << "Running Selective Repeat (N=" << windowSize << ")\n";
        int base = 0;
        int nextSeqNum = 0;
        std::vector<std::string> sentFrames(totalPackets);
        std::vector<bool> acked(totalPackets, false);
        std::vector<std::chrono::steady_clock::time_point> timers(totalPackets);
        const int TIMEOUT_MS = 1000;

        while (base < totalPackets) {
            while (nextSeqNum < base + windowSize && nextSeqNum < totalPackets) {
                Frame f;
                f.seqno = nextSeqNum; // Use index as seq number for simplicity in SR
                f.type = 'D';
                f.payload = packets[nextSeqNum];
                f.length = f.payload.size();
                std::string bytesWithFcs = f.toBytesWithFCS(DEFAULT_CRC);
                sentFrames[nextSeqNum] = bytesWithFcs;
                std::string finalSend = channelSimulate(bytesWithFcs, errProb, delayProb, maxDelay);
                sendto(sockfd, finalSend.data(), finalSend.size(), 0, (struct sockaddr*)&recvAddr, sizeof(recvAddr));
                stats.framesSent++;
                framesent.push_back(nextSeqNum);
                timers[nextSeqNum] = std::chrono::steady_clock::now();
                std::cout << "[SR] Sent packet with index=" << nextSeqNum << "\n";
                nextSeqNum++;
            }

            std::string ackBuf;
            sockaddr_in from{};
            bool gotAck = recvWithTimeout(sockfd, ackBuf, 100, from);

            if (gotAck) {
                Frame ackFrame;
                bool ok = Frame::fromBytesWithFCS(ackBuf, ackFrame, DEFAULT_CRC);
                if (ok && ackFrame.type == 'A') {
                    int ackIndex = ackFrame.seqno;
                    if (ackIndex >= base && ackIndex < nextSeqNum && !acked[ackIndex]) {
                        acked[ackIndex] = true;
                        
                        // Calculate RTT using the stored send time from the 'timers' vector
                        auto recvTime = std::chrono::steady_clock::now();
                        double rtt = std::chrono::duration_cast<std::chrono::microseconds>(recvTime - timers[ackIndex]).count() / 1000.0;
                        stats.rtts.push_back(rtt);
                        
                        std::cout << "[SR] Received ACK for index=" << ackIndex << " RTT=" << rtt << " ms\n";

                        if (ackIndex == base) {
                            while (base < totalPackets && acked[base]) {
                                base++;
                            }
                            std::cout << "[SR] Window base moved to " << base << "\n";
                        }
                    }
                }
            }

            auto now = std::chrono::steady_clock::now();
            for (int i = base; i < nextSeqNum; ++i) {
                if (!acked[i]) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timers[i]).count();
                    if (elapsed >= TIMEOUT_MS) {
                        std::cout << "[SR] Timeout for index=" << i << ". Retransmitting.\n";
                        std::string bytesToResend = sentFrames[i];
                        std::string finalSend = channelSimulate(bytesToResend, errProb, delayProb, maxDelay);
                        sendto(sockfd, finalSend.data(), finalSend.size(), 0, (struct sockaddr*)&recvAddr, sizeof(recvAddr));
                        stats.framesSent++;
                        framesent.push_back(i);
                        timers[i] = std::chrono::steady_clock::now();
                    }
                }
            }
        }
        std::cout << "Selective Repeat done.\n";
    }
    else {
        std::cerr << "Unknown mode specified. Please use 'stopwait', 'gbn', or 'sr'.\n";
    }
    
        // ---------------------------
    // Send FIN (end-of-transmission)
    // ---------------------------
    {
        const int FIN_RETRIES = 5;        // how many times to send FIN
        const int FIN_RETRY_MS = 150;     // wait between retries (ms)

        Frame fin;
        fin.srcMac = defaultMAC("sender");
        fin.dstMac = defaultMAC("receiver");
        fin.type = 'F';          // Finish signal the receiver should check for
        fin.seqno = 0;           // receiver only checks f.type == 'F', so seqno not important
        fin.length = 0;
        fin.payload.clear();

        std::string bytesWithFcs = fin.toBytesWithFCS(DEFAULT_CRC);

        std::cout << "[S] Sending FIN " << FIN_RETRIES << " times to let receiver close cleanly\n";
        for (int r = 0; r < FIN_RETRIES; ++r) {
            // Option A: run FIN through channel simulation (keeps experiments consistent)
            std::string finalSend = channelSimulate(bytesWithFcs, errProb, delayProb, maxDelay);

            int sent = sendto(sockfd, finalSend.data(), finalSend.size(), 0,
                              (struct sockaddr*)&recvAddr, sizeof(recvAddr));
            if (sent > 0) { stats.framesSent++; stats.bytesSent += sent; }
            std::cout << "[S] FIN sent (attempt " << (r+1) << ")\n";

            // short pause between retries
            std::this_thread::sleep_for(std::chrono::milliseconds(FIN_RETRY_MS));
        }
    }


    std::cout << "Frames sent in order: ";
    for(int f: framesent) std::cout<<f<<" ";
    std::cout<<"\n";

    stats.print();

#ifdef _WIN32
    closesocket(sockfd);
    WSACleanup();
#else
    close(sockfd);
#endif
    return 0;
}