#include "replacement.h"
#include <string.h>
#include <limits.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

// Static clock hand variable to track state across victim selection calls
static uint32_t clock_hand = 0;

int choose_victim_frame(SimulatorState *state) {
    if (!state) return -1;
    
    uint32_t frames_count = state->config.frames;
    
    if (strcasecmp(state->config.algorithm, "fifo") == 0) {
        // Find occupied frame with minimum load_timestamp
        int victim = -1;
        uint32_t min_timestamp = UINT32_MAX;
        for (uint32_t i = 0; i < frames_count; i++) {
            if (state->frame_table[i].occupied) {
                if (state->frame_table[i].load_timestamp < min_timestamp) {
                    min_timestamp = state->frame_table[i].load_timestamp;
                    victim = (int)i;
                }
            }
        }
        return victim;
    } 
    else if (strcasecmp(state->config.algorithm, "lru") == 0) {
        // Find occupied frame mapping to a page with minimum last_used_timestamp
        int victim = -1;
        uint32_t min_timestamp = UINT32_MAX;
        for (uint32_t i = 0; i < frames_count; i++) {
            if (state->frame_table[i].occupied) {
                uint32_t vpn = state->frame_table[i].vpn;
                uint32_t last_used = state->page_table[vpn].last_used_timestamp;
                if (last_used < min_timestamp) {
                    min_timestamp = last_used;
                    victim = (int)i;
                }
            }
        }
        return victim;
    } 
    else if (strcasecmp(state->config.algorithm, "clock") == 0) {
        // Clock page replacement policy
        while (1) {
            uint32_t current_frame = clock_hand;
            // Advance clock hand to next frame before returning or checking next
            clock_hand = (clock_hand + 1) % frames_count;
            
            if (state->frame_table[current_frame].occupied) {
                uint32_t vpn = state->frame_table[current_frame].vpn;
                if (state->page_table[vpn].referenced) {
                    state->page_table[vpn].referenced = false;
                } else {
                    // Found victim (referenced bit is 0)
                    return (int)current_frame;
                }
            }
        }
    }
    
    return -1;
}
