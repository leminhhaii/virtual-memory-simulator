#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdbool.h>

/* Page Table Entry: Represents mapping details for each virtual page */
typedef struct {
    bool present;                  /* true if page is mapped to a physical frame */
    bool dirty;                    /* true if modified since loaded */
    bool referenced;               /* true if accessed recently (used for Clock) */
    int frame_number;              /* index of the physical frame it maps to */
    uint32_t last_used_timestamp;  /* logical clock tick of last access (used for LRU) */
    uint32_t access_count;         /* total access frequency count (used for LFU) */
} PageTableEntry;

/* Frame Entry: Tracks physical memory allocations */
typedef struct {
    bool occupied;                 /* true if frame is currently allocated */
    uint32_t vpn;                  /* Virtual Page Number (VPN) mapped to this frame */
    uint32_t load_timestamp;       /* logical clock tick when loaded (used for FIFO) */
} FrameEntry;

/* TLB Entry: Represents cache lines for translation lookups */
typedef struct {
    bool valid;                    /* true if entry is active */
    uint32_t vpn;                  /* cached Virtual Page Number */
    int frame_number;              /* cached physical frame mapping */
    uint32_t last_used_timestamp;  /* logical clock tick of last access (LRU cache eviction) */
    uint32_t load_timestamp;       /* logical clock tick of cache loading (FIFO cache eviction) */
} TlbEntry;

/* SimulatorStats: Metric counters for hardware events */
typedef struct {
    uint32_t total_accesses;
    uint32_t total_reads;
    uint32_t total_writes;
    uint32_t tlb_hits;
    uint32_t tlb_misses;
    uint32_t page_faults;
    uint32_t evictions;
    uint32_t dirty_write_backs;
} SimulatorStats;

/* SimulatorConfig: Initialized parameters from CLI */
typedef struct {
    uint32_t pages;
    uint32_t frames;
    uint32_t page_size;
    uint32_t tlb_size;
    char algorithm[16];
    char trace_path[256];
} SimulatorConfig;

/* SimulatorState: Global simulator state package */
typedef struct {
    PageTableEntry *page_table;
    FrameEntry *frame_table;
    TlbEntry *tlb;
    uint32_t logical_clock;
    SimulatorConfig config;
    SimulatorStats stats;
} SimulatorState;

/* API Functions */
int vm_init(SimulatorState *state, SimulatorConfig *config);
void vm_destroy(SimulatorState *state);
int vm_access(SimulatorState *state, char op, uint32_t addr, int *tlb_hit, int *page_fault, int *evict_vpn, int *evict_frame, int *dirty_write_back, uint32_t *phys_addr);

#endif // VM_H
