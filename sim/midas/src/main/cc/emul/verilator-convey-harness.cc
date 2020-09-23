#include <memory>
#include <cassert>
#include <cmath>
#include <verilated.h>
#if VM_TRACE
#include <verilated_vcd_c.h>
#endif // VM_TRACE
#include "wolverine_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <deque>
#include <assert.h>

extern uint64_t main_time;

extern VWolverineShim* top;
#if VM_TRACE
extern VerilatedVcdC* tfp;
#endif // VM_TRACE


dispatch_req d_req;
dispatch_res d_res;
csr_req c_req;
csr_res c_res;

#define MEM_SIZE 0x40000000ULL
#define MEM_BASE 0x20000000ULL
#define MEM_CYCLES 50

typedef struct{
    int cycle;
    int cmd;
    int scmd;
    int size;
    uint64_t addr;
    int rtnctl;
    uint64_t data;
} mem_request;

char * mem = NULL;
int beat = 0;
int cycle = 0;
std::deque<mem_request> * queue = NULL;

void tick(wolverine_interface* wi) {
  // init memory stuff
    if(!mem){
        printf("allocating mem buffer\n");
        mem = (char *)malloc(MEM_SIZE);
    }
    if(!queue){
        queue = new std::deque<mem_request>();
    }

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


    // never stall for now
    top->io_mcReqStall = 0; //mc_req_stall

    top->io_mcResValid = 0; //mc_res_valid
    top->io_mcResCmd = 0; //mc_res_cmd
    top->io_mcResSCmd = 0; //mc_res_scmd
    top->io_mcResData = 0; //mc_res_data
    top->io_mcResRtnCtl = 0; //mc_res_rtnctl

    if((!queue->empty()) && queue->front().cycle <=cycle){
        mem_request mr = queue->front();

//        printf("MEMORY PROCESSING:\n");
//        printf("mr.cycle: %d\n", mr.cycle);
//        printf("mr.cmd: %d\n", mr.cmd);
//        printf("mr.scmd: %d\n", mr.scmd);
//        printf("mr.size: %d\n", mr.size);
//        printf("mr.addr: %lx\n", mr.addr);
//        printf("mr.rtnctl: %d\n", mr.rtnctl);
//        printf("mr.data: %lx\n", mr.data);
        uint64_t addr = mr.addr - MEM_BASE;
        assert(addr<MEM_SIZE);
        if(mr.cmd == 2){ // write
            assert(mr.scmd == 0);
            queue->pop_front();
            if(mr.size == 2){
                for(int i=0; i<4; i++){
                    mem[addr+i] = (mr.data >> (8*i)) & 0xFF;
                }
            } else if(mr.size == 3){
                for(int i=0; i<8; i++){
                    mem[addr+i] = (mr.data >> (8*i)) & 0xFF;
                }
            } else {
                assert(false);
            }
            top->io_mcResValid = 1;
            top->io_mcResCmd = 3;
            top->io_mcResSCmd = 0;
            top->io_mcResData = 0;
            top->io_mcResRtnCtl = mr.rtnctl;
        } else if(mr.cmd == 1){ // read
            assert(mr.scmd == 0);
            queue->pop_front();
            assert(mr.size == 3);
            uint64_t dt = 0;
            for(int i=0; i<8; i++){
                dt |= (mem[addr+i]&0xFFULL) << (i*8);
            }
            top->io_mcResValid = 1;
            top->io_mcResCmd = 2;
            top->io_mcResSCmd = 0;
            top->io_mcResData = dt;
            top->io_mcResRtnCtl = mr.rtnctl;
        } else if(mr.cmd == 6){ // multi write
            queue->pop_front();
            assert(mr.size == 3);
            int beats = (mr.scmd == 0) ? 8 : mr.scmd;
            for(int i=0; i<8; i++){
                mem[addr+i] = (mr.data >> (8*i)) & 0xFF;
            }
            beat++;
            if(beat == beats){
                beat = 0;
                top->io_mcResValid = 1;
                top->io_mcResCmd = 3;
                top->io_mcResSCmd = 0;
                top->io_mcResData = 0;
                top->io_mcResRtnCtl = mr.rtnctl;
            }
        } else if(mr.cmd == 7){ // multi read
            assert(mr.scmd == 0);
            assert(mr.size==3);
            uint64_t dt = 0;
            for(int i=0; i<8; i++){
                dt |= (mem[addr+i+beat*8]&0xFFULL) << (i*8);
            }
            top->io_mcResValid = 1;
            top->io_mcResCmd = 7;
            top->io_mcResSCmd = beat;
            top->io_mcResData = dt;
            top->io_mcResRtnCtl = mr.rtnctl;
            beat++;
            if(beat==8){
                beat = 0;
                queue->pop_front();
            }
        } else {
            assert(false);
        }
    }

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

    if(top->io_mcReqValid){
        mem_request mr;
        mr.cycle = cycle+MEM_CYCLES;
        mr.cmd = top->io_mcReqCmd;//mc_req_cmd
        mr.scmd = top->io_mcReqSCmd;//mc_req_scmd
        mr.size = top->io_mcReqSize;//mc_req_size
        mr.addr = top->io_mcReqAddr;//mc_req_addr
        mr.rtnctl = top->io_mcReqRtnCtl;//mc_req_rtnctl
        mr.data = top->io_mcReqData;//mc_req_data
//        printf("MEMORY REQUEST:\n");
//        printf("mr.cycle: %d\n", mr.cycle);
//        printf("mr.cmd: %d\n", mr.cmd);
//        printf("mr.scmd: %d\n", mr.scmd);
//        printf("mr.size: %d\n", mr.size);
//        printf("mr.addr: %lx\n", mr.addr);
//        printf("mr.rtnctl: %d\n", mr.rtnctl);
//        printf("mr.data: %lx\n", mr.data);
        queue->push_back(mr);
    }
    cycle++;


  top->clock = 1;
  top->eval();
}
