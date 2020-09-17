#include <vector>
#include <string>
#include <assert.h>
#include <stdio.h>
#include "wolverine_interface.h"

extern void tick(wolverine_interface* wi);

wolverine_interface::wolverine_interface(){
    //set default values
    d_req.disp_inst_valid = 0;
    d_req.disp_inst_data = 0;
    d_req.disp_reg_id = 0;
    d_req.disp_reg_read = 0;
    d_req.disp_reg_write = 0;
    d_req.disp_reg_wr_data = 0;

    c_req.csr_wr_valid = 0;
    c_req.csr_rd_valid = 0;
    c_req.csr_addr = 0;
    c_req.csr_wr_data = 0;

}

wolverine_interface::~wolverine_interface(){
}

void wolverine_interface::initHost(uint64_t mem_base){
    for(int i=0; i<20; i++){
        hostStep();
    }
    printf("initHost 0\n");
    hostStep();
    printf("initHost 1\n");
    writeCSR(1, mem_base);
    hostStep();
    // pseudo-dispatch
    writeCSR(0, 1);
}

void wolverine_interface::targetStep(
    dispatch_req *d_req,
    dispatch_res d_res,
    csr_req *c_req,
    csr_res c_res
){
//    // switch over to host
//    host.switch_to();
    this->d_res = d_res;
    this->c_res = c_res;
    *d_req = this->d_req;
    *c_req = this->c_req;
}

void wolverine_interface::hostStep(){
//    printf("hostStep 0\n");
    tick(this);
    //set default values
    d_req.disp_inst_valid = 0;
    d_req.disp_inst_data = 0;
    d_req.disp_reg_id = 0;
    d_req.disp_reg_read = 0;
    d_req.disp_reg_write = 0;
    d_req.disp_reg_wr_data = 0;

    c_req.csr_wr_valid = 0;
    c_req.csr_rd_valid = 0;
    c_req.csr_addr = 0;
    c_req.csr_wr_data = 0;
//    printf("hostStep 1\n");
}

void wolverine_interface::writeCSR(unsigned int regInd, uint64_t regValue){
    c_req.csr_addr = regInd;
    c_req.csr_wr_valid = 1;
    c_req.csr_wr_data = regValue;
    for(int i=0; i<20; i++){
        hostStep();
    }
}

uint64_t wolverine_interface::readCSR(unsigned int regInd){
    c_req.csr_addr = regInd;
    c_req.csr_rd_valid = 1;
    hostStep();
    while(!c_res.csr_rd_ack){
        hostStep();
    }
    uint64_t res = c_res.csr_rd_data;
    for(int i=0; i<20; i++){
        hostStep();
    }
    return res;
}