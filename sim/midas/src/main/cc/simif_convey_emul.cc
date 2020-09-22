// See LICENSE for license details.
#include "simif_convey_emul.h"
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <verilated.h>
#if VM_TRACE
#include <verilated_vcd_c.h>
#endif
#include <signal.h>

uint64_t main_time = 0;
VWolverineShim* top = NULL;
#if VM_TRACE
VerilatedVcdC* tfp = NULL;
#endif // VM_TRACE
double sc_time_stamp() {
  return (double) main_time;
}

extern void tick(wolverine_interface* wi);

void finish() {
#if VM_TRACE
  if (tfp) tfp->close();
  delete tfp;
#endif // VM_TRACE
}

void handle_sigterm(int sig) {
  finish();
}

void simif_convey_emul_t::init(int argc, char** argv, bool log) {
  // Parse args
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string waveform = "dump.vcd";
  std::string loadmem;
  bool fastloadmem = false;
  bool dramsim = false;
  uint64_t memsize = 1L << MEM_ADDR_BITS;
  for (auto arg: args) {
    if (arg.find("+waveform=") == 0) {
      waveform = arg.c_str() + 10;
    }
    if (arg.find("+memsize=") == 0) {
      memsize = strtoll(arg.c_str() + 9, NULL, 10);
    }
    if (arg.find("+fuzz-host-timing=") == 0) {
      maximum_host_delay = atoi(arg.c_str() + 18);
    }
  }

  signal(SIGTERM, handle_sigterm);

  Verilated::commandArgs(argc, argv); // Remember args

  top = new VWolverineShim;
#if VM_TRACE                         // If emul was invoked with --trace
  tfp = new VerilatedVcdC;
  Verilated::traceEverOn(true);      // Verilator must compute traced signals
  VL_PRINTF("Enabling waves: %s\n", waveform.c_str());
  top->trace(tfp, 99);                // Trace 99 levels of hierarchy
  tfp->open(waveform.c_str());        // Open the dump file
#endif // VM_TRACE

  top->reset = 1;
  for (size_t i = 0 ; i < 10 ; i++) ::tick(wi);
  top->reset = 0;

  wi->initHost(0x20000000U);

  simif_t::init(argc, argv, log);
}

int simif_convey_emul_t::finish() {
  int exitcode = simif_t::finish();
  ::finish();
  return exitcode;
}

#define CSR_ADAPT_AXI 8

simif_convey_emul_t::simif_convey_emul_t() {
    wi = new wolverine_interface();
}

simif_convey_emul_t::~simif_convey_emul_t() {
//    // make dispatch finish
//    writeCSR(0, 0);
//    assert(readCSR(0)==0);
}

void simif_convey_emul_t::writeCSR(unsigned int regInd, uint64_t regValue) {
  wi->writeCSR(regInd, regValue);
}

uint64_t simif_convey_emul_t::readCSR(unsigned int regInd) {
  return wi->readCSR(regInd);
}

uint32_t simif_convey_emul_t::read(size_t off)
{
    uint64_t tmp = (((uint64_t)(0x00000000UL | off*4)) << 32);
    writeCSR(CSR_ADAPT_AXI, tmp);
    uint64_t result = readCSR(CSR_ADAPT_AXI);
    printf("R %d - %x\n", off, (uint32_t)result);
    return result;
}

void simif_convey_emul_t::write(size_t off, uint32_t word)
{
    printf("W %d - %x\n", off, word);
    uint64_t tmp = (((uint64_t)(0x80000000UL | off*4)) << 32) | word;
    writeCSR(CSR_ADAPT_AXI, tmp);
}
