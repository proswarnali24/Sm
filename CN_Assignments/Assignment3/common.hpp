#pragma once

#include <string>
#include <vector>

struct Config {
	int numNodes = 10;
	double p = 0.5;
	double lambdaFps = 50.0;
	int frameBits = 1500 * 8;
	int jamBits = 48;
	int slotTimeBits = 512;
	double simSeconds = 1.0;
	double bitRate = 10e6;
	double externalCollisionProb = 0.0;
	unsigned long long seed = 12345;
	bool sweep = false;
	std::vector<double> sweepPValues;
	std::string csvPath = "results.csv";
};

struct RunResult {
	double throughputBps;
	double avgDelaySec;
	long long frames;
};


