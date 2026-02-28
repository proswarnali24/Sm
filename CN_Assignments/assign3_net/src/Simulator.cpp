#include "Simulator.hpp"
#include <algorithm>

Simulator::Simulator(const SimulationConfig& config)
	: cfg(config), rng(config.rngSeed) {
	init();
}

void Simulator::init() {
	nodes.clear();
	nodes.reserve(cfg.numNodes);
	for (int i = 0; i < cfg.numNodes; ++i) {
		nodes.emplace_back(i, cfg, rng);
	}
}

SimulationStats Simulator::run() {
	for (std::uint64_t slot = 0; slot < static_cast<std::uint64_t>(cfg.totalSlots); ++slot) {
		// New arrivals
		for (auto& node : nodes) {
			if (node.maybeGenerateFrame(slot)) {
				stats.generatedFrames += 1;
			}
		}

		// Advance backoff counters
		for (auto& node : nodes) node.onSlotAdvance();

		if (channelBusy) {
			remainingTxSlots -= 1;
			if (remainingTxSlots <= 0) {
				// Transmission finished successfully
				channelBusy = false;
				stats.successfulTransmissions += 1;
				stats.totalDelaySlots += static_cast<std::uint64_t>(slot - currentFrame.createdAtSlot);
				transmittingNodeId = -1;
			}
			continue; // Channel is busy; others defer
		}

		// Channel idle: contenders may attempt with probability p
		std::vector<int> contenders;
		contenders.reserve(nodes.size());
		for (auto& node : nodes) {
			if (node.decideAction(false) == NodeAction::AttemptTransmit) {
				contenders.push_back(node.getId());
			}
		}

		if (contenders.empty()) {
			continue; // idle slot
		}

		if (contenders.size() >= 2) {
			// Collision
			stats.collisions += 1;
			for (int nid : contenders) {
				nodes[nid].onCollision();
			}
			// Jam for one slot
			channelBusy = true;
			remainingTxSlots = 1;
			continue;
		}

		// Exactly one contender: start transmission
		int nid = contenders.front();
		Node& n = nodes[nid];
		Frame& f = n.peekFrame();
		n.onSuccessfulStart();
		n.popFrame();
		stats.startedTransmissions += 1;
		channelBusy = true;
		remainingTxSlots = f.sizeSlots;
		transmittingNodeId = nid;
		currentFrame = f;
	}
	return stats;
}
