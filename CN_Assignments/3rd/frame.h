#pragma once
struct Frame {
    long long arrival_ts = 0; // when frame enters queue
    int bits = 0;             // frame length in bits
    int src = -1;             // sender node id
    int dst = -1;             // receiver node id
    bool is_reply = false;    // true if server->client
};
