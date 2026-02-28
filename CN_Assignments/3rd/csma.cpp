#include "csma.h"

Medium::Medium(const SimConfig& cfg, std::mt19937_64& rng)
: c_(cfg), rng_(rng), nodes_(cfg.N) {}

bool Medium::idle(long long t) const {
    return (!channel_busy_ || t >= channel_busy_until_);
}

void Medium::push_frame(int node, const Frame& f) {
    nodes_[node].q.push_back(f);
}

void Medium::finish_events(long long t) {
    for (int i = 0; i < c_.N; i++) {
        auto &nd = nodes_[i];

        if (nd.jam_ends_at == t) {
            nd.jam_ends_at = 0;
            int mstage = std::min(c_.MAX_BEB, nd.backoff_stage);
            int cw = (1 << mstage) - 1;
            int k = (cw > 0) ? randint(0, cw, rng_) : 0;
            nd.next_attempt_when = t + k + 1;
            nd.is_deferring = true;
        }

        if (nd.tx_ends_at == t) {
            nd.is_transmitting = false;
            nd.backoff_stage = 0;
            complete_success(i, t);
        }
    }
    if (!(channel_busy_ && t < channel_busy_until_))
        channel_busy_ = false;
}

void Medium::complete_success(int i, long long t) {
    auto &nd = nodes_[i];
    if (nd.q.empty()) return;
    Frame fr = nd.q.front();
    nd.q.pop_front();

    long long delay = t - fr.arrival_ts;
    if (delay >= 0) {
        M_.frames_total++;
        M_.bits_total += fr.bits;
        M_.sum_delay_total += delay;

        if (fr.dst == c_.SERVER_ID) {
            M_.frames_cli2srv++;
            M_.bits_cli2srv += fr.bits;
            M_.sum_delay_cli2srv += delay;
        } else if (fr.src == c_.SERVER_ID) {
            M_.frames_srv2cli++;
            M_.bits_srv2cli += fr.bits;
            M_.sum_delay_srv2cli += delay;
            
        }
        
    }
    
}

void Medium::on_collision(const std::vector<int>& starters, long long t) {
    channel_busy_ = true;
    channel_busy_until_ = t + c_.JAM_SLOTS;
    for (int id : starters) {
        auto &nd = nodes_[id];
        nd.collided_last = true;
        nd.backoff_stage = std::min(c_.MAX_BEB, nd.backoff_stage + 1);
        nd.jam_ends_at = t + c_.JAM_SLOTS;
    }
}

void Medium::on_single_start(int id, long long t) {
    auto &nd = nodes_[id];
    nd.is_transmitting = true;
    int bits = c_.SLOT_BITS;
    if (!nd.q.empty()) bits = nd.q.front().bits;
    int tx_slots = slots_for_bits(bits, c_.SLOT_BITS);
    nd.tx_ends_at = t + tx_slots;
    channel_busy_ = true;
    channel_busy_until_ = nd.tx_ends_at;
}

void Medium::attempt_starts(long long t) {
    std::vector<int> starters;
    bool idle_now = idle(t);

    for (int i = 0; i < c_.N; i++) {
        auto &nd = nodes_[i];
        if (nd.q.empty() || nd.q.front().arrival_ts > t) continue;
        if (nd.is_deferring && t < nd.next_attempt_when) continue;
        if (nd.is_deferring && t >= nd.next_attempt_when) nd.is_deferring = false;

        if (idle_now) {
            if (bernoulli(c_.p, rng_)) starters.push_back(i);
            else { nd.is_deferring = true; nd.next_attempt_when = t + 1; }
        } else {
            nd.is_deferring = true; nd.next_attempt_when = t + 1;
        }
    }

    if (starters.empty()) return;
    bool collided = (starters.size() > 1);
    if (!collided && bernoulli(c_.collision_inject, rng_)) collided = true;
    if (collided) on_collision(starters, t);
    else on_single_start(starters.front(), t);
}

void Medium::tick(long long t) {
    finish_events(t);
    attempt_starts(t);
}
