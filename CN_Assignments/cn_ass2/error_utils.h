// error_utils.h
#ifndef ERROR_UTILS_H
#define ERROR_UTILS_H

#include <vector>
#include <string>

using namespace std;

// Checksum functions (byte-vector style)
vector<int> computeChecksum(const vector<int>& data);
bool verifyChecksum(const vector<int>& dataWithChecksum);

// CRC functions (bit-level)
vector<int> computeCRC(const vector<int>& dataBits, const vector<int>& generator);
bool verifyCRC(const vector<int>& dataWithCRCbits, const vector<int>& generator);
vector<int> stringToPolynomial(const string& polyStr);

// Conversion helpers
vector<int> bytesToBits(const vector<uint8_t>& bytes);
vector<uint8_t> bitsToBytes(const vector<int>& bits);
vector<int> stringToBits(const string& data);
string bitsToString(const vector<int>& bits);

// Error injection
void injectBitError(vector<uint8_t>& bytes, const string& mode, int burst_len = 1);

// Small utility
uint32_t bytesVectorToUint32(const vector<uint8_t>& v);

#endif
