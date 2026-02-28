#include "cdma_lib.h"


CDMAChannel::CDMAChannel(int W, int n): W_(W) {
if (W < n) throw runtime_error("Walsh size must be >= number of stations");
codes_ = walsh(W);
}


vector<int> Sender::encode_bit(int bit) const {
const auto& c = ch_.code_for(id_);
vector<int> chips(ch_.W());
int b = bit_to_bpsk(bit);
for (int i=0;i<ch_.W();++i) chips[i] = b * c[i];
return chips;
}


int Receiver::decode_bit(const vector<int>& superposed_chips) const {
const auto& c = ch_.code_for(id_);
long long acc = 0;
for (int i=0;i<ch_.W();++i) acc += 1LL * superposed_chips[i] * c[i];
return bpsk_to_bit(acc);
}


vector<int> superpose(const vector<vector<int>>& per_sender_chips) {
if (per_sender_chips.empty()) return {};
int W = (int)per_sender_chips.front().size();
vector<int> s(W, 0);
for (const auto& v: per_sender_chips) {
if ((int)v.size() != W) throw runtime_error("Mismatched chip vector lengths");
for (int i=0;i<W;++i) s[i] += v[i];
}
return s;
}