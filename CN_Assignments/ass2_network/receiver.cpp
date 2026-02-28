// receiver.cpp
#include "frame.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>

using namespace std;

void usage(const char *p) {
    cerr << "Usage: " << p << " <listen_port> <mode: sw|gbn|sr> <payload_size> <drop_prob> <bit_err_prob> <max_delay_ms>\n";
    cerr << "Example: ./receiver 9000 sr 46 0.0 0.0 0\n";
}

int main(int argc, char** argv){
    if(argc < 7) { usage(argv[0]); return 1; }
    int listen_port = atoi(argv[1]);
    string mode = argv[2];
    int payload_size = stoi(argv[3]);
    ChannelParams channel;
    channel.drop_prob = stod(argv[4]);
    channel.bit_error_prob = stod(argv[5]);
    channel.max_delay_ms = stod(argv[6]);

    if(payload_size < MIN_PAYLOAD) payload_size = MIN_PAYLOAD;
    if(payload_size > MAX_PAYLOAD) payload_size = MAX_PAYLOAD;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){ perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(::bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); close(sock); return 1; }

    cout << "Receiver listening on port " << listen_port << " mode="<<mode<<" payload="<<payload_size<<"\n";

    sockaddr_in sender_addr{};
    socklen_t sender_len = sizeof(sender_addr);

    const int MAX_SEQ = 256;
    vector<vector<uint8_t>> sr_buffer(MAX_SEQ);
    vector<bool> sr_received(MAX_SEQ,false);
    uint8_t expected_seq = 0;

    while(true){
        Frame f{};
        ssize_t r = recvfrom(sock, &f, sizeof(Frame), 0, (sockaddr*)&sender_addr, &sender_len);
        if(r <= 0) continue;
        // check actual recv size
        uint16_t plen = hdr_get_length(f.hdr);
        size_t expected_bytes = sizeof(FrameHeader) + (size_t)plen + sizeof(uint32_t);
        if((size_t)r < expected_bytes) {
            cerr << "[WARN] recv size " << r << " < expected " << expected_bytes << "\n";
            // continue; // keep processing - might still work
        }

        // simulate channel effects
        if(simulate_drop(channel)) {
            cout << "[channel] Dropped incoming frame\n";
            continue;
        }
        simulate_delay(channel);
        simulate_bit_error(f, plen, channel);

        // verify checksum: received is network order
        uint32_t recv_cs = ntohl(f.checksum);
        uint32_t calc = compute_checksum(f.hdr, f.payload, plen);
        if(calc != recv_cs){
            cout << "[receiver] Corrupted frame seq="<<int(f.hdr.seq)<<" - discarding (calc="<<calc<<" recv="<<recv_cs<<")\n";
            // for SR we could send NAK; for now just discard
            if(mode == "sr") {
                Frame nack{}; memset(&nack,0,sizeof(nack));
                hdr_set_length(nack.hdr, 0);
                nack.hdr.seq = f.hdr.seq;
                nack.hdr.type = FrameType::NAK;
                uint32_t cs = compute_checksum(nack.hdr, nullptr, 0);
                nack.checksum = htonl(cs);
                sendto(sock, &nack, sizeof(FrameHeader)+sizeof(uint32_t), 0, (sockaddr*)&sender_addr, sender_len);
            }
            continue;
        }

        // Good frame
        uint8_t seq = f.hdr.seq;
        if(mode == "sw"){
            if(seq == expected_seq){
                cout << "[SW] Received expected seq="<<int(seq)<<" payload_len="<<plen<<"\n";
                if(plen>0) cout.write((char*)f.payload, plen), cout<<"\n";
                // send ACK
                Frame ack{}; memset(&ack,0,sizeof(ack));
                hdr_set_length(ack.hdr, 0);
                ack.hdr.seq = seq;
                ack.hdr.type = FrameType::ACK;
                uint32_t cs = compute_checksum(ack.hdr, nullptr, 0);
                ack.checksum = htonl(cs);
                sendto(sock, &ack, sizeof(FrameHeader)+sizeof(uint32_t), 0, (sockaddr*)&sender_addr, sender_len);
                expected_seq++;
                cout << "[SW] Sent ACK " << int(seq) << "\n";
            } else {
                cout << "[SW] Out-of-order seq="<<int(seq)<<" expected="<<int(expected_seq)<<" - discard\n";
                // re-ACK last (optional)
                uint8_t last = expected_seq - 1;
                Frame ack{}; memset(&ack,0,sizeof(ack));
                hdr_set_length(ack.hdr,0);
                ack.hdr.seq = last;
                ack.hdr.type = FrameType::ACK;
                ack.checksum = htonl(compute_checksum(ack.hdr,nullptr,0));
                sendto(sock, &ack, sizeof(FrameHeader)+sizeof(uint32_t), 0, (sockaddr*)&sender_addr, sender_len);
            }
        } else if(mode == "gbn"){
            if(seq == expected_seq){
                cout << "[GBN] Received in-order seq="<<int(seq)<<"\n";
                if(plen>0) cout.write((char*)f.payload, plen), cout<<"\n";
                Frame ack{}; memset(&ack,0,sizeof(ack));
                hdr_set_length(ack.hdr,0);
                ack.hdr.seq = seq;
                ack.hdr.type = FrameType::ACK;
                ack.checksum = htonl(compute_checksum(ack.hdr,nullptr,0));
                sendto(sock, &ack, sizeof(FrameHeader)+sizeof(uint32_t), 0, (sockaddr*)&sender_addr, sender_len);
                expected_seq++;
            } else {
                cout << "[GBN] Out-of-order seq="<<int(seq)<<" expected="<<int(expected_seq)<<" - discard\n";
                uint8_t last = expected_seq - 1;
                Frame ack{}; memset(&ack,0,sizeof(ack));
                hdr_set_length(ack.hdr,0);
                ack.hdr.seq = last;
                ack.hdr.type = FrameType::ACK;
                ack.checksum = htonl(compute_checksum(ack.hdr,nullptr,0));
                sendto(sock, &ack, sizeof(FrameHeader)+sizeof(uint32_t), 0, (sockaddr*)&sender_addr, sender_len);
            }
        } else if(mode == "sr"){
            if(!sr_received[seq]){
                sr_received[seq] = true;
                sr_buffer[seq].assign(f.payload, f.payload + plen);
                cout << "[SR] Buffered seq="<<int(seq)<<"\n";
            } else {
                cout << "[SR] Duplicate seq="<<int(seq)<<" - ignored\n";
            }
            // send ACK for this seq
            Frame ack{}; memset(&ack,0,sizeof(ack));
            hdr_set_length(ack.hdr,0);
            ack.hdr.seq = seq;
            ack.hdr.type = FrameType::ACK;
            ack.checksum = htonl(compute_checksum(ack.hdr,nullptr,0));
            sendto(sock, &ack, sizeof(FrameHeader)+sizeof(uint32_t), 0, (sockaddr*)&sender_addr, sender_len);

            // deliver contiguous from expected_seq
            while(sr_received[expected_seq]){
                auto &v = sr_buffer[expected_seq];
                cout << "[SR] Deliver seq="<<int(expected_seq)<<" size="<<v.size()<<"\n";
                if(!v.empty()) cout.write((char*)v.data(), v.size()), cout<<"\n";
                sr_received[expected_seq]=false;
                sr_buffer[expected_seq].clear();
                expected_seq++;
            }
        } else {
            cerr << "Unknown mode\n";
            break;
        }
    }

    close(sock);
    return 0;
}
