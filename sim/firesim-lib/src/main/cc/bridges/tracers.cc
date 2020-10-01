//See LICENSE for license details
#ifdef TRACERSBRIDGEMODULE_struct_guard

#include "tracers.h"

#include <stdio.h>
#include <string.h>

tracers_t::tracers_t(
    simif_t *sim,
    TRACERSBRIDGEMODULE_struct * mmio_addrs,
    int tracerno) :
        bridge_driver_t(sim),
        mmio_addrs(mmio_addrs){
    //Biancolin: move into elaboration
}

tracers_t::~tracers_t() {
    free(this->mmio_addrs);
}

void tracers_t::init() {
    printf("tracers init\n");
    write(this->mmio_addrs->traceActive, false);
    write(this->mmio_addrs->sampleStep, 1000);
    write(this->mmio_addrs->initDone, true);
}

void tracers_t::tick() {
    if(!read(this->mmio_addrs->block)){
        return;
    }

//    if(invalid_cycles == 100000){
//        printf("tracers - 100000 invalid cycles\n");

        printf("tracers - li_valid: %x\n", read(this->mmio_addrs->li_valid));
        uint64_t addr = read(this->mmio_addrs->li_iaddr_upper);
        addr = addr << 32 | read(this->mmio_addrs->li_iaddr_lower);
        printf("tracers - li_iaddr: %x\n", addr);
//        printf("tracers - li_insn: %x\n", read(this->mmio_addrs->li_insn));
//        printf("tracers - li_priv: %x\n", read(this->mmio_addrs->li_priv));
//        printf("tracers - li_exception: %x\n", read(this->mmio_addrs->li_exception));
//        printf("tracers - li_interrupt: %x\n", read(this->mmio_addrs->li_interrupt));
//        printf("tracers - li_cause: %x\n", read(this->mmio_addrs->li_cause));
//        printf("tracers - li_tval: %x\n", read(this->mmio_addrs->li_tval));
        uint64_t li_cycle = read(this->mmio_addrs->li_cycle_upper);
        li_cycle = li_cycle << 32 | read(this->mmio_addrs->li_cycle_lower);
        printf("tracers - li_cycle: %d\n", li_cycle);
        uint64_t cycle = read(this->mmio_addrs->trace_cycle_upper);
        cycle = cycle << 32 | read(this->mmio_addrs->trace_cycle_lower);
        printf("tracers - cycle: %d\n", cycle);
//        exit(-1);
//    }

    // this read hs the sideeffect of progressing simulation -> execute last
    uint32_t li = read(this->mmio_addrs->lastInstruction);
    bool valid = li & 0x1;
//    uint32_t addr = li ^ valid;
    if(valid){
        //printf("tracers %x\n", addr);
        invalid_cycles = 0;
     } else{
        //printf("tracers invalid\n");
        invalid_cycles++;
    }

}


void tracers_t::finish() {
}

#endif // TRACERSBRIDGEMODULE_struct_guard
