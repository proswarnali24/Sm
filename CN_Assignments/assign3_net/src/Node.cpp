#include "Node.hpp"
#include <algorithm>

Node::Node(int nodeId, const SimulationConfig& config, std::mt19937& sharedRng)
	: id(nodeId), cfg(config), collisionCount(0), backoffCounterSlots(0), rng(sharedRng),
	  arrivalBernoulli(config.arrivalRatePerNode),
	  frameLenDist(config.minFrameSizeSlots, config.maxFrameSizeSlots),
	  pPersistBernoulli(config.persistenceP) {}

bool Node::maybeGenerateFrame(std::uint64_t currentSlot) {
	if (arrivalBernoulli(rng)) {
		Frame f{frameIdCounter++, id, frameLenDist(rng), currentSlot};
		queueFrames.push(f);
		return true;
	}
	return false;
}

NodeAction Node::decideAction(bool channelBusy) const {
	if (!hasData()) return NodeAction::Defer;
	if (backoffCounterSlots > 0) return NodeAction::Defer;
	if (channelBusy) return NodeAction::Defer;
	return pPersistBernoulli(rng) ? NodeAction::AttemptTransmit : NodeAction::Defer;
}

void Node::onCollision() {
	collisionCount = std::min(collisionCount + 1, cfg.maxBackoffExponent);
	int cw = computeContentionWindow();
	backoffCounterSlots = pickBackoffSlots(cw);
}

void Node::onSuccessfulStart() {
	collisionCount = 0;
	backoffCounterSlots = 0;
}

void Node::onSlotAdvance() {
	if (backoffCounterSlots > 0) {
		backoffCounterSlots -= 1;
	}
}

bool Node::hasData() const { return !queueFrames.empty(); }

Frame& Node::peekFrame() { return queueFrames.front(); }

void Node::popFrame() { queueFrames.pop(); }

int Node::computeContentionWindow() const {
	int k = collisionCount;
	int window = (1 << k) - 1;
	if (window < 0) window = 0;
	return window + 1;
}

int Node::pickBackoffSlots(int contentionWindow) {
	if (contentionWindow <= 1) return 0;
	std::uniform_int_distribution<int> dist(0, contentionWindow - 1);
	return dist(rng);
}
