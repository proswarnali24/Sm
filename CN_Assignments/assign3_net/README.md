# CSMA/CD p-persistent Simulator (C++)

Build:

```
mkdir -p build && cd build
cmake .. && cmake --build .
```

Run:

```
./csma_cd --nodes 10 --p 0.5 --slots 100000 --arrival 0.01 --min 32 --max 64 --mbe 10 --seed 42
```

Outputs basic metrics: generated frames, started transmissions, successes, collisions, dropped, throughput per slot, and average delay (in slots).
