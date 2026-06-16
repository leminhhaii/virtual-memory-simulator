#include "tlb.h"
#include <string.h>
#include <limits.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

void tlb_init(SimulatorState *state) {
    if (!state || !state->tlb || state->config.tlb_size == 0) return;
    for (uint32_t i = 0; i < state->config.tlb_size; i++) {
        state->tlb[i].valid = false;
        state->tlb[i].vpn = 0;
        state->tlb[i].frame_number = -1;
        state->tlb[i].last_used_timestamp = 0;
        state->tlb[i].load_timestamp = 0;
    }
}

int tlb_lookup(SimulatorState *state, uint32_t vpn, int *frame_num) {
    if (!state || !state->tlb || state->config.tlb_size == 0) return 0;
    for (uint32_t i = 0; i < state->config.tlb_size; i++) {
        if (state->tlb[i].valid && state->tlb[i].vpn == vpn) {
            state->tlb[i].last_used_timestamp = state->logical_clock;
            if (frame_num) {
                *frame_num = state->tlb[i].frame_number;
            }
            return 1; // TLB Hit
        }
    }
    return 0; // TLB Miss
}

void tlb_insert(SimulatorState *state, uint32_t vpn, int frame_num) {
    if (!state || !state->tlb || state->config.tlb_size == 0) return;
    
    // Check if the VPN is already in the TLB (update if found)
    for (uint32_t i = 0; i < state->config.tlb_size; i++) {
        if (state->tlb[i].valid && state->tlb[i].vpn == vpn) {
            state->tlb[i].frame_number = frame_num;
            state->tlb[i].last_used_timestamp = state->logical_clock;
            return;
        }
    }
    
    // Find an invalid slot first
    for (uint32_t i = 0; i < state->config.tlb_size; i++) {
        if (!state->tlb[i].valid) {
            state->tlb[i].valid = true;
            state->tlb[i].vpn = vpn;
            state->tlb[i].frame_number = frame_num;
            state->tlb[i].load_timestamp = state->logical_clock;
            state->tlb[i].last_used_timestamp = state->logical_clock;
            return;
        }
    }
    
    // TLB is full, execute replacement
    int victim_idx = 0;
    if (strcasecmp(state->config.algorithm, "lru") == 0) {
        // LRU replacement
        uint32_t min_time = UINT32_MAX;
        for (uint32_t i = 0; i < state->config.tlb_size; i++) {
            if (state->tlb[i].last_used_timestamp < min_time) {
                min_time = state->tlb[i].last_used_timestamp;
                victim_idx = (int)i;
              }
        }
    } else {
        // FIFO replacement (default for FIFO, Clock, or anything else)
        uint32_t min_time = UINT32_MAX;
        for (uint32_t i = 0; i < state->config.tlb_size; i++) {
            if (state->tlb[i].load_timestamp < min_time) {
                min_time = state->tlb[i].load_timestamp;
                victim_idx = (int)i;
            }
        }
    }
    
    // Replace the victim
    state->tlb[victim_idx].valid = true;
    state->tlb[victim_idx].vpn = vpn;
    state->tlb[victim_idx].frame_number = frame_num;
    state->tlb[victim_idx].load_timestamp = state->logical_clock;
    state->tlb[victim_idx].last_used_timestamp = state->logical_clock;
}

void tlb_invalidate(SimulatorState *state, uint32_t vpn) {
    if (!state || !state->tlb || state->config.tlb_size == 0) return;
    for (uint32_t i = 0; i < state->config.tlb_size; i++) {
        if (state->tlb[i].valid && state->tlb[i].vpn == vpn) {
            state->tlb[i].valid = false;
            break;
        }
    }
}
