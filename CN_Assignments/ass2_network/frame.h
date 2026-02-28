// frame.h
#pragma once

#include <cstdint>
#include <cstring>
#include <random>
#include <thread>
#include <chrono>
#include <arpa/inet.h> // htonl/ntohl
#include <iostream>

static const int MAC_LEN = 6;
static const int MAX_PAYLOAD = 1500;
static const int MIN_PAYLOAD = 46;

enum FrameType : uint8_t {
    DATA = 1,
    ACK  = 2,
    NAK  = 3
};

#pragma pack(push,1)
struct FrameHeader {
    uint8_t src[MAC_LEN];
    uint8_t dst[MAC_LEN];
    uint16_t length;    // payload length (network order)
    uint8_t seq;        // sequence number
    uint8_t type;       // FrameType
};
#pragma pack(pop)

#pragma pack(push,1)
struct Frame {
    FrameHeader hdr;
    uint8_t payload[MAX_PAYLOAD];
    uint32_t checksum; // network order
};
#pragma pack(pop)

// helpers
inline void hdr_set_length(FrameHeader &h, uint16_t len) { h.length = htons(len); }
inline uint16_t hdr_get_length(const FrameHeader &h) { return ntohs(h.length); }

// compute checksum by summing raw bytes of header + payload (deterministic)
inline uint32_t compute_checksum(const FrameHeader &hdr, const uint8_t *payload, uint16_t len) {
    uint32_t sum = 0;
    const uint8_t *hdr_bytes = reinterpret_cast<const uint8_t*>(&hdr);
    for(size_t i=0;i<sizeof(FrameHeader);++i) sum += hdr_bytes[i];
    for(size_t i=0;i<len;++i) sum += payload[i];
    return sum;
}

// Channel sim params and helpers (used to simulate drops/delays/corruption)
struct ChannelParams {
    double drop_prob = 0.0;
    double bit_error_prob = 0.0;
    double max_delay_ms = 0.0;
    std::mt19937 rng;
    ChannelParams() : rng(std::random_device{}()) {}
};

inline bool simulate_drop(ChannelParams &p) {
    std::uniform_real_distribution<double> d(0.0,1.0);
    return d(p.rng) < p.drop_prob;
}
inline void simulate_delay(ChannelParams &p) {
    if(p.max_delay_ms <= 0.0) return;
    std::uniform_real_distribution<double> dd(0.0, p.max_delay_ms);
    int ms = (int)dd(p.rng);
    if(ms>0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline void simulate_bit_error(Frame &f, uint16_t len, ChannelParams &p) {
    std::uniform_real_distribution<double> d(0.0,1.0);
    if(d(p.rng) >= p.bit_error_prob) return;
    if(len==0) return;
    std::uniform_int_distribution<int> idx(0, (int)len - 1);
    int i = idx(p.rng);
    f.payload[i] ^= 0xFF;
}
