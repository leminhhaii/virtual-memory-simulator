#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "vm.h"
#include "trace.h"
#include "stats.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s --pages <count> --frames <count> --page-size <bytes> --tlb-size <count> --algorithm <fifo|lru|clock> <trace_file>\n", prog_name);
}

int main(int argc, char *argv[]) {
    SimulatorConfig config;
    memset(&config, 0, sizeof(SimulatorConfig));
    
    bool has_pages = false;
    bool has_frames = false;
    bool has_page_size = false;
    bool has_tlb_size = false;
    bool has_algorithm = false;
    char *trace_path = NULL;
    
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--pages") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for --pages\n");
                return 1;
            }
            char *endptr;
            long val = strtol(argv[i+1], &endptr, 10);
            if (*endptr != '\0' || val <= 0) {
                fprintf(stderr, "Error: --pages must be a positive integer\n");
                return 1;
            }
            config.pages = (uint32_t)val;
            has_pages = true;
            i += 2;
        } else if (strcmp(argv[i], "--frames") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for --frames\n");
                return 1;
            }
            char *endptr;
            long val = strtol(argv[i+1], &endptr, 10);
            if (*endptr != '\0' || val <= 0) {
                fprintf(stderr, "Error: --frames must be a positive integer\n");
                return 1;
            }
            config.frames = (uint32_t)val;
            has_frames = true;
            i += 2;
        } else if (strcmp(argv[i], "--page-size") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for --page-size\n");
                return 1;
            }
            char *endptr;
            long val = strtol(argv[i+1], &endptr, 10);
            if (*endptr != '\0' || val <= 0) {
                fprintf(stderr, "Error: --page-size must be a positive integer\n");
                return 1;
            }
            // Bitwise check for Power-of-Two: (val & (val - 1)) == 0
            if ((val & (val - 1)) != 0) {
                fprintf(stderr, "Error: --page-size must be a power of two\n");
                return 1;
            }
            config.page_size = (uint32_t)val;
            has_page_size = true;
            i += 2;
        } else if (strcmp(argv[i], "--tlb-size") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for --tlb-size\n");
                return 1;
            }
            char *endptr;
            long val = strtol(argv[i+1], &endptr, 10);
            if (*endptr != '\0' || val < 0) {
                fprintf(stderr, "Error: --tlb-size must be a non-negative integer\n");
                return 1;
            }
            config.tlb_size = (uint32_t)val;
            has_tlb_size = true;
            i += 2;
        } else if (strcmp(argv[i], "--algorithm") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing value for --algorithm\n");
                return 1;
            }
            const char *algo = argv[i+1];
            if (strcasecmp(algo, "fifo") != 0 && strcasecmp(algo, "lru") != 0 && strcasecmp(algo, "clock") != 0) {
                fprintf(stderr, "Error: --algorithm must be fifo, lru, or clock\n");
                return 1;
            }
            strncpy(config.algorithm, algo, sizeof(config.algorithm) - 1);
            config.algorithm[sizeof(config.algorithm) - 1] = '\0';
            has_algorithm = true;
            i += 2;
        } else {
            if (argv[i][0] == '-') {
                fprintf(stderr, "Error: Unrecognized option '%s'\n", argv[i]);
                print_usage(argv[0]);
                return 1;
            }
            if (trace_path != NULL) {
                fprintf(stderr, "Error: Multiple trace files specified\n");
                return 1;
            }
            trace_path = argv[i];
            i++;
        }
    }
    
    if (!has_pages || !has_frames || !has_page_size || !has_tlb_size || !has_algorithm || !trace_path) {
        fprintf(stderr, "Error: Missing mandatory command line configurations\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Explicit reject of zero-valued configurations
    if (config.pages == 0 || config.frames == 0 || config.page_size == 0) {
        fprintf(stderr, "Error: Pages, Frames, and Page-Size parameters must be greater than zero\n");
        return 1;
    }
    
    strncpy(config.trace_path, trace_path, sizeof(config.trace_path) - 1);
    config.trace_path[sizeof(config.trace_path) - 1] = '\0';
    
    // Initialize simulation state and memory tables
    SimulatorState state;
    if (vm_init(&state, &config) != 0) {
        fprintf(stderr, "Error: Failed to allocate simulation data structures\n");
        return 1;
    }
    
    // Open trace stream
    TraceFile trace;
    if (trace_open(&trace, config.trace_path) != 0) {
        fprintf(stderr, "Error: Could not open trace file '%s'\n", config.trace_path);
        vm_destroy(&state);
        return 1;
    }
    
    char op;
    uint32_t addr;
    int line_num;
    int read_status;
    
    // Loop through trace operations
    while ((read_status = trace_read_next(&trace, &op, &addr, &line_num)) > 0) {
        uint32_t current_clock = state.logical_clock;
        int tlb_hit = 0;
        int page_fault = 0;
        int evict_vpn = -1;
        int evict_frame = -1;
        int dirty_write_back = 0;
        uint32_t phys_addr = 0;
        
        int status = vm_access(&state, op, addr, &tlb_hit, &page_fault, &evict_vpn, &evict_frame, &dirty_write_back, &phys_addr);
        if (status != 0) {
            fprintf(stderr, "Error: Address translation failed (out of bounds) for address 0x%X at line %d\n", addr, line_num);
            trace_close(&trace);
            vm_destroy(&state);
            return 1;
        }
        
        uint32_t vpn = addr / config.page_size;
        uint32_t offset = addr % config.page_size;
        
        printf("[%02u] %s VA=0x%04X VPN=%u OFFSET=%u\n",
               current_clock, (op == 'R' ? "READ" : "WRITE"), addr, vpn, offset);
        
        if (tlb_hit) {
            printf("     TLB hit -> PA=0x%04X\n", phys_addr);
        } else {
            if (page_fault) {
                if (evict_vpn != -1) {
                    printf("     TLB miss -> page fault -> RAM full\n");
                    if (dirty_write_back) {
                        printf("     evict VPN=%d from FRAME=%d -> dirty write-back\n", evict_vpn, evict_frame);
                    } else {
                        printf("     evict VPN=%d from FRAME=%d\n", evict_vpn, evict_frame);
                    }
                } else {
                    printf("     TLB miss -> page fault\n");
                }
                int loaded_frame = (int)(phys_addr / config.page_size);
                printf("     load VPN=%u into FRAME=%d -> PA=0x%04X\n", vpn, loaded_frame, phys_addr);
            } else {
                printf("     TLB miss -> PA=0x%04X\n", phys_addr);
            }
        }
    }
    
    if (read_status < 0) {
        fprintf(stderr, "Error: Parser syntax error on line %d\n", line_num);
        trace_close(&trace);
        vm_destroy(&state);
        return 1;
    }
    
    trace_close(&trace);
    
    // Print stats
    stats_print(&state);
    
    // Deallocate buffers
    vm_destroy(&state);
    return 0;
}
