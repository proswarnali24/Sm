// receiver.cpp
#include "common.h"
#include "error_utils.h"

#include <iostream>
#include <vector>
#include <cstring>
#include <map>
#include <thread>
#include <chrono>

// sockets
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

vector<uint8_t> serializeFrameFull(const Frame &f, const vector<uint8_t>& crc_rem_bytes) {
    vector<uint8_t> buf;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&f.header);
    buf.insert(buf.end(), p, p + sizeof(FrameHeader));
    buf.insert(buf.end(), f.payload.begin(), f.payload.end());
    if (!crc_rem_bytes.empty()) buf.insert(buf.end(), crc_rem_bytes.begin(), crc_rem_bytes.end());
    if (f.header.detect == DET_CHECKSUM) {
        uint32_t c = htonl(f.checksum);
        const uint8_t *cp = reinterpret_cast<const uint8_t*>(&c);
        buf.insert(buf.end(), cp, cp + 4);
    }
    return buf;
}

Frame parseReceived(const vector<uint8_t>& buf, vector<uint8_t>& out_crc_rem) {
    Frame f;
    out_crc_rem.clear();
    if (buf.size() < sizeof(FrameHeader)) {
        memset(&f.header,0,sizeof(f.header)); f.payload.clear(); f.checksum=0; return f;
    }
    memcpy(&f.header, buf.data(), sizeof(FrameHeader));
    size_t payload_len = ntohs(f.header.payload_len);
    size_t offset = sizeof(FrameHeader);
    if (buf.size() < offset + payload_len) { f.payload.clear(); f.checksum=0; return f; }
    f.payload.assign(buf.begin() + offset, buf.begin() + offset + payload_len);
    offset += payload_len;
    if (f.header.detect == DET_CRC && f.header.crc_rem_bytes > 0) {
        size_t r = f.header.crc_rem_bytes;
        if (buf.size() >= offset + r) {
            out_crc_rem.assign(buf.begin() + offset, buf.begin() + offset + r);
            offset += r;
        }
    }
    if (f.header.detect == DET_CHECKSUM) {
        if (buf.size() >= offset + 4) {
            uint32_t c; memcpy(&c, buf.data() + offset, 4); f.checksum = ntohl(c); offset += 4;
        } else f.checksum = 0;
    }
    return f;
}

uint32_t computeFrameChecksum(const Frame &f) {
    vector<int> bytes;
    const uint8_t *hp = reinterpret_cast<const uint8_t*>(&f.header);
    for (size_t i = 0; i < sizeof(FrameHeader); ++i) bytes.push_back(hp[i]);
    for (auto b : f.payload) bytes.push_back(b);
    auto cs = computeChecksum(bytes);
    uint32_t out = 0;
    out |= (uint32_t)cs[0] << 24;
    out |= (uint32_t)cs[1] << 16;
    out |= (uint32_t)cs[2] << 8;
    out |= (uint32_t)cs[3];
    return out;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "=== Receiver ===\n";
    cout << "Choose mode:\n1. Flow Control (no errors assumed)\n2. Error Control (ARQ)\n> ";
    int mainMode; cin >> mainMode;
    if (mainMode != 1 && mainMode != 2) { cerr << "Invalid mode\n"; return 1; }

    cout << "Choose algorithm:\n";
    if (mainMode == 1) {
        cout << "1. Stop & Wait\n2. Sliding Window\n> ";
    } else {
        cout << "1. Stop & Wait ARQ\n2. Go-Back-N ARQ\n3. Selective Repeat ARQ\n> ";
    }
    int algo; cin >> algo;
    int windowN = 1;
    if ((mainMode == 1 && algo == 2) || (mainMode == 2 && (algo == 2 || algo == 3))) {
        cout << "Enter window size (N): "; cin >> windowN;
        if (windowN < 1) windowN = 1;
    }

    cout << "Choose detection method:\n1. Checksum\n2. CRC\n> ";
    int detectChoice; cin >> detectChoice;
    vector<int> generatorPoly;
    string generatorStr;
    if (detectChoice == 2) {
        cout << "Enter CRC generator polynomial as bits (same as sender): ";
        cin >> generatorStr;
        generatorPoly = stringToPolynomial(generatorStr);
        if (generatorPoly.size() < 2) { cerr << "Invalid generator\n"; return 1;}
    }

    cout << "Receiver binding to port 6000, waiting for sender...\n";

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    sockaddr_in raddr{}; raddr.sin_family = AF_INET; raddr.sin_port = htons(6000); raddr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (sockaddr*)&raddr, sizeof(raddr));

    sockaddr_in senderAddr{}; socklen_t slen = sizeof(senderAddr);

    int expectedSeq = 0;
    map<int, vector<uint8_t>> srBuffer;

    while (true) {
        uint8_t buf[2048];
        int n = recvfrom(sock, (char*)buf, sizeof(buf), 0, (sockaddr*)&senderAddr, &slen);
        if (n <= 0) continue;
        vector<uint8_t> in(buf, buf + n);
        vector<uint8_t> crc_rem;
        Frame f = parseReceived(in, crc_rem);

        if (f.header.type != DATA) continue;

        // Decide detection: check header.detect matches chosen - if not, still attempt using chosen method
        bool valid = false;
        if (detectChoice == 1) {
            // Checksum mode
            uint32_t calc = computeFrameChecksum(f);
            valid = (calc == f.checksum);
        } else {
            // CRC mode: recompute bits from header+payload+crc_rem and run verifyCRC
            vector<uint8_t> tmp;
            const uint8_t* hp = reinterpret_cast<const uint8_t*>(&f.header);
            tmp.insert(tmp.end(), hp, hp + sizeof(FrameHeader));
            tmp.insert(tmp.end(), f.payload.begin(), f.payload.end());
            tmp.insert(tmp.end(), crc_rem.begin(), crc_rem.end());
            vector<int> bits = bytesToBits(tmp);
            valid = verifyCRC(bits, generatorPoly);
        }

        cout << "[RCV] Frame seq=" << (int)f.header.seqNo << " payload_len=" << ntohs(f.header.payload_len) << " valid=" << valid << "\n";

        if (!valid) {
            cout << "[RCV] Corrupted frame discarded: seq " << (int)f.header.seqNo << "\n";
            // For ARQ: typically do nothing (no ACK) except maybe in SR send NAK; here we do nothing
            continue;
        }

        if (mainMode == 1) {
            // Flow control (no errors) - just accept and (optionally) send ACK? Flow control commonly doesn't use ACKs in simple assignment, but we'll send ACKs to show progress.
            if (algo == 1) {
                if (f.header.seqNo == expectedSeq) {
                    cout << "[FLOW-SW] Delivered seq " << expectedSeq << "\n";
                    // send ACK
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = f.header.seqNo;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                    expectedSeq = (expectedSeq + 1) % 256;
                } else {
                    // out of order -> send last ack
                    uint8_t last = (expectedSeq + 255) % 256;
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = last;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                }
            } else {
                // Sliding window (flow): accept all frames arriving (we don't expect errors). We'll ACK each.
                cout << "[FLOW-WIND] Delivered seq " << (int)f.header.seqNo << "\n";
                Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = f.header.seqNo;
                ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
            }
        } else {
            // ARQ modes
            if (algo == 1) {
                // Stop & Wait ARQ
                if (f.header.seqNo == expectedSeq) {
                    cout << "[ARQ-SW] Delivered seq " << expectedSeq << "\n";
                    // send ACK
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = f.header.seqNo;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                    expectedSeq = (expectedSeq + 1) % 256;
                } else {
                    // send ACK for last delivered
                    uint8_t last = (expectedSeq + 255) % 256;
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = last;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                }
            } else if (algo == 2) {
                // Go-Back-N
                if (f.header.seqNo == expectedSeq) {
                    cout << "[GBN] Delivered seq " << expectedSeq << "\n";
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = f.header.seqNo;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                    expectedSeq = (expectedSeq + 1) % 256;
                } else {
                    uint8_t last = (expectedSeq + 255) % 256;
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = last;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                }
            } else {
                // Selective Repeat
                int seq = f.header.seqNo;
                if (seq >= expectedSeq && seq < expectedSeq + windowN) {
                    if (!srBuffer.count(seq)) {
                        srBuffer[seq] = f.payload;
                        cout << "[SR] Buffered seq " << seq << "\n";
                    }
                    // send individual ACK
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = f.header.seqNo;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                    // try deliver in-order
                    while (srBuffer.count(expectedSeq)) {
                        cout << "[SR] Delivered buffered seq " << expectedSeq << "\n";
                        srBuffer.erase(expectedSeq);
                        expectedSeq = (expectedSeq + 1) % 256;
                    }
                } else {
                    // outside window -> re-ACK last
                    uint8_t last = (expectedSeq + 255) % 256;
                    Frame ack; memset(&ack.header,0,sizeof(ack.header)); ack.header.type = ACK; ack.header.seqNo = last;
                    ack.header.payload_len = htons(0); ack.header.detect = DET_CHECKSUM; ack.checksum = computeFrameChecksum(ack);
                    vector<uint8_t> ackbuf = serializeFrameFull(ack, {});
                    sendto(sock, (const char*)ackbuf.data(), ackbuf.size(), 0, (sockaddr*)&senderAddr, slen);
                }
            }
        }
    }

    close(sock);
    return 0;
}
