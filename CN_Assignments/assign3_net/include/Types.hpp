#pragma once
#include <cstdint>
struct Frame {
	int id;
	int sourceId;
	int sizeSlots;
	std::uint64_t createdAtSlot;
};
struct SimulationConfig {
	int numNodes = 10;
	double persistenceP = 0.5;
	int totalSlots = 100000;
	double arrivalRatePerNode = 0.01;
	int minFrameSizeSlots = 32;
	int maxFrameSizeSlots = 64;
	int maxBackoffExponent = 10;
	unsigned int rngSeed = 42;
};
struct SimulationStats {
	std::uint64_t generatedFrames = 0;
	std::uint64_t startedTransmissions = 0;
	std::uint64_t successfulTransmissions = 0;
	std::uint64_t collisions = 0;
	std::uint64_t droppedFrames = 0;
	std::uint64_t totalDelaySlots = 0;
};
