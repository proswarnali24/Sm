#pragma once
#include "Types.hpp"
#include <queue>
#include <random>
enum class NodeAction { Defer, AttemptTransmit };
class Node {
public:
	Node(int nodeId, const SimulationConfig& config, std::mt19937& sharedRng);
	bool maybeGenerateFrame(std::uint64_t currentSlot);
	NodeAction decideAction(bool channelBusy) const;
	void onCollision();
	void onSuccessfulStart();
	void onSlotAdvance();
	bool hasData() const;
	Frame& peekFrame();
	void popFrame();
	int getId() const { return id; }
private:
	int id;
	const SimulationConfig& cfg;
	std::queue<Frame> queueFrames;
	int collisionCount;
	int backoffCounterSlots;
	std::mt19937& rng;
	std::bernoulli_distribution arrivalBernoulli;
	std::uniform_int_distribution<int> frameLenDist;
	mutable std::bernoulli_distribution pPersistBernoulli;
	int frameIdCounter = 0;
	int computeContentionWindow() const;
	int pickBackoffSlots(int contentionWindow);
};
