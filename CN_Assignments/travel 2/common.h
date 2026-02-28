#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <cstdint>

// Platform-specific network headers
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

// Assuming these are custom headers and located correctly relative to the source files.
#include "./crc.h"
#include "./checksum.h"


constexpr int PAYLOAD_SIZE = 46; // bytes
constexpr int MAX_FRAME_BYTES = 1600;

// default CRC type
static std::string DEFAULT_CRC = "CRC-32";

// Helper: convert bytes -> bitstring and back
inline std::string bytesToBitString(const std::string &data) {
    std::string bits;
    for (unsigned char c : data) {
        std::bitset<8> b(c);
        bits += b.to_string();
    }
    return bits;
}
inline std::string bitStringToBytes(const std::string &bits) {
    std::string out;
    size_t n = bits.size();
    for (size_t i = 0; i + 8 <= n; i += 8) {
        std::string byte_str = bits.substr(i, 8);
        char ch = static_cast<char>(std::stoi(byte_str, nullptr, 2));
        out.push_back(ch);
    }
    return out;
}

// append CRC remainder (using crc.h). dataBits expected to be '0'/'1' string
inline std::string appendCRC(const std::string &dataBits, const std::string &crcType = DEFAULT_CRC) {
    std::string divisor = get_divisor(crcType);
    std::string dataCopy = dataBits; // crc_remainder modifies
    std::string remainder = crc_remainder(dataCopy, divisor); // returns remainder of size m-1
    return dataBits + remainder;
}

// validate incoming bitstring using CRC divisor
inline bool validateCRC(const std::string &bitsWithRemainder, const std::string &crcType = DEFAULT_CRC) {
    std::string divisor = get_divisor(crcType);
    std::string copy = bitsWithRemainder;
    std::string remainder = crc_remainder(copy, divisor);
    return (remainder.find('1') == std::string::npos);
}

// If you prefer checksum instead, small wrappers (optional)
inline std::string appendChecksum(const std::string &dataBits, int blockSize=8) {
    std::string checksum = generate_checksum(dataBits, blockSize);
    return dataBits + checksum;
}
inline bool validateChecksum(std::vector<std::string> &packets, std::string checksum) {
    return validate_checksum(packets, checksum);
}

// MAC helper: default MAC addresses
inline std::string defaultMAC(const std::string &label) {
    // returns 6-byte string (non-printable allowed)
    if (label == "sender") return std::string("\xAA\xBB\xCC\x00\x00\x01", 6);
    else return std::string("\xAA\xBB\xCC\x00\x00\x02", 6);
}

// Utility: print bits debug
inline std::string bitsPreview(const std::string &bits, int maxChars=64) {
    if (bits.size() <= static_cast<size_t>(maxChars)) return bits;
    return bits.substr(0, maxChars) + "...";
}

// Stats struct
struct Stats {
    std::uint64_t bytesSent = 0;
    std::uint64_t bytesReceived = 0;
    std::uint64_t framesSent = 0;
    std::uint64_t framesReceived = 0;
    std::vector<double> rtts;
    void print() {
        std::cout << "Frames sent: " << framesSent << ", frames received: " << framesReceived << "\n";
        std::cout << "Bytes sent: " << bytesSent << ", bytes received: " << bytesReceived << "\n";
        if (!rtts.empty()) {
            double sum=0;
            for (double v: rtts) sum+=v;
            std::cout << "Average RTT (ms): " << (sum / rtts.size()) << "\n";
        }
    }
};
