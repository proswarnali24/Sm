// sender.cpp
#include "frame.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

using namespace std;

void usage(const char *p){
    cerr << "Usage: " << p << " <receiver_ip> <receiver_port> <mode: sw|gbn|sr> <window_size> <payload_size> <drop_prob> <bit_err_prob> <max_delay_ms> <input_file>\n";
    cerr << "Example: ./sender 127.0.0.1 9000 sr 4 46 0 0 0 input.txt\n";
}

struct SentInfo {
    Frame frame;
    chrono::steady_clock::time_point send_time;
    bool acked = false;
};

int main(int argc, char** argv){
    if(argc < 10){ usage(argv[0]); return 1; }
    string recv_ip = argv[1];
    int recv_port = atoi(argv[2]);
    string mode = argv[3];
    int win_size = stoi(argv[4]);
    int payload_size = stoi(argv[5]);
    ChannelParams channel;
    channel.drop_prob = stod(argv[6]);
    channel.bit_error_prob = stod(argv[7]);
    channel.max_delay_ms = stod(argv[8]);
    string input_file = argv[9];

    if(payload_size < MIN_PAYLOAD) payload_size = MIN_PAYLOAD;
    if(payload_size > MAX_PAYLOAD) payload_size = MAX_PAYLOAD;

    // read input file into chunks
    ifstream fin(input_file, ios::binary);
    if(!fin){ cerr << "Cannot open input file\n"; return 1; }
    vector<vector<uint8_t>> chunks;
    while(!fin.eof()){
        vector<uint8_t> buf(payload_size, 0);
        fin.read((char*)buf.data(), payload_size);
        streamsize r = fin.gcount();
        if(r<=0) break;
        buf.resize((size_t)r);
        chunks.push_back(buf);
    }
    fin.close();
    if(chunks.empty()){ cerr << "No data to send\n"; return 1; }
    cout << "Read " << chunks.size() << " payload chunks\n";

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){ perror("socket"); return 1; }

    sockaddr_in raddr{};
    raddr.sin_family = AF_INET;
    raddr.sin_port = htons(recv_port);
    inet_pton(AF_INET, recv_ip.c_str(), &raddr.sin_addr);

    const int MAX_SEQ = 256;
    vector<SentInfo> buffer(MAX_SEQ);
    uint8_t base = 0;
    uint8_t nextseq = 0;
    mutex mtx;
    condition_variable cv;
    atomic<bool> running(true);
    atomic<int> total_sent(0), total_acked(0), total_retx(0);

    // ACK listener thread
    thread ack_thread([&](){
        sockaddr_in from{};
        socklen_t fromlen = sizeof(from);
        while(running){
            Frame ackf{}; ssize_t r = recvfrom(sock, &ackf, sizeof(Frame), 0, (sockaddr*)&from, &fromlen);
            if(r <= 0) continue;
            // defensive check
            uint16_t plen = hdr_get_length(ackf.hdr);
            size_t expected = sizeof(FrameHeader) + (size_t)plen + sizeof(uint32_t);
            if((size_t)r < expected){
                cerr << "[sender-ack] recv size " << r << " < expected " << expected << "\n";
            }
            uint32_t recv_cs = ntohl(ackf.checksum);
            uint32_t calc = compute_checksum(ackf.hdr, nullptr, 0);
            if(recv_cs != calc){
                cout << "[sender] Received corrupted ACK/NAK (calc="<<calc<<" recv="<<recv_cs<<")\n";
                continue;
            }
            if(ackf.hdr.type == FrameType::ACK){
                uint8_t s = ackf.hdr.seq;
                unique_lock<mutex> lk(mtx);
                if(!buffer[s].acked){
                    buffer[s].acked = true;
                    total_acked++;
                    cout << "[sender] ACK for seq=" << int(s) << "\n";
                }
                // advance base for GBN and SR
                if(mode == "sw"){
                    if(s == base){
                        buffer[base].acked = false;
                        base++;
                        cv.notify_all();
                    }
                } else if(mode == "gbn"){
                    while(buffer[base].acked && base != nextseq){
                        buffer[base].acked = false; base++;
                    }
                    cv.notify_all();
                } else if(mode == "sr"){
                    while(buffer[base].acked && base != nextseq){
                        buffer[base].acked = false; base++;
                    }
                    cv.notify_all();
                }
            } else if(ackf.hdr.type == FrameType::NAK){
                uint8_t s = ackf.hdr.seq;
                cout << "[sender] Received NAK for seq="<<int(s)<<"\n";
                unique_lock<mutex> lk(mtx);
                buffer[s].acked = false;
                buffer[s].send_time = chrono::steady_clock::time_point(); // force retransmit
                cv.notify_all();
            }
        }
    });

    // timeout thread
    const chrono::milliseconds TIMEOUT_MS(500);
    thread timeout_thread([&](){
        while(running){
            this_thread::sleep_for(chrono::milliseconds(50));
            unique_lock<mutex> lk(mtx);
            if(mode == "gbn"){
                if(base != nextseq){
                    auto &info = buffer[base];
                    if(!info.acked){
                        auto now = chrono::steady_clock::now();
                        if(now - info.send_time > TIMEOUT_MS){
                            cout << "[timeout] GBN timeout at base="<<int(base)<<"\n";
                            uint8_t seq = base;
                            while(seq != nextseq){
                                Frame copy = buffer[seq].frame;
                                if(!simulate_drop(channel)){
                                    simulate_delay(channel);
                                    simulate_bit_error(copy, hdr_get_length(copy.hdr), channel);
                                    sendto(sock, &copy, sizeof(FrameHeader) + hdr_get_length(copy.hdr) + sizeof(uint32_t), 0, (sockaddr*)&raddr, sizeof(raddr));
                                    total_sent++; total_retx++;
                                } else {
                                    cout << "[sender-channel] simulated drop on retransmit seq="<<int(seq)<<"\n";
                                }
                                seq++;
                            }
                            // reset send_time
                            seq = base;
                            while(seq != nextseq){ buffer[seq].send_time = chrono::steady_clock::now(); seq++; }
                        }
                    }
                }
            } else {
                uint8_t seq = base;
                while(seq != nextseq){
                    auto &info = buffer[seq];
                    if(!info.acked){
                        if(info.send_time.time_since_epoch().count() > 0){
                            auto now = chrono::steady_clock::now();
                            if(now - info.send_time > TIMEOUT_MS){
                                cout << "[timeout] Retransmit seq="<<int(seq)<<"\n";
                                Frame copy = info.frame;
                                if(!simulate_drop(channel)){
                                    simulate_delay(channel);
                                    simulate_bit_error(copy, hdr_get_length(copy.hdr), channel);
                                    sendto(sock, &copy, sizeof(FrameHeader) + hdr_get_length(copy.hdr) + sizeof(uint32_t), 0, (sockaddr*)&raddr, sizeof(raddr));
                                    total_sent++; total_retx++;
                                } else {
                                    cout << "[sender-channel] simulated drop on retransmit seq="<<int(seq)<<"\n";
                                }
                                info.send_time = chrono::steady_clock::now();
                            }
                        } // else send_time not set (shouldn't normally happen)
                    }
                    seq++;
                }
            }
        }
    });

    // helper to make frame (and store checksum in network order)
    auto make_frame = [&](uint8_t seq, const vector<uint8_t> &payload)->Frame{
        Frame f{}; memset(&f,0,sizeof(f));
        for(int i=0;i<MAC_LEN;i++){ f.hdr.src[i] = 0xAA; f.hdr.dst[i] = 0xBB; }
        hdr_set_length(f.hdr, (uint16_t)payload.size());
        f.hdr.seq = seq;
        f.hdr.type = FrameType::DATA;
        memcpy(f.payload, payload.data(), payload.size());
        uint32_t cs = compute_checksum(f.hdr, f.payload, payload.size());
        f.checksum = htonl(cs);
        return f;
    };

    // main sending loop: send all chunks
    size_t idx = 0;
    size_t total_chunks = chunks.size();
    while(idx < total_chunks){
        unique_lock<mutex> lk(mtx);
        uint8_t window_used = (uint8_t)((nextseq - base) & 0xFF);
        if(mode == "sw"){
            if(window_used >= 1){ cv.wait_for(lk, chrono::milliseconds(100)); continue; }
        } else {
            if(window_used >= (uint8_t)win_size){ cv.wait_for(lk, chrono::milliseconds(50)); continue; }
        }

        uint8_t seq = nextseq;
        Frame f = make_frame(seq, chunks[idx]);
        // simulate send-side channel
        if(!simulate_drop(channel)){
            simulate_delay(channel);
            Frame copy = f;
            simulate_bit_error(copy, hdr_get_length(copy.hdr), channel);
            ssize_t s = sendto(sock, &copy, sizeof(FrameHeader) + hdr_get_length(copy.hdr) + sizeof(uint32_t), 0, (sockaddr*)&raddr, sizeof(raddr));
            (void)s;
        } else {
            cout << "[sender-channel] Simulated drop of outgoing frame seq="<<int(seq)<<"\n";
        }
        total_sent++;

        buffer[seq].frame = f;
        buffer[seq].send_time = chrono::steady_clock::now();
        buffer[seq].acked = false;
        nextseq++;
        idx++;
    }

    // wait for outstanding ACKs (simple)
    while(true){
        unique_lock<mutex> lk(mtx);
        if(base == nextseq) break;
        cv.wait_for(lk, chrono::milliseconds(200));
    }

    running = false;
    this_thread::sleep_for(chrono::milliseconds(200));
    close(sock);
    if(ack_thread.joinable()) ack_thread.join();
    if(timeout_thread.joinable()) timeout_thread.join();

    cout << "Finished sending. total_sent="<<total_sent<<" total_acked="<<total_acked<<" total_retx="<<total_retx<<"\n";
    return 0;
}
