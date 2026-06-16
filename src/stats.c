#include "stats.h"
#include <stdio.h>
#include <string.h>

void stats_init(SimulatorStats *stats) {
    if (stats) {
        memset(stats, 0, sizeof(SimulatorStats));
    }
}

void stats_print(SimulatorState *state) {
    if (!state) return;
    SimulatorStats *stats = &state->stats;
    
    double tlb_hit_rate = 0.0;
    if (stats->total_accesses > 0) {
        tlb_hit_rate = (double)stats->tlb_hits / stats->total_accesses * 100.0;
    }
    
    double page_fault_rate = 0.0;
    if (stats->total_accesses > 0) {
        page_fault_rate = (double)stats->page_faults / stats->total_accesses * 100.0;
    }
    
    printf("--- Simulation Statistics ---\n");
    printf("Total Accesses: %u\n", stats->total_accesses);
    printf("Total Reads: %u\n", stats->total_reads);
    printf("Total Writes: %u\n", stats->total_writes);
    printf("TLB Hits: %u\n", stats->tlb_hits);
    printf("TLB Misses: %u\n", stats->tlb_misses);
    printf("Page Faults: %u\n", stats->page_faults);
    printf("Evictions: %u\n", stats->evictions);
    printf("Dirty Write-backs: %u\n", stats->dirty_write_backs);
    printf("TLB Hit Rate: %.2f%%\n", tlb_hit_rate);
    printf("Page Fault Rate: %.2f%%\n", page_fault_rate);
}
