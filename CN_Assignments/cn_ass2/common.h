// common.h
#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <vector>
#include <string>

static const int PAYLOAD_SIZE = 46; // bytes per frame payload

enum FrameType : uint8_t {
    DATA = 0,
    ACK  = 1
};

enum DetectMethod : uint8_t {
    DET_CHECKSUM = 0,
    DET_CRC      = 1
};

#pragma pack(push,1)
struct FrameHeader {
    uint8_t srcAddr[6];
    uint8_t dstAddr[6];
    uint16_t payload_len; // original payload length in bytes (network order used on wire)
    uint8_t seqNo;        // sequence number
    uint8_t type;         // DATA or ACK
    uint8_t detect;       // DET_CHECKSUM or DET_CRC
    uint8_t crc_rem_bytes; // if detect==CRC, how many bytes of remainder appended
};
#pragma pack(pop)

struct Frame {
    FrameHeader header;
    std::vector<uint8_t> payload; // payload_len bytes
    // If checksum used: uint32_t checksum appended (4 bytes)
    // If CRC used: crc_rem_bytes bytes appended after payload (serialized)
    uint32_t checksum; // used only for checksum mode (host order)
};

std::vector<uint8_t> serializeFrame(const Frame &f);
Frame deserializeFrame(const std::vector<uint8_t> &buf);

#endif
