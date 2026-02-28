// receiver.cpp — FIXED (no <bits/stdc++.h>, safe string literals)
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <algorithm>
using namespace std;

#include "cdma_lib.h"

static void usage_receiver() {
    cerr << "Usage: ./receiver <n> <W>\n"
         << "  Example: ./receiver 4 4\n";
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 3) { usage_receiver(); return 1; }
    int n = stoi(argv[1]);
    int W = stoi(argv[2]);

    ifstream ifs("channel.bin", ios::binary);
    if (!ifs) { cerr << "Cannot open channel.bin for read\n"; return 2; }

    int Wf = 0, T = 0;
    ifs.read(reinterpret_cast<char*>(&Wf), sizeof(int));
    ifs.read(reinterpret_cast<char*>(&T),  sizeof(int));
    if (Wf != W) {
        cerr << "Walsh size mismatch: file W=" << Wf << " arg W=" << W << "\n";
        return 3;
    }

    vector<vector<int>> superposed_frames(T, vector<int>(W));
    for (int t = 0; t < T; ++t) {
        ifs.read(reinterpret_cast<char*>(superposed_frames[t].data()),
                 sizeof(int) * W);
    }

    CDMAChannel ch(W, n);
    vector<unique_ptr<Receiver>> receivers;
    receivers.reserve(n);
    for (int i = 0; i < n; ++i)
        receivers.emplace_back(make_unique<Receiver>(i, ch));

    vector<vector<int>> decoded(n, vector<int>(T));
    for (int i = 0; i < n; ++i)
        for (int t = 0; t < T; ++t)
            decoded[i][t] = receivers[i]->decode_bit(superposed_frames[t]);

    cout << "Decoded bits at each receiver (n=" << n
         << ", W=" << W << ", T=" << T << ")\n";
    for (int i = 0; i < n; ++i) {
        cout << "R" << i << ": ";
        for (int b : decoded[i]) cout << b;
        cout << "\n";
    }
    return 0;
}