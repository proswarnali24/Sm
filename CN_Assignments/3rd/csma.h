#pragma once
#include "common.h"
#include "frame.h"

// ---------------- Node State ----------------
struct NodeState {
    std::deque<Frame> q;
    bool is_transmitting = false;
    bool is_deferring = false;
    bool collided_last = false;
    int backoff_stage = 0;
    long long tx_ends_at = 0;
    long long jam_ends_at = 0;
    long long next_attempt_when = 0;
};

// ---------------- Metrics ----------------
struct Metrics {
    long long bits_total = 0;
    long long frames_total = 0;
    long double sum_delay_total = 0;
    long long bits_cli2srv = 0;
    long long frames_cli2srv = 0;
    long double sum_delay_cli2srv = 0;
    long long bits_srv2cli = 0;
    long long frames_srv2cli = 0;
    long double sum_delay_srv2cli = 0;
};

// ---------------- Medium Class ----------------
class Medium {
public:
    Medium(const SimConfig& cfg, std::mt19937_64& rng);
    void tick(long long t);
    void push_frame(int node, const Frame& f);
    const Metrics& metrics() const { return M_; }

private:
    const SimConfig& c_;
    std::mt19937_64& rng_;
    std::vector<NodeState> nodes_;
    bool channel_busy_ = false;
    long long channel_busy_until_ = -1;
    Metrics M_;

    bool idle(long long t) const;
    void finish_events(long long t);
    void attempt_starts(long long t);
    void complete_success(int i, long long t);
    void on_collision(const std::vector<int>& starters, long long t);
    void on_single_start(int id, long long t);
};
