#pragma once

#include <deque>
#include <random>
#include "common.hpp"

struct Frame {
	long long enqueueTime;
	unsigned long long id;
};

struct NodeMetrics {
	long long totalSuccessfulBits = 0;
	long long totalSuccessfulFrames = 0;
	long long totalDelayBits = 0;
};

struct Stats {
	long long totalBitsDelivered = 0;
	long long totalFramesDelivered = 0;
	long long totalDelayBits = 0;
};

class Sender {
public:
	explicit Sender(const Config &cfg);
	RunResult run();

private:
	struct Node {
		std::deque<Frame> queue;
		bool isTransmitting = false;
		bool isJamming = false;
		bool isBackingOff = false;
		long long txRemaining = 0;
		long long jamRemaining = 0;
		long long backoffRemaining = 0;
		int collisionsForCurrentFrame = 0;
		std::mt19937_64 rng;
		std::uniform_real_distribution<double> uni{0.0, 1.0};
		NodeMetrics metrics;
	};

	struct Channel { int activeTxCount = 0; bool collisionOngoing = false; };

	Config cfg;
	std::vector<Node> nodes;
	Channel ch;
	std::mt19937_64 masterRng;
	std::uniform_real_distribution<double> uni{0.0, 1.0};
	double perBitArrivalProb = 0.0;
	long long totalBitsToSimulate = 0;

	void enqueueArrivals(long long nowBit);
	void startTransmissionIfAllowed(Node &node, long long nowBit);
	void detectAndHandleCollisions();
	void injectExternalCollisions();
	void processTransmissions(long long nowBit);
	void tryNewTransmissions(long long nowBit);
	void step(long long nowBit);
};


