#include "sender.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

static unsigned long long globalFrameIdSender = 0ULL;

static inline bool eventHappens(std::mt19937_64 &rng, std::uniform_real_distribution<double> &uni, double prob) {
	if (prob <= 0.0) return false;
	if (prob >= 1.0) return true;
	return uni(rng) < prob;
}

static inline long long secondsToBits(double seconds, double bitRate) {
	double bits = seconds * bitRate;
	return (long long) llround(bits);
}

Sender::Sender(const Config &config) : cfg(config) {
	masterRng.seed(cfg.seed);
	nodes.resize(cfg.numNodes);
	for (int i = 0; i < cfg.numNodes; ++i) {
		unsigned long long s = (cfg.seed + 0x9e3779b97f4a7c15ULL * (i + 1)) ^ (0xBF58476D1CE4E5B9ULL * (i + 123));
		nodes[i].rng.seed(s);
	}
	perBitArrivalProb = cfg.lambdaFps / cfg.bitRate;
	totalBitsToSimulate = secondsToBits(cfg.simSeconds, cfg.bitRate);
}

void Sender::enqueueArrivals(long long nowBit) {
	for (int i = 0; i < cfg.numNodes; ++i) {
		while (eventHappens(nodes[i].rng, nodes[i].uni, perBitArrivalProb)) {
			Frame f{nowBit, ++globalFrameIdSender};
			nodes[i].queue.push_back(f);
		}
	}
}

void Sender::startTransmissionIfAllowed(Node &node, long long nowBit) {
	if (node.queue.empty()) return;
	if (node.isTransmitting || node.isJamming || node.isBackingOff) return;
	if (ch.activeTxCount > 0 || ch.collisionOngoing) return;
	if (nowBit % cfg.slotTimeBits != 0) return;
	if (!eventHappens(node.rng, node.uni, cfg.p)) return;
	node.isTransmitting = true;
	node.txRemaining = cfg.frameBits;
	ch.activeTxCount += 1;
}

void Sender::detectAndHandleCollisions() {
	if (ch.activeTxCount > 1) {
		ch.collisionOngoing = true;
	}
}

void Sender::injectExternalCollisions() {
	if (cfg.externalCollisionProb <= 0.0) return;
	if (ch.activeTxCount == 0) return;
	if (uni(masterRng) < cfg.externalCollisionProb) {
		ch.collisionOngoing = true;
	}
}

void Sender::processTransmissions(long long nowBit) {
	if (ch.collisionOngoing) {
		for (auto &node : nodes) {
			if (node.isTransmitting) {
				node.isTransmitting = false;
				node.txRemaining = 0;
				node.isJamming = true;
				node.jamRemaining = cfg.jamBits;
				node.collisionsForCurrentFrame = std::min(node.collisionsForCurrentFrame + 1, 16);
			}
		}
		ch.activeTxCount = 0;
		ch.collisionOngoing = false;
	}

	for (auto &node : nodes) {
		if (node.isJamming) {
			if (node.jamRemaining > 0) node.jamRemaining -= 1;
			if (node.jamRemaining <= 0) {
				node.isJamming = false;
				int m = std::min(node.collisionsForCurrentFrame, 10);
				unsigned long long W = 1ULL << m;
				std::uniform_int_distribution<long long> dist(0, (long long)W - 1);
				long long k = dist(node.rng);
				node.isBackingOff = true;
				node.backoffRemaining = k * (long long)cfg.slotTimeBits;
			}
			continue;
		}
		if (node.isBackingOff) {
			if (node.backoffRemaining > 0) node.backoffRemaining -= 1;
			if (node.backoffRemaining <= 0) {
				node.isBackingOff = false;
			}
			continue;
		}
		if (node.isTransmitting) {
			if (node.txRemaining > 0) node.txRemaining -= 1;
			if (node.txRemaining <= 0) {
				node.isTransmitting = false;
				ch.activeTxCount = std::max(0, ch.activeTxCount - 1);
				if (!node.queue.empty()) {
					Frame f = node.queue.front();
					node.queue.pop_front();
					node.metrics.totalSuccessfulBits += cfg.frameBits;
					node.metrics.totalSuccessfulFrames += 1;
					node.metrics.totalDelayBits += (nowBit - f.enqueueTime);
				}
				node.collisionsForCurrentFrame = 0;
			}
		}
	}
}

void Sender::tryNewTransmissions(long long nowBit) {
	if (ch.activeTxCount == 0) {
		if (nowBit % cfg.slotTimeBits == 0) {
			for (auto &node : nodes) {
				if (!node.isBackingOff && !node.isJamming) {
					startTransmissionIfAllowed(node, nowBit);
				}
			}
		}
	}
}

void Sender::step(long long nowBit) {
	enqueueArrivals(nowBit);
	injectExternalCollisions();
	detectAndHandleCollisions();
	processTransmissions(nowBit);
	tryNewTransmissions(nowBit);
}

RunResult Sender::run() {
	for (long long t = 0; t < totalBitsToSimulate; ++t) {
		step(t);
	}
	Stats s{};
	for (auto &n : nodes) {
		s.totalBitsDelivered += n.metrics.totalSuccessfulBits;
		s.totalFramesDelivered += n.metrics.totalSuccessfulFrames;
		s.totalDelayBits += n.metrics.totalDelayBits;
	}
	RunResult r{};
	r.throughputBps = (cfg.simSeconds > 0.0) ? (double)s.totalBitsDelivered / cfg.simSeconds : 0.0;
	r.avgDelaySec = (s.totalFramesDelivered > 0) ? ((double)s.totalDelayBits / (double)s.totalFramesDelivered) / cfg.bitRate : 0.0;
	r.frames = s.totalFramesDelivered;
	return r;
}


