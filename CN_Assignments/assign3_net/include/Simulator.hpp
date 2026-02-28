#pragma once
#include "Types.hpp"
#include "Node.hpp"
#include <vector>
#include <random>
class Simulator {
public:
	explicit Simulator(const SimulationConfig& config);
	SimulationStats run();
private:
	const SimulationConfig cfg;
	SimulationStats stats;
	std::mt19937 rng;
	std::vector<Node> nodes;
	bool channelBusy = false;
	int remainingTxSlots = 0;
	int transmittingNodeId = -1;
	Frame currentFrame{};
	void init();
};
