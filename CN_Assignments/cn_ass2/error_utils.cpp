// error_utils.cpp
#include "error_utils.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <cstdint>

static std::mt19937 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());

vector<int> computeChecksum(const vector<int>& data) {
    uint32_t sum = 0;
    for (auto v : data) sum += (uint8_t)v;
    vector<int> out(4);
    out[0] = (sum >> 24) & 0xFF;
    out[1] = (sum >> 16) & 0xFF;
    out[2] = (sum >> 8) & 0xFF;
    out[3] = (sum) & 0xFF;
    return out;
}

bool verifyChecksum(const vector<int>& dataWithChecksum) {
    if (dataWithChecksum.size() < 4) return false;
    size_t n = dataWithChecksum.size();
    vector<int> body(dataWithChecksum.begin(), dataWithChecksum.begin() + n - 4);
    vector<int> cs = computeChecksum(body);
    for (int i = 0; i < 4; ++i) {
        if (cs[i] != dataWithChecksum[n - 4 + i]) return false;
    }
    return true;
}

// Convert bytes to bits (MSB first per byte)
vector<int> bytesToBits(const vector<uint8_t>& bytes) {
    vector<int> bits;
    bits.reserve(bytes.size() * 8);
    for (uint8_t b : bytes) {
        for (int i = 7; i >= 0; --i) bits.push_back((b >> i) & 1);
    }
    return bits;
}

vector<uint8_t> bitsToBytes(const vector<int>& bits) {
    vector<uint8_t> out;
    size_t n = bits.size();
    for (size_t i = 0; i < n; i += 8) {
        uint8_t ch = 0;
        for (size_t j = 0; j < 8 && i + j < n; ++j) {
            ch = (ch << 1) | (bits[i + j] & 1);
        }
        // pad last byte to 8 bits if needed (left shift)
        int used = (int)std::min((size_t)8, n - i);
        if (used < 8) ch <<= (8 - used);
        out.push_back(ch);
    }
    return out;
}

vector<int> stringToBits(const string& data) {
    vector<int> bits;
    for (unsigned char c : data) {
        for (int i = 7; i >= 0; --i) bits.push_back((c >> i) & 1);
    }
    return bits;
}

string bitsToString(const vector<int>& bits) {
    string s;
    for (size_t i = 0; i < bits.size(); i += 8) {
        unsigned char ch = 0;
        for (size_t j = 0; j < 8 && (i + j) < bits.size(); ++j) {
            ch = (ch << 1) | (bits[i + j] & 1);
        }
        s.push_back((char)ch);
    }
    return s;
}

vector<int> stringToPolynomial(const string& polyStr) {
    vector<int> out;
    for (char c : polyStr) if (c == '0' || c == '1') out.push_back(c - '0');
    return out;
}

// computeCRC - modulo-2 division: dataBits appended with zeros(gLen-1) -> remainder
vector<int> computeCRC(const vector<int>& dataBits, const vector<int>& generator) {
    if (generator.empty()) return {};
    vector<int> work = dataBits;
    int gLen = (int)generator.size();
    work.insert(work.end(), gLen - 1, 0);
    for (size_t i = 0; i + gLen <= work.size(); ++i) {
        if (work[i] == 1) {
            for (int j = 0; j < gLen; ++j) work[i + j] ^= generator[j];
        }
    }
    vector<int> remainder(work.end() - (gLen - 1), work.end());
    return remainder;
}

bool verifyCRC(const vector<int>& dataWithCRCbits, const vector<int>& generator) {
    if (generator.empty()) return false;
    vector<int> work = dataWithCRCbits;
    int gLen = (int)generator.size();
    for (size_t i = 0; i + gLen <= work.size(); ++i) {
        if (work[i] == 1) {
            for (int j = 0; j < gLen; ++j) work[i + j] ^= generator[j];
        }
    }
    // remainder should be all zeros
    for (size_t i = work.size() - (gLen - 1); i < work.size(); ++i) if (work[i] != 0) return false;
    return true;
}

void injectBitError(vector<uint8_t>& bytes, const string& mode, int burst_len) {
    if (bytes.empty()) return;
    std::uniform_int_distribution<> bitPos(0, (int)bytes.size() * 8 - 1);
    if (mode == "bit") {
        int pos = bitPos(rng);
        int bIdx = pos / 8;
        int bitIdx = pos % 8;
        bytes[bIdx] ^= (1 << (7 - bitIdx));
    } else if (mode == "burst") {
        int start = bitPos(rng);
        for (int k = 0; k < burst_len; ++k) {
            int p = (start + k) % (bytes.size() * 8);
            int bIdx = p / 8;
            int bitIdx = p % 8;
            bytes[bIdx] ^= (1 << (7 - bitIdx));
        }
    }
}

uint32_t bytesVectorToUint32(const vector<uint8_t>& v) {
    uint32_t x = 0;
    size_t n = v.size();
    for (size_t i = 0; i < n && i < 4; ++i) {
        x = (x << 8) | v[i];
    }
    return x;
}
