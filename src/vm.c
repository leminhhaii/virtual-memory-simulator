#include "vm.h"
#include "tlb.h"
#include "replacement.h"
#include <stdlib.h>
#include <string.h>

int vm_init(SimulatorState *state, SimulatorConfig *config) {
    if (!state || !config) {
        return -1;
    }
    state->config = *config;
    state->logical_clock = 0;
    
    // Allocate Page Table
    state->page_table = calloc(config->pages, sizeof(PageTableEntry));
    if (!state->page_table) {
        return -1;
    }
    
    // Allocate Frame Table
    state->frame_table = calloc(config->frames, sizeof(FrameEntry));
    if (!state->frame_table) {
        free(state->page_table);
        state->page_table = NULL;
        return -1;
    }
    
    // Allocate TLB Cache (only if tlb_size > 0)
    if (config->tlb_size > 0) {
        state->tlb = calloc(config->tlb_size, sizeof(TlbEntry));
        if (!state->tlb) {
            free(state->frame_table);
            free(state->page_table);
            state->frame_table = NULL;
            state->page_table = NULL;
            return -1;
        }
    } else {
        state->tlb = NULL;
    }
    
    // Reset Stats
    memset(&state->stats, 0, sizeof(SimulatorStats));
    
    // Initialize TLB entries (sets valid to false and frame_number to -1)
    tlb_init(state);
    
    return 0;
}

void vm_destroy(SimulatorState *state) {
    if (!state) return;
    if (state->page_table) {
        free(state->page_table);
        state->page_table = NULL;
    }
    if (state->frame_table) {
        free(state->frame_table);
        state->frame_table = NULL;
    }
    if (state->tlb) {
        free(state->tlb);
        state->tlb = NULL;
    }
}

int vm_access(SimulatorState *state, char op, uint32_t addr, int *tlb_hit, int *page_fault, int *evict_vpn, int *evict_frame, int *dirty_write_back, uint32_t *phys_addr) {
    if (!state) return -1;
    
    // 1. Bounds check
    uint64_t max_addr = (uint64_t)state->config.pages * state->config.page_size;
    if (addr >= max_addr) {
        return -1; // Out of bounds
    }
    
    // Decompose virtual address
    uint32_t vpn = addr / state->config.page_size;
    uint32_t offset = addr % state->config.page_size;
    
    // Initialize default return values
    if (tlb_hit) *tlb_hit = 0;
    if (page_fault) *page_fault = 0;
    if (evict_vpn) *evict_vpn = -1;
    if (evict_frame) *evict_frame = -1;
    if (dirty_write_back) *dirty_write_back = 0;
    
    int frame_num = -1;
    int hit_in_tlb = 0;
    
    // Increment general metrics
    state->stats.total_accesses++;
    if (op == 'R') {
        state->stats.total_reads++;
    } else {
        state->stats.total_writes++;
    }
    
    // 2. TLB Lookup
    if (state->config.tlb_size > 0 && state->tlb) {
        hit_in_tlb = tlb_lookup(state, vpn, &frame_num);
        if (hit_in_tlb) {
            if (tlb_hit) *tlb_hit = 1;
            state->stats.tlb_hits++;
        } else {
            state->stats.tlb_misses++;
        }
    } else {
        // TLB is disabled, but we still count as TLB miss
        state->stats.tlb_misses++;
    }
    
    // 3. Page Table Lookup (on TLB miss)
    if (!hit_in_tlb) {
        PageTableEntry *pte = &state->page_table[vpn];
        if (pte->present) {
            // Page Table Hit
            frame_num = pte->frame_number;
            
            // Insert into TLB
            if (state->config.tlb_size > 0 && state->tlb) {
                tlb_insert(state, vpn, frame_num);
            }
        } else {
            // Page Fault!
            if (page_fault) *page_fault = 1;
            state->stats.page_faults++;
            
            // Look for a free physical frame
            int free_frame = -1;
            for (uint32_t f = 0; f < state->config.frames; f++) {
                if (!state->frame_table[f].occupied) {
                    free_frame = (int)f;
                    break;
                }
            }
            
            if (free_frame != -1) {
                // Load page into free frame
                frame_num = free_frame;
                state->frame_table[frame_num].occupied = true;
                state->frame_table[frame_num].vpn = vpn;
                state->frame_table[frame_num].load_timestamp = state->logical_clock;
                
                pte->present = true;
                pte->frame_number = frame_num;
                pte->dirty = false;
                pte->referenced = false;
                
                // Insert into TLB
                if (state->config.tlb_size > 0 && state->tlb) {
                    tlb_insert(state, vpn, frame_num);
                }
            } else {
                // RAM is full, execute page replacement!
                int victim = choose_victim_frame(state);
                if (victim < 0) return -1; // Replacement failed
                
                uint32_t evicted_vpn = state->frame_table[victim].vpn;
                PageTableEntry *ev_pte = &state->page_table[evicted_vpn];
                
                if (evict_vpn) *evict_vpn = (int)evicted_vpn;
                if (evict_frame) *evict_frame = victim;
                state->stats.evictions++;
                
                if (ev_pte->dirty) {
                    if (dirty_write_back) *dirty_write_back = 1;
                    state->stats.dirty_write_backs++;
                }
                
                // Invalidate TLB entry for evicted page
                if (state->config.tlb_size > 0 && state->tlb) {
                    tlb_invalidate(state, evicted_vpn);
                }
                
                // Evict the page
                ev_pte->present = false;
                ev_pte->dirty = false;
                ev_pte->referenced = false;
                
                // Load the new page into this frame
                frame_num = victim;
                state->frame_table[frame_num].vpn = vpn;
                state->frame_table[frame_num].load_timestamp = state->logical_clock;
                
                pte->present = true;
                pte->frame_number = frame_num;
                pte->dirty = false;
                pte->referenced = false;
                
                // Insert new page mapping into TLB
                if (state->config.tlb_size > 0 && state->tlb) {
                    tlb_insert(state, vpn, frame_num);
                }
            }
        }
    }
    
    // 4. Update physical address
    if (phys_addr) {
        *phys_addr = (uint32_t)frame_num * state->config.page_size + offset;
    }
    
    // 5. Update page table flags and timestamps for accessed page
    PageTableEntry *pte = &state->page_table[vpn];
    pte->last_used_timestamp = state->logical_clock;
    pte->referenced = true;
    if (op == 'W') {
        pte->dirty = true;
    }
    
    // Increment logical clock
    state->logical_clock++;
    
    return 0;
}
