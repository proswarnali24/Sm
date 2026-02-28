#include "common.h"
#include "frame.h"
#include "csma.h"

// Enqueue ACK/reply after processing delay
void enqueue_server_reply(int server_id, int client_id, long long now,
                          const SimConfig& c, Medium& medium) {
    Frame ack;
    ack.arrival_ts = now + c.SERVER_PROC_SLOTS;
    ack.bits = c.ACK_BITS;
    ack.src = server_id;
    ack.dst = client_id;
    ack.is_reply = true;
    medium.push_frame(server_id, ack);
}
