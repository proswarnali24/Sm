#pragma once
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <deque>
#include <vector>
#include <string>
#include <limits>

// ------------------ Simulation Configuration ------------------
struct SimConfig {
    int N = 16;                     // total nodes (1 server + N-1 clients)
    int SERVER_ID = 0;              // server node id
    long long SIM_SLOTS = 200000;   // total time slots

    // traffic
    double lambda_per_client = 0.02; // request arrival probability per slot per client
    int REQ_BYTES = 1500;            // request size (bytes)
    bool enable_ack = false;         // server replies?
    int ACK_BITS = 64;               // ACK size (bits)
    int SERVER_PROC_SLOTS = 0;       // server processing delay (slots)

    // medium parameters
    int SLOT_BITS = 512;             // slot duration (in bits)
    int JAM_SLOTS = 1;               // jam signal duration (slots)
    int MAX_BEB = 10;                // max backoff exponent
    double p = 0.5;                  // p-persistence
    double collision_inject = 0.0;   // random collision chance
    unsigned long long seed = 42;    // RNG seed

    // sweep options
    double pmin = -1, pmax = -1, pstep = -1;
    std::string csv_out = "results.csv";
};

// --------------- Helpers ----------------
inline int bytes_to_bits(int b) { return b * 8; }
inline int slots_for_bits(int bits, int SLOT_BITS) {
    return (bits + SLOT_BITS - 1) / SLOT_BITS;
}
inline bool bernoulli(double p, std::mt19937_64 &rng) {
    if (p <= 0) return false;
    if (p >= 1) return true;
    std::bernoulli_distribution d(p);
    return d(rng);
}
inline int randint(int lo, int hi, std::mt19937_64 &rng) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}
