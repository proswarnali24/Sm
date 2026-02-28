#ifndef CDMA_LIB_H
#define CDMA_LIB_H
#include "common.h"
#include "frame.h"


class CDMAChannel {
int W_;
vector<vector<int>> codes_;
public:
CDMAChannel(int W, int n);
int W() const { return W_; }
const vector<int>& code_for(int station_id) const { return codes_.at(station_id); }
const vector<vector<int>>& codes() const { return codes_; }
};


class Sender {
int id_{}; const CDMAChannel& ch_;
public:
Sender(int id, const CDMAChannel& ch): id_(id), ch_(ch) {}
vector<int> encode_bit(int bit) const; // returns W chips
};


class Receiver {
int id_{}; const CDMAChannel& ch_;
public:
Receiver(int id, const CDMAChannel& ch): id_(id), ch_(ch) {}
int decode_bit(const vector<int>& superposed_chips) const; // returns {0,1}
};


// Superpose chips from multiple senders for one symbol
vector<int> superpose(const vector<vector<int>>& per_sender_chips);


#endif // CDMA_LIB_H