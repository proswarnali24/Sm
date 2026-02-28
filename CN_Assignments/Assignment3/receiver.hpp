#pragma once

#include "common.hpp"

class Receiver {
public:
	explicit Receiver(const Config &cfg) : cfg(cfg) {}

	// In this simulator, the receiver is implicit on the bus. This class is kept
	// to match the sender-receiver structure and could be extended for per-node
	// receive-side logging or error checks.
	void onDelivery() {}

private:
	Config cfg;
};


