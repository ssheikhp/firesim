#ifndef WOLVERINE_INTERFACE_H
#define WOLVERINE_INTERFACE_H

typedef struct {
    unsigned char disp_inst_valid;
    int           disp_inst_data;
    int           disp_reg_id;
    unsigned char disp_reg_read;
    unsigned char disp_reg_write;
    long long     disp_reg_wr_data;
} dispatch_req;

typedef struct {
    int            disp_aeg_cnt;
    int            disp_exception;
    unsigned char  disp_idle;
    unsigned char  disp_rtn_valid;
    long long      disp_rtn_data;
    unsigned char  disp_stall;
} dispatch_res;

typedef struct {
    unsigned char csr_wr_valid;
    unsigned char csr_rd_valid;
    int           csr_addr;
    long long     csr_wr_data;
} csr_req;

typedef struct {
    unsigned char  csr_rd_ack;
    long long      csr_rd_data;
} csr_res;

class wolverine_interface {
  public:
    wolverine_interface();
    ~wolverine_interface();

    void initHost(uint64_t mem_base);
    void writeCSR(unsigned int regInd, uint64_t regValue);
    uint64_t readCSR(unsigned int regInd);
    void targetStep(
        dispatch_req *d_req,
        dispatch_res d_res,
        csr_req *c_req,
        csr_res c_res
    );
    void hostStep();

  private:
    dispatch_req d_req;
    dispatch_res d_res;
    csr_req c_req;
    csr_res c_res;
};

#endif // WOLVERINE_INTERFACE_H