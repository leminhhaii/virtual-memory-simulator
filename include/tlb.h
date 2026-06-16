#ifndef TLB_H
#define TLB_H

#include "vm.h"

void tlb_init(SimulatorState *state);
int tlb_lookup(SimulatorState *state, uint32_t vpn, int *frame_num);
void tlb_insert(SimulatorState *state, uint32_t vpn, int frame_num);
void tlb_invalidate(SimulatorState *state, uint32_t vpn);

#endif // TLB_H
