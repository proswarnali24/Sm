#include "Simulator.hpp"
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
	SimulationConfig cfg;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		auto next = [&](int& target){ if (i + 1 < argc) { target = std::stoi(argv[++i]); }};
		auto nextd = [&](double& target){ if (i + 1 < argc) { target = std::stod(argv[++i]); }};
		if (arg == "--nodes") next(cfg.numNodes);
		else if (arg == "--p") nextd(cfg.persistenceP);
		else if (arg == "--slots") next(cfg.totalSlots);
		else if (arg == "--arrival") nextd(cfg.arrivalRatePerNode);
		else if (arg == "--min") next(cfg.minFrameSizeSlots);
		else if (arg == "--max") next(cfg.maxFrameSizeSlots);
		else if (arg == "--mbe") next(cfg.maxBackoffExponent);
		else if (arg == "--seed") { int tmp = 0; next(tmp); cfg.rngSeed = static_cast<unsigned int>(tmp); }
		else if (arg == "--help") {
			std::cout << "Usage: csma_cd [--nodes N] [--p P] [--slots S] [--arrival R] [--min A] [--max B] [--mbe K] [--seed Z]\n";
			return 0;
		}
	}
	Simulator sim(cfg);
	SimulationStats stats = sim.run();
	std::cout << "Generated: " << stats.generatedFrames << "\n";
	std::cout << "Started:   " << stats.startedTransmissions << "\n";
	std::cout << "Success:   " << stats.successfulTransmissions << "\n";
	std::cout << "Collisions:" << stats.collisions << "\n";
	std::cout << "Dropped:   " << stats.droppedFrames << "\n";
	double throughput = (cfg.totalSlots > 0) ? static_cast<double>(stats.successfulTransmissions) / cfg.totalSlots : 0.0;
	double avgDelay = (stats.successfulTransmissions > 0) ? static_cast<double>(stats.totalDelaySlots) / stats.successfulTransmissions : 0.0;
	std::cout << "Throughput/slot: " << throughput << "\n";
	std::cout << "Avg delay (slots): " << avgDelay << "\n";
	return 0;
}
