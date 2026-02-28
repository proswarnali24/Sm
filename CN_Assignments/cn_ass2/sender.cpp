// sender.cpp
#include "common.h"
#include "error_utils.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>
#include <random>

// socket includes
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>

using namespace std;

static std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());

vector<uint8_t> serializeFrame(const Frame &f) {
    vector<uint8_t> buf;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&f.header);
    buf.insert(buf.end(), p, p + sizeof(FrameHeader));
    // payload
    buf.insert(buf.end(), f.payload.begin(), f.payload.end());
    if (f.header.detect == DET_CRC) {
        // crc_rem_bytes bytes are assumed already appended to payload? In our design CRC remainder is appended after payload when serializing
        // But here we assume sender stored remainder bytes in f.payload after payload area? To be safe: receiver expects header then payload then crc bytes then checksum (absent)
        // We'll assume f.payload contains only the payload and we'll append CRC bytes separately (we stored them in header.crc_rem_bytes field and we pass them via payload_extra below)
        // For simplicity we will append nothing here: CRC remainder must be appended by caller prior to serialization in f.payloadTail (we use f.payload to contain only payload)
    }
    if (f.header.detect == DET_CHECKSUM) {
        uint32_t c = htonl(f.checksum);
        const uint8_t *cp = reinterpret_cast<const uint8_t*>(&c);
        buf.insert(buf.end(), cp, cp + 4);
    }
    return buf;
}

// We'll create a helper that serializes header + payload + crc remainder bytes (if any) + checksum
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

Frame deserializeFrame(const vector<uint8_t> &buf, vector<uint8_t>& out_crc_rem) {
    Frame f;
    out_crc_rem.clear();
    if (buf.size() < sizeof(FrameHeader)) {
        memset(&f.header, 0, sizeof(f.header));
        f.payload.clear();
        f.checksum = 0;
        return f;
    }
    memcpy(&f.header, buf.data(), sizeof(FrameHeader));
    size_t payload_len = ntohs(f.header.payload_len);
    size_t offset = sizeof(FrameHeader);
    if (buf.size() < offset + payload_len) {
        f.payload.clear();
        f.checksum = 0;
        return f;
    }
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
            uint32_t c;
            memcpy(&c, buf.data() + offset, 4);
            f.checksum = ntohl(c);
            offset += 4;
        } else f.checksum = 0;
    }
    return f;
}

// compute checksum over header+payload bytes (excluding f.checksum field)
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

vector<uint8_t> computeCRCremBytes(const Frame &f, const vector<int>& genPoly) {
    // build bit-vector of header bytes + payload bytes
    vector<uint8_t> tmpBuf;
    const uint8_t *hp = reinterpret_cast<const uint8_t*>(&f.header);
    tmpBuf.insert(tmpBuf.end(), hp, hp + sizeof(FrameHeader));
    tmpBuf.insert(tmpBuf.end(), f.payload.begin(), f.payload.end());
    vector<int> bits = bytesToBits(tmpBuf);
    vector<int> rem = computeCRC(bits, genPoly); // remainder bits
    // pack remainder bits into bytes (MSB-first)
    vector<uint8_t> rem_bytes = bitsToBytes(rem);
    return rem_bytes;
}

void usageAndExit() {
    cerr << "sender usage: run without args; program will ask interactively.\n";
    exit(1);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "=== Sender ===\n";
    cout << "Choose mode:\n1. Flow Control (no errors assumed)\n2. Error Control (ARQ with errors)\n> ";
    int mainMode;
    cin >> mainMode;
    if (mainMode != 1 && mainMode != 2) { cerr << "Invalid mode\n"; return 1; }

    cout << "Choose algorithm:\n";
    if (mainMode == 1) {
        cout << "1. Stop & Wait\n2. Sliding Window\n> ";
    } else {
        cout << "1. Stop & Wait ARQ\n2. Go-Back-N ARQ\n3. Selective Repeat ARQ\n> ";
    }
    int algo;
    cin >> algo;

    int windowN = 1;
    if ((mainMode == 1 && algo == 2) || (mainMode == 2 && (algo == 2 || algo == 3))) {
        cout << "Enter window size (N): ";
        cin >> windowN;
        if (windowN < 1) windowN = 1;
    }

    cout << "Choose detection method:\n1. Checksum\n2. CRC\n> ";
    int detectChoice;
    cin >> detectChoice;
    vector<int> generatorPoly;
    string generatorStr;
    if (detectChoice == 2) {
        cout << "Enter CRC generator polynomial as bits (e.g. 10011): ";
        cin >> generatorStr;
        generatorPoly = stringToPolynomial(generatorStr);
        if (generatorPoly.size() < 2) {
            cerr << "Invalid generator\n"; return 1;
        }
    }

    double errorProb = 0.0, delayProb = 0.0;
    if (mainMode == 2) {
        cout << "Enter bit-error probability (0.0 - 1.0): ";
        cin >> errorProb;
        cout << "Enter delay/drop probability (0.0 - 1.0): ";
        cin >> delayProb;
    }

    string infile;
    cout << "Enter input filename to send: ";
    cin >> infile;

    // Read file and split
    ifstream ifs(infile, ios::binary);
    if (!ifs) { cerr << "Cannot open " << infile << "\n"; return 1; }
    vector<vector<uint8_t>> payloads;
    while (!ifs.eof()) {
        vector<uint8_t> buf(PAYLOAD_SIZE);
        ifs.read(reinterpret_cast<char*>(buf.data()), PAYLOAD_SIZE);
        streamsize r = ifs.gcount();
        if (r <= 0) break;
        buf.resize(r);
        payloads.push_back(buf);
    }
    if (payloads.empty()) { cerr << "No data in file\n"; return 1; }

    // sockets (sender -> bind 5000 ; send to 6000)
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    sockaddr_in saddr{}, raddr{};
    saddr.sin_family = AF_INET; saddr.sin_port = htons(5000); saddr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (sockaddr*)&saddr, sizeof(saddr));
    raddr.sin_family = AF_INET; raddr.sin_port = htons(6000);
    inet_pton(AF_INET, "127.0.0.1", &raddr.sin_addr);

    uniform_real_distribution<> prob(0.0,1.0);
    int totalFrames = (int)payloads.size();

    // prepare frames
    vector<Frame> frames;
    vector<vector<uint8_t>> crc_rem_bytes_list; crc_rem_bytes_list.resize(totalFrames);
    for (int i = 0; i < totalFrames; ++i) {
        Frame f;
        memset(&f.header, 0, sizeof(f.header));
        for (int k=0;k<6;++k) { f.header.srcAddr[k]=0xAA; f.header.dstAddr[k]=0xBB; }
        f.header.payload_len = htons((uint16_t)payloads[i].size());
        f.header.seqNo = (uint8_t)(i % 256);
        f.header.type = DATA;
        f.header.detect = (detectChoice == 1 ? DET_CHECKSUM : DET_CRC);
        f.header.crc_rem_bytes = 0;
        f.payload = payloads[i];
        f.checksum = 0;
        // if CRC method chosen, compute remainder bytes now so they are sent along
        if (detectChoice == 2) {
            vector<uint8_t> rem = computeCRCremBytes(f, generatorPoly);
            frames.push_back(f);
            crc_rem_bytes_list[i] = rem;
            frames.back().header.crc_rem_bytes = (uint8_t)rem.size();
        } else {
            frames.push_back(f);
            frames.back().header.crc_rem_bytes = 0;
            frames.back().checksum = computeFrameChecksum(frames.back());
        }
    }

    cout << "Starting transmission. total frames = " << totalFrames << "\n";

    // helpers to send & optionally simulate errors/drops (only for ARQ mode)
    auto send_on_socket = [&](int idx) {
        Frame &f = frames[idx];
        vector<uint8_t> rem_bytes = crc_rem_bytes_list[idx];
        vector<uint8_t> buf = serializeFrameFull(f, rem_bytes);
        // ARQ mode: simulate errors/delay
        if (mainMode == 2) {
            if (prob(rng) < errorProb) {
                // flip random bit (or burst)
                injectBitError(buf, "bit", 3);
                cout << "[CHAN] Injected error into frame " << idx << "\n";
            }
            if (prob(rng) < delayProb) {
                // simulate long delay (drop) by sleeping longer than timeout
                this_thread::sleep_for(chrono::milliseconds(800));
                cout << "[CHAN] Simulated delay for frame " << idx << "\n";
            }
        }
        sendto(sock, reinterpret_cast<const char*>(buf.data()), buf.size(), 0, (sockaddr*)&raddr, sizeof(raddr));
    };

    fd_set readfds;
    timeval tv{};
    const int TIMEOUT_MS = 500;

    // Transmission loops for each mode
    if (mainMode == 1) {
        // Flow control (assume no errors)
        if (algo == 1) {
            // Stop & Wait (flow)
            for (int i = 0; i < totalFrames; ++i) {
                cout << "[FLOW-SW] Sending seq " << i << "\n";
                send_on_socket(i);
                // wait small time to simulate link (no retransmits because no errors)
                this_thread::sleep_for(chrono::milliseconds(50));
            }
            cout << "Flow control SW complete\n";
        } else {
            // Sliding Window (flow control) - send in windows only (no ACKs)
            int base = 0;
            while (base < totalFrames) {
                int sendUntil = min(totalFrames, base + windowN);
                for (int s = base; s < sendUntil; ++s) {
                    cout << "[FLOW-SWIND] Sending seq " << s << "\n";
                    send_on_socket(s);
                }
                base = sendUntil;
                this_thread::sleep_for(chrono::milliseconds(100));
            }
            cout << "Flow control Sliding Window complete\n";
        }
    } else {
        // ARQ modes with ACK handling
        if (algo == 1) {
            // Stop-and-wait ARQ
            for (int i = 0; i < totalFrames; ++i) {
                bool acked = false;
                cout << "[ARQ-SW] Sending seq " << i << "\n";
                send_on_socket(i);
                auto start = chrono::high_resolution_clock::now();
                while (!acked) {
                    FD_ZERO(&readfds);
                    FD_SET(sock, &readfds);
                    tv.tv_sec = 0; tv.tv_usec = 100000;
                    int rv = select(sock+1, &readfds, NULL, NULL, &tv);
                    if (rv > 0 && FD_ISSET(sock, &readfds)) {
                        uint8_t rbuf[1500];
                        sockaddr_in from{}; socklen_t fl = sizeof(from);
                        int n = recvfrom(sock, (char*)rbuf, sizeof(rbuf), 0, (sockaddr*)&from, &fl);
                        if (n > 0) {
                            vector<uint8_t> v(rbuf, rbuf + n);
                            vector<uint8_t> rem;
                            Frame rf = deserializeFrame(v, rem);
                            if (rf.header.type == ACK && rf.header.seqNo == frames[i].header.seqNo) {
                                cout << "[ARQ-SW] Got ACK for " << i << "\n";
                                acked = true; break;
                            }
                        }
                    }
                    auto now = chrono::high_resolution_clock::now();
                    if (chrono::duration_cast<chrono::milliseconds>(now - start).count() > TIMEOUT_MS) {
                        cout << "[ARQ-SW] Timeout, retransmit " << i << "\n";
                        send_on_socket(i);
                        start = chrono::high_resolution_clock::now();
                    }
                }
            }
            cout << "Stop-and-wait ARQ done\n";
        } else if (algo == 2) {
            // Go-Back-N ARQ
            int base = 0, nextSeq = 0;
            while (base < totalFrames) {
                while (nextSeq < totalFrames && nextSeq < base + windowN) {
                    cout << "[GBN] Sending seq " << nextSeq << "\n";
                    send_on_socket(nextSeq);
                    ++nextSeq;
                }
                // wait for ACK (cumulative)
                FD_ZERO(&readfds);
                FD_SET(sock, &readfds);
                tv.tv_sec = 0; tv.tv_usec = 300000;
                int rv = select(sock+1, &readfds, NULL, NULL, &tv);
                bool gotAck = false;
                if (rv > 0 && FD_ISSET(sock, &readfds)) {
                    uint8_t rbuf[1500];
                    sockaddr_in from{}; socklen_t fl = sizeof(from);
                    int n = recvfrom(sock, (char*)rbuf, sizeof(rbuf), 0, (sockaddr*)&from, &fl);
                    if (n > 0) {
                        vector<uint8_t> v(rbuf, rbuf + n);
                        vector<uint8_t> rem;
                        Frame rf = deserializeFrame(v, rem);
                        if (rf.header.type == ACK) {
                            int ackno = rf.header.seqNo;
                            cout << "[GBN] Got ACK " << ackno << "\n";
                            if (ackno >= base && ackno < totalFrames) {
                                base = ackno + 1;
                                gotAck = true;
                            }
                        }
                    }
                }
                if (!gotAck) {
                    // timeout -> retransmit from base
                    cout << "[GBN] Timeout -> retransmit from base " << base << "\n";
                    nextSeq = base;
                }
            }
            cout << "Go-Back-N ARQ done\n";
        } else {
            // Selective Repeat ARQ
            vector<bool> acked(totalFrames,false);
            int base = 0;
            const int TIMEOUT_MS_SR = 500;
            // For simplicity: don't implement per-packet timers fully; we'll use periodic retransmit of unacked in window after a timeout tick
            while (base < totalFrames) {
                // send unacked in window
                for (int s = base; s < totalFrames && s < base + windowN; ++s) {
                    if (!acked[s]) {
                        cout << "[SR] Sending seq " << s << "\n";
                        send_on_socket(s);
                    }
                }
                // wait short and collect ACKs
                FD_ZERO(&readfds);
                FD_SET(sock, &readfds);
                tv.tv_sec = 0; tv.tv_usec = 250000;
                int rv = select(sock+1, &readfds, NULL, NULL, &tv);
                if (rv > 0 && FD_ISSET(sock, &readfds)) {
                    uint8_t rbuf[1500];
                    sockaddr_in from{}; socklen_t fl = sizeof(from);
                    int n = recvfrom(sock, (char*)rbuf, sizeof(rbuf), 0, (sockaddr*)&from, &fl);
                    if (n > 0) {
                        vector<uint8_t> v(rbuf, rbuf + n);
                        vector<uint8_t> rem;
                        Frame rf = deserializeFrame(v, rem);
                        if (rf.header.type == ACK) {
                            int ackno = rf.header.seqNo;
                            if (ackno >= 0 && ackno < totalFrames) {
                                acked[ackno] = true;
                                cout << "[SR] Got ACK " << ackno << "\n";
                                while (base < totalFrames && acked[base]) base++;
                            }
                        }
                    }
                } else {
                    // timeout tick -> retransmit unacked in window (loop will do)
                    cout << "[SR] Timeout tick -> retransmit unacked in window\n";
                }
            }
            cout << "Selective Repeat ARQ done\n";
        }
    }

    close(sock);
    cout << "Sender finished.\n";
    return 0;
}
