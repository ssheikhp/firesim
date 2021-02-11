//See LICENSE for license details
#ifdef TRACERSBRIDGEMODULE_struct_guard

#include "tracers.h"

#include <stdio.h>
#include <string.h>

tracers_t::tracers_t(
    simif_t *sim,
    std::vector<std::string> &args,
    TRACERSBRIDGEMODULE_struct * mmio_addrs,
    int tracerno) :
        bridge_driver_t(sim),
        mmio_addrs(mmio_addrs){

    std::string suffix = std::string("=");
    std::string tracefile_arg = std::string("+tracefile") + suffix;
    std::string tracefreq_arg = std::string("+trace-freq") + suffix;
    std::string tracefreq_arg = std::string("+trace-freq") + suffix;
    const char *tracefilename = NULL;
    for (auto &arg: args) {
        if (arg.find(tracefile_arg) == 0) {
            tracefilename = const_cast<char*>(arg.c_str()) + tracefile_arg.length();
            this->tracefilename = std::string(tracefilename);
        }
        if (arg.find(tracefreq_arg) == 0) {
            char *str = const_cast<char*>(arg.c_str()) + tracefreq_arg.length();
            this->trace_freq = atol(str);
        }
    }
    if (tracefilename) {
        // giving no tracefilename means we will create NO tracefiles
        std::string tfname = std::string(tracefilename) + std::string("-C") + std::to_string(tracerno);
        this->tracefile = fopen(tfname.c_str(), "w");
        if (!this->tracefile) {
            fprintf(stderr, "Could not open Trace log file: %s\n", tracefilename);
            abort();
        }
    } else {
        fprintf(stderr, "TraceRV %d: Tracing disabled, since +tracefile was not provided.\n", tracerno);
        this->trace_enabled = false;
    }
}

tracers_t::~tracers_t() {
    if (this->tracefile) {
        fclose(this->tracefile);
    }
    free(this->mmio_addrs);
}

void tracers_t::init() {
    printf("tracers init\n");
    write(this->mmio_addrs->traceActive, this->trace_enabled);
    write(this->mmio_addrs->sampleStep, this->trace_freq);
    write(this->mmio_addrs->initDone, true);
}

void tracers_t::tick() {
    if(!this->trace_enabled || !read(this->mmio_addrs->block)){
        return;
    }

//    if(invalid_cycles == 100000){
//        printf("tracers - 100000 invalid cycles\n");

        fprintf(this->tracefile, "tracers - li_valid: %x\n", read(this->mmio_addrs->li_valid));
        uint64_t addr = read(this->mmio_addrs->li_iaddr_upper);
        addr = addr << 32 | read(this->mmio_addrs->li_iaddr_lower);
        fprintf(this->tracefile, "tracers - li_iaddr: %x\n", addr);
//        fprintf(this->tracefile, "tracers - li_insn: %x\n", read(this->mmio_addrs->li_insn));
//        fprintf(this->tracefile, "tracers - li_priv: %x\n", read(this->mmio_addrs->li_priv));
//        fprintf(this->tracefile, "tracers - li_exception: %x\n", read(this->mmio_addrs->li_exception));
//        fprintf(this->tracefile, "tracers - li_interrupt: %x\n", read(this->mmio_addrs->li_interrupt));
//        fprintf(this->tracefile, "tracers - li_cause: %x\n", read(this->mmio_addrs->li_cause));
//        fprintf(this->tracefile, "tracers - li_tval: %x\n", read(this->mmio_addrs->li_tval));
        uint64_t li_cycle = read(this->mmio_addrs->li_cycle_upper);
        li_cycle = li_cycle << 32 | read(this->mmio_addrs->li_cycle_lower);
        fprintf(this->tracefile, "tracers - li_cycle: %d\n", li_cycle);
        uint64_t cycle = read(this->mmio_addrs->trace_cycle_upper);
        cycle = cycle << 32 | read(this->mmio_addrs->trace_cycle_lower);
        fprintf(this->tracefile, "tracers - cycle: %d\n", cycle);
        fflush(this->tracefile);
//        exit(-1);
//    }

    // this read has the sideeffect of progressing simulation -> execute last
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
