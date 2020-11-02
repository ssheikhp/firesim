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
#include "vcd_writer.h"
using namespace vcd;

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

std::string toBinary(int n) {
    if (n==0) return "0";
    else if (n==1) return "1";
    else if (n%2 == 0) return toBinary(n/2) + "0";
    else if (n%2 != 0) return toBinary(n/2) + "1";
}

int simif_convey_emul_t::finish() {
    HeadPtr head = makeVCDHeader(TimeScale::ONE, TimeScaleUnit::ns, utils::now());
    	VCDWriter writer{"axi.vcd", std::move(head)};

    	VarPtr clock =  writer.register_var("mem",  "clock",  VariableType::wire, 1);

    	VarPtr r_ready =  writer.register_var("mem.r",  "r_ready",  VariableType::wire, CSR_r_ready_width);
    	VarPtr r_resp =   writer.register_var("mem.r",  "r_resp",   VariableType::wire, CSR_r_resp_width);
    	VarPtr r_last =   writer.register_var("mem.r",  "r_last",   VariableType::wire, CSR_r_last_width);
    	VarPtr r_data =   writer.register_var("mem.r",  "r_data",   VariableType::wire, CSR_r_data_width);
    	VarPtr r_valid =  writer.register_var("mem.r",  "r_valid",  VariableType::wire, CSR_r_valid_width);
    	VarPtr r_id =     writer.register_var("mem.r",  "r_id",     VariableType::wire, CSR_r_id_width);

    	VarPtr ar_addr =  writer.register_var("mem.ar", "ar_addr",  VariableType::wire, CSR_ar_addr_width);
    	VarPtr ar_qos =   writer.register_var("mem.ar", "ar_qos",   VariableType::wire, CSR_ar_qos_width);
    	VarPtr ar_len =   writer.register_var("mem.ar", "ar_len",   VariableType::wire, CSR_ar_len_width);
    	VarPtr ar_prot =  writer.register_var("mem.ar", "ar_prot",  VariableType::wire, CSR_ar_prot_width);
    	VarPtr ar_burst = writer.register_var("mem.ar", "ar_burst", VariableType::wire, CSR_ar_burst_width);
    	VarPtr ar_ready = writer.register_var("mem.ar", "ar_ready", VariableType::wire, CSR_ar_ready_width);
    	VarPtr ar_id =    writer.register_var("mem.ar", "ar_id",    VariableType::wire, CSR_ar_id_width);
    	VarPtr ar_size =  writer.register_var("mem.ar", "ar_size",  VariableType::wire, CSR_ar_size_width);
    	VarPtr ar_lock =  writer.register_var("mem.ar", "ar_lock",  VariableType::wire, CSR_ar_lock_width);
    	VarPtr ar_valid = writer.register_var("mem.ar", "ar_valid", VariableType::wire, CSR_ar_valid_width);
    	VarPtr ar_cache = writer.register_var("mem.ar", "ar_cache", VariableType::wire, CSR_ar_cache_width);
    	VarPtr b_ready =  writer.register_var("mem.b",  "b_ready",  VariableType::wire, CSR_b_ready_width);
    	VarPtr b_id =     writer.register_var("mem.b",  "b_id",     VariableType::wire, CSR_b_id_width);
    	VarPtr b_resp =   writer.register_var("mem.b",  "b_resp",   VariableType::wire, CSR_b_resp_width);
    	VarPtr b_valid =  writer.register_var("mem.b",  "b_valid",  VariableType::wire, CSR_b_valid_width);
    	VarPtr w_last =   writer.register_var("mem.w",  "w_last",   VariableType::wire, CSR_w_last_width);
    	VarPtr w_data =   writer.register_var("mem.w",  "w_data",   VariableType::wire, CSR_w_data_width);
    	VarPtr w_valid =  writer.register_var("mem.w",  "w_valid",  VariableType::wire, CSR_w_valid_width);
    	VarPtr w_strb =   writer.register_var("mem.w",  "w_strb",   VariableType::wire, CSR_w_strb_width);
    	VarPtr w_ready =  writer.register_var("mem.w",  "w_ready",  VariableType::wire, CSR_w_ready_width);
    	VarPtr aw_id =    writer.register_var("mem.aw", "aw_id",    VariableType::wire, CSR_aw_id_width);
    	VarPtr aw_addr =  writer.register_var("mem.aw", "aw_addr",  VariableType::wire, CSR_aw_addr_width);
    	VarPtr aw_valid = writer.register_var("mem.aw", "aw_valid", VariableType::wire, CSR_aw_valid_width);
    	VarPtr aw_cache = writer.register_var("mem.aw", "aw_cache", VariableType::wire, CSR_aw_cache_width);
    	VarPtr aw_size =  writer.register_var("mem.aw", "aw_size",  VariableType::wire, CSR_aw_size_width);
    	VarPtr aw_qos =   writer.register_var("mem.aw", "aw_qos",   VariableType::wire, CSR_aw_qos_width);
    	VarPtr aw_len =   writer.register_var("mem.aw", "aw_len",   VariableType::wire, CSR_aw_len_width);
    	VarPtr aw_lock =  writer.register_var("mem.aw", "aw_lock",  VariableType::wire, CSR_aw_lock_width);
    	VarPtr aw_burst = writer.register_var("mem.aw", "aw_burst", VariableType::wire, CSR_aw_burst_width);
    	VarPtr aw_ready = writer.register_var("mem.aw", "aw_ready", VariableType::wire, CSR_aw_ready_width);
    	VarPtr aw_prot =  writer.register_var("mem.aw", "aw_prot",  VariableType::wire, CSR_aw_prot_width);

    	writer.change(clock,1,"x");
    	writer.change(r_ready,1,"x");
        writer.change(r_resp,1,"x");
        writer.change(r_last,1,"x");
        writer.change(r_data,1,"x");
        writer.change(r_valid,1,"x");
        writer.change(r_id,1,"x");
        writer.change(ar_addr,1,"x");
        writer.change(ar_qos,1,"x");
        writer.change(ar_len,1,"x");
        writer.change(ar_prot,1,"x");
        writer.change(ar_burst,1,"x");
        writer.change(ar_ready,1,"x");
        writer.change(ar_id,1,"x");
        writer.change(ar_size,1,"x");
        writer.change(ar_lock,1,"x");
        writer.change(ar_valid,1,"x");
        writer.change(ar_cache,1,"x");
        writer.change(b_ready,1,"x");
        writer.change(b_id,1,"x");
        writer.change(b_resp,1,"x");
        writer.change(b_valid,1,"x");
        writer.change(w_last,1,"x");
        writer.change(w_data,1,"x");
        writer.change(w_valid,1,"x");
        writer.change(w_strb,1,"x");
        writer.change(w_ready,1,"x");
        writer.change(aw_id,1,"x");
        writer.change(aw_addr,1,"x");
        writer.change(aw_valid,1,"x");
        writer.change(aw_cache,1,"x");
        writer.change(aw_size,1,"x");
        writer.change(aw_qos,1,"x");
        writer.change(aw_len,1,"x");
        writer.change(aw_lock,1,"x");
        writer.change(aw_burst,1,"x");
        writer.change(aw_ready,1,"x");
        writer.change(aw_prot,1,"x");

        uint64_t current_time_step = readCSR(CSR_time_step);
        std::map<uint64_t, std::map<VarPtr, uint64_t>> data_map;
        for(int idx = 0;idx<16;idx++){
            writeCSR(CSR_read_index_reg, idx);
            uint64_t r_time_step = readCSR(CSR_r_time_step);
            uint64_t ar_time_step = readCSR(CSR_ar_time_step);
            uint64_t b_time_step = readCSR(CSR_b_time_step);
            uint64_t w_time_step = readCSR(CSR_w_time_step);
            uint64_t aw_time_step = readCSR(CSR_aw_time_step);

            if(r_time_step){
                data_map[r_time_step][r_ready] =  readCSR(CSR_r_ready);
                data_map[r_time_step][r_resp] =   readCSR(CSR_r_resp);
                data_map[r_time_step][r_last] =   readCSR(CSR_r_last);
                data_map[r_time_step][r_data] =   readCSR(CSR_r_data);
                data_map[r_time_step][r_valid] =  readCSR(CSR_r_valid);
                data_map[r_time_step][r_id] =     readCSR(CSR_r_id);
                data_map[r_time_step+1].emplace(r_valid,0);
            }

            if(ar_time_step){
                data_map[ar_time_step][ar_addr] =  readCSR(CSR_ar_addr);
                data_map[ar_time_step][ar_qos] =   readCSR(CSR_ar_qos);
                data_map[ar_time_step][ar_len] =   readCSR(CSR_ar_len);
                data_map[ar_time_step][ar_prot] =  readCSR(CSR_ar_prot);
                data_map[ar_time_step][ar_burst] = readCSR(CSR_ar_burst);
                data_map[ar_time_step][ar_ready] = readCSR(CSR_ar_ready);
                data_map[ar_time_step][ar_id] =    readCSR(CSR_ar_id);
                data_map[ar_time_step][ar_size] =  readCSR(CSR_ar_size);
                data_map[ar_time_step][ar_lock] =  readCSR(CSR_ar_lock);
                data_map[ar_time_step][ar_valid] = readCSR(CSR_ar_valid);
                data_map[ar_time_step][ar_cache] = readCSR(CSR_ar_cache);
                data_map[ar_time_step+1].emplace(ar_valid,0);
            }

            if(b_time_step){
                data_map[b_time_step][b_ready] =  readCSR(CSR_b_ready);
                data_map[b_time_step][b_id] =     readCSR(CSR_b_id);
                data_map[b_time_step][b_resp] =   readCSR(CSR_b_resp);
                data_map[b_time_step][b_valid] =  readCSR(CSR_b_valid);
                data_map[b_time_step+1].emplace(b_valid,0);
            }

            if(w_time_step){
                data_map[w_time_step][w_last] =   readCSR(CSR_w_last);
                data_map[w_time_step][w_data] =   readCSR(CSR_w_data);
                data_map[w_time_step][w_valid] =  readCSR(CSR_w_valid);
                data_map[w_time_step][w_strb] =   readCSR(CSR_w_strb);
                data_map[w_time_step][w_ready] =  readCSR(CSR_w_ready);
                data_map[w_time_step+1].emplace(w_valid,0);
            }

            if(aw_time_step){
                data_map[aw_time_step][aw_id] =    readCSR(CSR_aw_id);
                data_map[aw_time_step][aw_addr] =  readCSR(CSR_aw_addr);
                data_map[aw_time_step][aw_valid] = readCSR(CSR_aw_valid);
                data_map[aw_time_step][aw_cache] = readCSR(CSR_aw_cache);
                data_map[aw_time_step][aw_size] =  readCSR(CSR_aw_size);
                data_map[aw_time_step][aw_qos] =   readCSR(CSR_aw_qos);
                data_map[aw_time_step][aw_len] =   readCSR(CSR_aw_len);
                data_map[aw_time_step][aw_lock] =  readCSR(CSR_aw_lock);
                data_map[aw_time_step][aw_burst] = readCSR(CSR_aw_burst);
                data_map[aw_time_step][aw_ready] = readCSR(CSR_aw_ready);
                data_map[aw_time_step][aw_prot] =  readCSR(CSR_aw_prot);
                data_map[aw_time_step+1].emplace(aw_valid,0);
            }
        }

        data_map[current_time_step][r_ready] =  readCSR(CSR_r_current_ready);
        data_map[current_time_step][r_resp] =   readCSR(CSR_r_current_resp);
        data_map[current_time_step][r_last] =   readCSR(CSR_r_current_last);
        data_map[current_time_step][r_data] =   readCSR(CSR_r_current_data);
        data_map[current_time_step][r_valid] =  readCSR(CSR_r_current_valid);
        data_map[current_time_step][r_id] =     readCSR(CSR_r_current_id);
        data_map[current_time_step+1][r_valid] = 0;

        data_map[current_time_step][ar_addr] =  readCSR(CSR_ar_current_addr);
        data_map[current_time_step][ar_qos] =   readCSR(CSR_ar_current_qos);
        data_map[current_time_step][ar_len] =   readCSR(CSR_ar_current_len);
        data_map[current_time_step][ar_prot] =  readCSR(CSR_ar_current_prot);
        data_map[current_time_step][ar_burst] = readCSR(CSR_ar_current_burst);
        data_map[current_time_step][ar_ready] = readCSR(CSR_ar_current_ready);
        data_map[current_time_step][ar_id] =    readCSR(CSR_ar_current_id);
        data_map[current_time_step][ar_size] =  readCSR(CSR_ar_current_size);
        data_map[current_time_step][ar_lock] =  readCSR(CSR_ar_current_lock);
        data_map[current_time_step][ar_valid] = readCSR(CSR_ar_current_valid);
        data_map[current_time_step][ar_cache] = readCSR(CSR_ar_current_cache);
        data_map[current_time_step+1][ar_valid] = 0;

        data_map[current_time_step][b_ready] =  readCSR(CSR_b_current_ready);
        data_map[current_time_step][b_id] =     readCSR(CSR_b_current_id);
        data_map[current_time_step][b_resp] =   readCSR(CSR_b_current_resp);
        data_map[current_time_step][b_valid] =  readCSR(CSR_b_current_valid);
        data_map[current_time_step+1][b_valid] = 0;

        data_map[current_time_step][w_last] =   readCSR(CSR_w_current_last);
        data_map[current_time_step][w_data] =   readCSR(CSR_w_current_data);
        data_map[current_time_step][w_valid] =  readCSR(CSR_w_current_valid);
        data_map[current_time_step][w_strb] =   readCSR(CSR_w_current_strb);
        data_map[current_time_step][w_ready] =  readCSR(CSR_w_current_ready);
        data_map[current_time_step+1][w_valid] = 0;

        data_map[current_time_step][aw_id] =    readCSR(CSR_aw_current_id);
        data_map[current_time_step][aw_addr] =  readCSR(CSR_aw_current_addr);
        data_map[current_time_step][aw_valid] = readCSR(CSR_aw_current_valid);
        data_map[current_time_step][aw_cache] = readCSR(CSR_aw_current_cache);
        data_map[current_time_step][aw_size] =  readCSR(CSR_aw_current_size);
        data_map[current_time_step][aw_qos] =   readCSR(CSR_aw_current_qos);
        data_map[current_time_step][aw_len] =   readCSR(CSR_aw_current_len);
        data_map[current_time_step][aw_lock] =  readCSR(CSR_aw_current_lock);
        data_map[current_time_step][aw_burst] = readCSR(CSR_aw_current_burst);
        data_map[current_time_step][aw_ready] = readCSR(CSR_aw_current_ready);
        data_map[current_time_step][aw_prot] =  readCSR(CSR_aw_current_prot);
        data_map[current_time_step+1][aw_valid] = 0;
        uint64_t ts_prev = data_map.begin()->first;
        for(auto ts : data_map){
            for(;ts_prev<ts.first; ts_prev++){
                writer.change(clock,  ts_prev*2, "1");
                writer.change(clock,  ts_prev*2+1, "0");
            }
            for(auto v: ts.second){
                writer.change(v.first,  ts.first*2, toBinary(v.second));
            }
        }
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
//    printf("R %d - %x\n", off, (uint32_t)result);
    return result;
}

void simif_convey_emul_t::write(size_t off, uint32_t word)
{
//    printf("W %d - %x\n", off, word);
    uint64_t tmp = (((uint64_t)(0x80000000UL | off*4)) << 32) | word;
    writeCSR(CSR_ADAPT_AXI, tmp);
}
