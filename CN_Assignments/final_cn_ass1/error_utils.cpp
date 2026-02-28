#include "error_utils.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <algorithm>
using namespace std;

vector<int> stringToBits(const string& data) {
    vector<int> bits;
    for (char c : data) {
        if (c == '0' || c == '1') {
            bits.push_back(c - '0');
        }
    }
    return bits;
}

string bitsToString(const vector<int>& bits) {
    string s;
    for (int i=0;i<bits.size();i++) {
        int bit=bits[i];
        s += (bit + '0');
    }
    return s;
}

// ============ Checksum =============

vector<int> computeChecksum(vector<int> data) {
    if (data.size() % 16 != 0) {
        size_t pad = 16 - (data.size() % 16);
        data.insert(data.end(), pad, 0);
    }

    vector<int> sum(16, 0);
    for (size_t i = 0; i < data.size(); i += 16) {
        int carry = 0;
        for (int j = 15; j >= 0; --j) {
            int temp = sum[j] + data[i + j] + carry;
            sum[j] = temp % 2;
            carry = temp / 2;
        }
        if (carry) {
            for (int j = 15; j >= 0; --j) {
                int temp = sum[j] + carry;
                sum[j] = temp % 2;
                carry = temp / 2;
                if (!carry) break;
            }
        }
    }

    for (int i=0;i<sum.size();i++)sum[i] = !sum[i];
    return sum;
}

bool verifyChecksum(vector<int> dataWithChecksum) {
    vector<int> checksum = computeChecksum(dataWithChecksum);
    for (int i=0;i<checksum.size();i++) {
        int bit=checksum[i];
        if (bit != 0) return false;
    }
    return true;
}

// ============ CRC =============

vector<int> stringToPolynomial(string poly) {
    vector<int> result;
    for(char c:poly){
        if(c=='0'|| c=='1')
            result.push_back(c - '0');
    }
    if (result.empty()){
        cerr << "Error:generator polynomial is empty";
        exit(1);
    }
    return result;
}

vector<int> computeCRC(vector<int> data, vector<int> generator) {
    vector<int> appendedData = data;
    appendedData.resize(data.size() + generator.size() - 1, 0);
    vector<int> remainder = appendedData;

    for (size_t i = 0; i < data.size(); ++i) {
        if (remainder[i] == 1) {
            for (size_t j = 0; j < generator.size(); ++j) {
                remainder[i + j] ^= generator[j];
            }
        }
    }

    vector<int> crc(remainder.end() - (generator.size() - 1), remainder.end());
    return crc;
}

bool verifyCRC(vector<int> dataWithCRC, vector<int> generator) {
    vector<int> remainder = dataWithCRC;
    for (size_t i = 0; i <= remainder.size() - generator.size(); ++i) {
        if (remainder[i] == 1) {
            for (size_t j = 0; j < generator.size(); ++j) {
                remainder[i + j] ^= generator[j];
            }
        }
    }

    for (size_t i = remainder.size() - (generator.size() - 1); i < remainder.size(); ++i) {
        if (remainder[i] != 0) return false;
    }
    return true;
}

// ============ Error Injection =============

void injectError(vector<int>& data, string errorType) {
    srand(time(0));
    if (errorType == "single") {
        int pos = rand() % data.size();
        data[pos] ^= 1;
    } else if (errorType == "double") {
        int pos1 = rand() % data.size();
        int pos2;
        do {
            pos2 = rand() % data.size();
        } while (pos1 == pos2);
        data[pos1] ^= 1;
        data[pos2] ^= 1;
    } else if (errorType == "odd") {
        for (int i = 0; i < 3; ++i) {
            int pos = rand() % data.size();
            data[pos] ^= 1;
        }
    } else if (errorType == "burst") {
        int start = rand() % (data.size() - 5);
        for (int i = 0; i < 5; ++i) {
            data[start + i] ^= 1;
        }
    }
}
