// CSE/PC/B/S/314 - Assignment 3: p-persistent CSMA/CD Simulator
// Language: C++17

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "common.hpp"
#include "sender.hpp"
#include "receiver.hpp"
using namespace std;

static void printUsage(const char *prog) {
	cerr << "Usage: " << prog << " [options]\n";
	cerr << "Options:\n";
	cerr << "  --nodes N                 Number of nodes (default 10)\n";
	cerr << "  --p P                     p-persistent probability in (0,1] (default 0.5)\n";
	cerr << "  --lambda FPS              Arrival rate per node (frames/sec) (default 50)\n";
	cerr << "  --frame-bits B            Frame size in bits (default 12000)\n";
	cerr << "  --slot-bits B             Slot time in bits (default 512)\n";
	cerr << "  --jam-bits B              Jam signal length in bits (default 48)\n";
	cerr << "  --bitrate R               Bit rate in bps (default 10e6)\n";
	cerr << "  --time S                  Simulation time in seconds (default 1.0)\n";
	cerr << "  --seed X                  RNG seed (default 12345)\n";
	cerr << "  --ext-coll P              External collision prob per bit (default 0)\n";
	cerr << "  --sweep p1,p2,...         Sweep p values and output CSV\n";
	cerr << "  --csv PATH                CSV output path (default results.csv)\n";
	cerr << "\nMetrics reported per run: throughput (bits/sec), avg_delay (seconds), frames\n";
}

static RunResult runOnce(const Config &cfg) {
	Sender sender(cfg);
	Receiver receiver(cfg);
	(void)receiver; // placeholder to keep the receiver in structure
	RunResult r = sender.run();
	return r;
}

int main(int argc, char **argv) {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	Config cfg;
	// Defaults adjusted for more stable stats
	cfg.simSeconds = 5.0;
	cfg.lambdaFps = 100.0;
	cfg.frameBits = 12000;

	for (int i = 1; i < argc; ++i) {
		string arg = argv[i];
		auto need = [&](int more) {
			if (i + more >= argc) {
				printUsage(argv[0]);
				exit(1);
			}
		};
		if (arg == "--help" || arg == "-h") {
			printUsage(argv[0]);
			return 0;
		} else if (arg == "--nodes") {
			need(1); cfg.numNodes = stoi(argv[++i]);
		} else if (arg == "--p") {
			need(1); cfg.p = stod(argv[++i]);
		} else if (arg == "--lambda") {
			need(1); cfg.lambdaFps = stod(argv[++i]);
		} else if (arg == "--frame-bits") {
			need(1); cfg.frameBits = stoi(argv[++i]);
		} else if (arg == "--slot-bits") {
			need(1); cfg.slotTimeBits = stoi(argv[++i]);
		} else if (arg == "--jam-bits") {
			need(1); cfg.jamBits = stoi(argv[++i]);
		} else if (arg == "--bitrate") {
			need(1); cfg.bitRate = stod(argv[++i]);
		} else if (arg == "--time") {
			need(1); cfg.simSeconds = stod(argv[++i]);
		} else if (arg == "--seed") {
			need(1); cfg.seed = stoull(argv[++i]);
		} else if (arg == "--ext-coll") {
			need(1); cfg.externalCollisionProb = stod(argv[++i]);
		} else if (arg == "--sweep") {
			need(1); cfg.sweep = true; string vals = argv[++i];
			cfg.sweepPValues.clear();
			stringstream ss(vals);
			string tok;
			while (getline(ss, tok, ',')) {
				if (!tok.empty()) cfg.sweepPValues.push_back(stod(tok));
			}
		} else if (arg == "--csv") {
			need(1); cfg.csvPath = argv[++i];
		} else {
			cerr << "Unknown option: " << arg << "\n";
			printUsage(argv[0]);
			return 1;
		}
	}

	if (!cfg.sweep) {
		RunResult r = runOnce(cfg);
		cout.setf(std::ios::fixed); cout<<setprecision(6);
		cout << "throughput_bps," << r.throughputBps << "\n";
		cout << "avg_delay_sec," << r.avgDelaySec << "\n";
		cout << "frames," << r.frames << "\n";
		return 0;
	}

	// Sweep mode
	vector<double> pvals = cfg.sweepPValues;
	if (pvals.empty()) {
		for (double p = 0.05; p <= 1.0001; p += 0.05) pvals.push_back(p);
	}
	ofstream csv(cfg.csvPath);
	if (!csv) {
		cerr << "Failed to open CSV path: " << cfg.csvPath << "\n";
		return 2;
	}
	csv.setf(std::ios::fixed); csv<<setprecision(6);
	csv << "p,throughput_bps,avg_delay_sec,frames\n";
	for (double p : pvals) {
		Config runCfg = cfg;
		runCfg.p = p;
		RunResult r = runOnce(runCfg);
		csv << p << "," << r.throughputBps << "," << r.avgDelaySec << "," << r.frames << "\n";
	}
	csv.close();
	cout << "CSV written to " << cfg.csvPath << "\n";
	return 0;
}


