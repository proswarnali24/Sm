// sender.cpp  — FIXED (no <bits/stdc++.h> and safe string literals)
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <algorithm>
using namespace std;

#include "cdma_lib.h"

static void usage_sender() {
    cerr << "Usage: ./sender <n> <W> <bits_comma>\n"
         << "  Example: ./sender 4 4 1011,0110,1100,0001\n";
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 4) { usage_sender(); return 1; }
    int n = stoi(argv[1]);
    int W = stoi(argv[2]);
    string bits_arg = argv[3];

    // Parse comma-separated bitstrings
    vector<vector<int>> bits_per_sender; bits_per_sender.reserve(n);
    {
        stringstream ss(bits_arg); string token; int count = 0;
        while (getline(ss, token, ',')) { bits_per_sender.push_back(parse_bits_string(token)); count++; }
        if (count != n) {
            cerr << "Error: expected " << n << " bit-strings, got " << count << "\n";
            return 2;
        }
    }
    // Validate equal lengths
    int T = (int)bits_per_sender[0].size();
    for (int i = 0; i < n; ++i) {
        if ((int)bits_per_sender[i].size() != T) {
            cerr << "All senders must have the same number of bits (T)\n";
            return 3;
        }
    }

    // Build channel and senders
    CDMAChannel ch(W, n);
    vector<unique_ptr<Sender>> senders;
    senders.reserve(n);
    for (int i = 0; i < n; ++i) senders.emplace_back(make_unique<Sender>(i, ch));

    // Superpose frames over T symbol periods
    vector<vector<int>> superposed_frames; superposed_frames.reserve(T);
    for (int t = 0; t < T; ++t) {
        vector<vector<int>> chips_each; chips_each.reserve(n);
        for (int i = 0; i < n; ++i)
            chips_each.push_back(senders[i]->encode_bit(bits_per_sender[i][t]));
        superposed_frames.push_back(superpose(chips_each));
    }

    // Write channel.bin (W, T, then T×W integers)
    ofstream ofs("channel.bin", ios::binary);
    if (!ofs) { cerr << "Cannot open channel.bin for write\n"; return 4; }
    ofs.write((char*)&W, sizeof(int));
    ofs.write((char*)&T, sizeof(int));
    for (int t = 0; t < T; ++t)
        ofs.write((char*)superposed_frames[t].data(), sizeof(int) * W);

    // Optional: human-readable dump
    ofstream meta("channel.txt");
    if (meta) {
        meta << "Walsh W=" << W << " n=" << n << " T=" << T << "\n";
        for (int i = 0; i < n; ++i)
            meta << "S" << i << " bits: " << join_ints(bits_per_sender[i], "") << "\n";
        for (int t = 0; t < T; ++t)
            meta << "t=" << t << " chips: " << join_ints(superposed_frames[t]) << "\n";
    }

    cerr << "[sender] Wrote channel.bin (W=" << W << ", T=" << T << ")\n";
    return 0;
}