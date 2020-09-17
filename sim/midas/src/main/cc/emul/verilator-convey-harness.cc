#include <memory>
#include <cassert>
#include <cmath>
#include <verilated.h>
#if VM_TRACE
#include <verilated_vcd_c.h>
#endif // VM_TRACE
#include "wolverine_interface.h"

extern uint64_t main_time;

extern VWolverineShim* top;
#if VM_TRACE
extern VerilatedVcdC* tfp;
#endif // VM_TRACE


dispatch_req d_req;
dispatch_res d_res;
csr_req c_req;
csr_res c_res;

void tick(wolverine_interface* wi) {
  // ASSUMPTION: All models have *no* combinational paths through I/O
  // Step 1: Clock lo -> propagate signals between DUT and software models

  top -> io_dispInstValid= d_req.disp_inst_valid; //disp_inst_valid
  top -> io_dispInstData= d_req.disp_inst_data; //disp_inst_data
  top -> io_dispRegID= d_req.disp_reg_id; //disp_reg_id
  top -> io_dispRegRead= d_req.disp_reg_read; //disp_reg_read
  top -> io_dispRegWrite= d_req.disp_reg_write; //disp_reg_write
  top -> io_dispRegWrData= d_req.disp_reg_wr_data; //disp_reg_wr_data

  top -> io_csrWrValid= c_req.csr_wr_valid; //csr_wr_valid
  top -> io_csrRdValid= c_req.csr_rd_valid; //csr_rd_valid
  top -> io_csrAddr= c_req.csr_addr; //csr_addr
  top -> io_csrWrData= c_req.csr_wr_data; //csr_wr_data

  top -> io_aeid = 1;
  top->eval();
#if VM_TRACE
  if (tfp) tfp->dump((double) main_time);
#endif // VM_TRACE
  main_time++;

  top->clock = 0;
  top->eval(); // This shouldn't do much
#if VM_TRACE
  if (tfp) tfp->dump((double) main_time);
#endif // VM_TRACE
  main_time++;

  // Step 2: Clock high, tick all software models and evaluate DUT with posedge
  d_res.disp_aeg_cnt = top->io_dispAegCnt;//disp_aeg_cnt
  d_res.disp_exception = top->io_dispException;//disp_exception
  d_res.disp_idle = top->io_dispIdle;//disp_idle
  d_res.disp_rtn_valid = top->io_dispRtnValid;//disp_rtn_valid
  d_res.disp_rtn_data = top->io_dispRtnData;//disp_rtn_data
  d_res.disp_stall = top->io_dispStall;//disp_stall
  c_res.csr_rd_ack = top->io_csrReadAck;//csr_rd_ack
  c_res.csr_rd_data = top->io_csrReadData;//csr_rd_data
  wi->targetStep(&d_req, d_res, &c_req, c_res);


  top->clock = 1;
  top->eval();
}
