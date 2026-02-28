#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
using namespace std;

// Map bit {0,1} to BPSK symbol {-1,+1}
inline int bit_to_bpsk(int b) { return b ? +1 : -1; }
inline int bpsk_to_bit(long long x) { return x >= 0 ? 1 : 0; }

// Join integers into a string for printing
inline string join_ints(const vector<int>& v, const string& sep = " ") {
    string s;
    for (size_t i = 0; i < v.size(); ++i) {
        s += to_string(v[i]);
        if (i + 1 < v.size()) s += sep;
    }
    return s;
}

// Convert string like "10110" to vector<int>{1,0,1,1,0}
inline vector<int> parse_bits_string(const string& s) {
    vector<int> bits;
    bits.reserve(s.size());
    for (char c : s)
        if (c == '0' || c == '1')
            bits.push_back(c - '0');
    return bits;
}

// Recursive Hadamard matrix generation (power-of-2 size)
inline vector<vector<int>> hadamard(int n) {
    if (n == 1) return {{1}};
    if (n & (n - 1)) throw runtime_error("Walsh size must be a power of 2");
    auto H = hadamard(n / 2);
    vector<vector<int>> M(n, vector<int>(n));
    for (int i = 0; i < n / 2; ++i) {
        for (int j = 0; j < n / 2; ++j) {
            int a = H[i][j];
            M[i][j] = a;               // top-left
            M[i][j + n / 2] = a;       // top-right
            M[i + n / 2][j] = a;       // bottom-left
            M[i + n / 2][j + n / 2] = -a; // bottom-right
        }
    }
    return M;
}

// Return Walsh matrix (same as Hadamard)
inline vector<vector<int>> walsh(int n) { return hadamard(n); }

#endif // COMMON_H