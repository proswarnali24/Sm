#pragma once
#include "common.h"
#include <string>
#include <cstdint>
#include <cstring> // For memcpy

struct Frame {
    std::string srcMac; // 6 bytes
    std::string dstMac; // 6 bytes
    uint16_t length; // payload length in bytes // 2 bytes
    uint8_t seqno; //1 bytes
    char type; // 'D' data, 'A' ack, 'N' nak // 1bytes
    std::string payload; // raw bytes 

    Frame() {
        srcMac = defaultMAC("sender");
        dstMac = defaultMAC("receiver");
        length = 0;
        seqno = 0;
        type = 'D';
        payload = "";
    }

    // serialize header + payload to raw bytes (no CRC appended here)
    std::string toBytesNoFCS() const {
        std::string out;
        out += srcMac;
        out += dstMac;
        uint16_t netlen = htons(length);
        out.append(reinterpret_cast<const char*>(&netlen), sizeof(netlen));
        out.push_back(static_cast<char>(seqno));
        out.push_back(type);
        out += payload;
        return out;
    }

    // convert to bitstring, append CRC remainder bits and return bytes suitable for UDP send
    std::string toBytesWithFCS(const std::string &crcType = DEFAULT_CRC) const {
        std::string raw = toBytesNoFCS();
        std::string bits = bytesToBitString(raw);
        std::string withFcsBits = appendCRC(bits, crcType);
        std::string bytes = bitStringToBytes(withFcsBits);
        return bytes;
    }

    // Build Frame object from raw bytes received (with CRC at end) - validate CRC first externally
    static bool fromBytesWithFCS(const std::string &bytes, Frame &out, const std::string &crcType = DEFAULT_CRC) {
        // convert bytes -> bits
        std::string bits = bytesToBitString(bytes);
        std::string divisor = get_divisor(crcType);
        // validate using crc_remainder: we expect remainder==0
        std::string copy = bits;
        std::string remainder = crc_remainder(copy, divisor);
        if (remainder.find('1') != std::string::npos) {
            return false; // CRC failed
        }
        // To reconstruct header and payload, we need to remove remainder bits (m-1)
        size_t m = divisor.size();
        size_t remainderLen = m - 1;
        std::string pureBits = bits.substr(0, bits.size() - remainderLen);
        // convert back to bytes
        std::string raw = bitStringToBytes(pureBits);
        // parse fields
        if (raw.size() < 6+6+2+1+1) return false;
        out.srcMac = raw.substr(0,6);
        out.dstMac = raw.substr(6,6);
        uint16_t netlen;
        memcpy(&netlen, raw.data()+12, 2);
        out.length = ntohs(netlen);
        out.seqno = static_cast<uint8_t>(raw[14]);
        out.type = raw[15];
        if (raw.size() < 16 + out.length) return false;
        out.payload = raw.substr(16, out.length);
        return true;
    }
};
