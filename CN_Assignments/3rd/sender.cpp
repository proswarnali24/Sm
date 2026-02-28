#include "common.h"
#include "frame.h"
#include "csma.h"

// Forward decl
void enqueue_server_reply(int server_id, int client_id, long long now,
                          const SimConfig& c, Medium& medium);

// ---------------- Client traffic generator ----------------
static void generate_client_arrivals(long long t, const SimConfig& c,
                                     Medium& medium, std::mt19937_64& rng) {
    for (int i = 0; i < c.N; i++) {
        if (i == c.SERVER_ID) continue;
        if (bernoulli(c.lambda_per_client, rng)) {
            Frame f;
            f.arrival_ts = t;
            f.bits = bytes_to_bits(c.REQ_BYTES);
            f.src = i;
            f.dst = c.SERVER_ID;
            f.is_reply = false;
            medium.push_frame(i, f);
        }
    }
}

// ---------------- CSV helpers ----------------
static void write_csv_header(std::ofstream& fout) {
    fout << "p,thr_total_bits_per_slot,thr_cli2srv_bits_per_slot,"
            "thr_srv2cli_bits_per_slot,avg_delay_cli2srv_slots,"
            "avg_delay_srv2cli_slots,efficiency\n";
}

static void write_csv_row(std::ofstream& fout, double p,
                          const Metrics& M, const SimConfig& c) {
    double T = (double)M.bits_total / (double)c.SIM_SLOTS;
    double Tc2s = (double)M.bits_cli2srv / (double)c.SIM_SLOTS;
    double Ts2c = (double)M.bits_srv2cli / (double)c.SIM_SLOTS;
    double Dc2s = (M.frames_cli2srv > 0)
                    ? (double)(M.sum_delay_cli2srv / M.frames_cli2srv)
                    : std::numeric_limits<double>::quiet_NaN();
    double Ds2c = (M.frames_srv2cli > 0)
                    ? (double)(M.sum_delay_srv2cli / M.frames_srv2cli)
                    : std::numeric_limits<double>::quiet_NaN();
    double eff = T / (double)c.SLOT_BITS;

    fout << std::fixed << std::setprecision(6)
         << p << "," << T << "," << Tc2s << "," << Ts2c << ","
         << Dc2s << "," << Ds2c << "," << eff << "\n";
}

// ---------------- Simulation run ----------------
static void run_once(SimConfig c, double p, std::ofstream& fout) {
    c.p = p;
    std::mt19937_64 rng(c.seed);
    Medium medium(c, rng);

    for (long long t = 0; t < c.SIM_SLOTS; ++t) {
        generate_client_arrivals(t, c, medium, rng);
        medium.tick(t);
    }

    write_csv_row(fout, p, medium.metrics(), c);
}

// ---------------- CLI parser ----------------
static SimConfig parse_args(int argc, char** argv) {
    SimConfig c;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto need = [&](const char* f){ return std::string(argv[++i]); };
        if (a == "--nodes") c.N = std::stoi(need("--nodes"));
        else if (a == "--server") c.SERVER_ID = std::stoi(need("--server"));
        else if (a == "--time") c.SIM_SLOTS = std::stoll(need("--time"));
        else if (a == "--lambda") c.lambda_per_client = std::stod(need("--lambda"));
        else if (a == "--frame") c.REQ_BYTES = std::stoi(need("--frame"));
        else if (a == "--slotbits") c.SLOT_BITS = std::stoi(need("--slotbits"));
        else if (a == "--p") c.p = std::stod(need("--p"));
        else if (a == "--pmin") c.pmin = std::stod(need("--pmin"));
        else if (a == "--pmax") c.pmax = std::stod(need("--pmax"));
        else if (a == "--pstep") c.pstep = std::stod(need("--pstep"));
        else if (a == "--ack") c.enable_ack = true;
        else if (a == "--ackbits") c.ACK_BITS = std::stoi(need("--ackbits"));
        else if (a == "--srvproc") c.SERVER_PROC_SLOTS = std::stoi(need("--srvproc"));
        else if (a == "--inj") c.collision_inject = std::stod(need("--inj"));
        else if (a == "--seed") c.seed = std::stoull(need("--seed"));
        else if (a == "--csv") c.csv_out = need("--csv");
    }
    return c;
}

// ---------------- MAIN ----------------
int main(int argc, char** argv) {
    SimConfig c = parse_args(argc, argv);
    std::ofstream fout(c.csv_out);
    write_csv_header(fout);

    if (c.pmin > 0 && c.pmax > 0 && c.pstep > 0) {
        for (double p = c.pmin; p <= c.pmax + 1e-12; p += c.pstep)
            run_once(c, p, fout);
    } else {
        run_once(c, c.p, fout);
    }

    std::cout << "Simulation finished. Results saved in " << c.csv_out << "\n";
    return 0;
}
