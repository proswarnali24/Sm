#ifndef ERROR_UTILS_H
#define ERROR_UTILS_H

#include <vector>
#include <string>
using namespace std;

// Checksum functions
vector<int> computeChecksum(vector<int> data);
bool verifyChecksum(vector<int> dataWithChecksum);

// CRC functions
vector<int> computeCRC(vector<int> data, vector<int> generator);
bool verifyCRC(vector<int> dataWithCRC, vector<int> generator);
vector<int> stringToPolynomial(string polyStr);

// Error injection
void injectError(vector<int>& data, string errorType);

// Conversion
vector<int> stringToBits(const string& data);
string bitsToString(const vector<int>& bits);

#endif
