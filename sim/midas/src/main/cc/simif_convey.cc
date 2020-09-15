// See LICENSE for license details.

#include "simif_convey.h"
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>

extern "C"{
    #include "os/fw_info.h"
    #include "os/fw_ioctl.h"
}

#define CSR_ADAPT_AXI 8

simif_convey_t::simif_convey_t(int argc, char** argv) {
  m_coproc = WDM_INVALID;
  m_coproc = wdm_reserve(WDM_CPID_ANY, NULL);
  if (m_coproc == WDM_INVALID) {
        fprintf(stderr, "Unable to reserve coprocessor\n");
        exit(-1);
  }
  wdm_attrs_t attr;
  wdm_attributes (m_coproc, &attr);
  printf("using coprocessor %d\n", attr.attr_cpid);

  char *name = "wolverine_firesim";
  printf("Loading personality %s...", name);
  if (wdm_attach(m_coproc, name)) {
      fprintf(stderr, "Unable to attach signature \"%s\"\n", name);
      fprintf(stderr, " Please verify that the personality is installed in");
      fprintf(stderr, " /opt/convey/personalities or CNY_PERSONALITY_PATH is set.\n");
      exit(-1);
  }
  printf("done\n");

  printf("opening device for csr...");
  //choose right device for processor (see /opt/convey/bin/wxinfo )
  switch(attr.attr_cpid){
      case 0:
          fd = open("/dev/wxpfwc0", O_RDWR);
          break;
      case 1:
          fd = open("/dev/wxpfwa0", O_RDWR);
          break;
      case 2:
          fd = open("/dev/wxpfwb0", O_RDWR);
          break;
      case 3:
          fd = open("/dev/wxpfwd0", O_RDWR);
          break;
      default:
          fd = -1;
  }
  if(fd == -1){
    int errsv = errno;
    fprintf(stderr, "open failed failed err: %d\n", errsv);
    exit(-1);
  }
  printf("done\n");

  printf("setting to not stalled...");
  writeCSR(0,0);
  // not stalled
  assert(readCSR(0)==0);
  printf("done\n");

  printf("allocate memmory...");
  //allocate memmory
  uint64_t *cp_a1;
  // allocate 1GB alligned to 64bytes
  int aloc_resp = wdm_posix_memalign(m_coproc, (void**)&cp_a1, 64, 0x40000000UL);
  printf("response: %d\n", aloc_resp);
  printf("cp_a1: %p\n", (void *)cp_a1);

  wdm_dispatch_t ds;
  memset((void *)&ds, 0, sizeof(ds));
  uint64_t args[1];
  uint64_t resp[1];
  // write base of allocated region
  args[0] = (uint64_t) cp_a1;
  ds.ae[0].aeg_ptr_s = args;
  ds.ae[0].aeg_cnt_s = 1;
  ds.ae[0].aeg_base_s = 1;
  ds.ae[0].aeg_ptr_r = resp;
  ds.ae[0].aeg_cnt_r = 0;
  ds.ae[0].aeg_base_r = 0;

  printf("Dispatching...");
  if (wdm_dispatch(m_coproc, &ds)) {
    perror("dispatch error");
    exit(-1);
  }
  assert(wdm_dispatch_status(m_coproc)==0);
  printf("...done\n");

  printf("Waiting for marked as stalled...");
  while(readCSR(0)!=1);
  printf("...done\n");
  // address was written
  uint64_t badr_csr_value = readCSR(1);
  printf("badr_csr_value: %p cp_a1: %p\n", (void *)badr_csr_value, (void *)cp_a1);
}

simif_convey_t::~simif_convey_t() {
    // make dispatch finish
    writeCSR(0, 0);
    assert(readCSR(0)==0);

    if(close(fd)!=0){
      fprintf(stderr, "fd close Failed\n");
      exit(-1);
    }

    if(wdm_detach(m_coproc)!=0){
      fprintf(stderr, "Detach Failed\n");
      exit(-1);
    }

    if(wdm_release(m_coproc)!=0){
      fprintf(stderr, "Release Failed\n");
      exit(-1);
    }
}

void simif_convey_t::writeCSR(unsigned int regInd, uint64_t regValue) {
  uint64_t offset = 0x30000 + 0x8*regInd;

  wx_fw_dev_access_ctl_t s;
  s.ctl_cmd = WX_CSR_WRITE;
  s.mask = 0;
  s.fpga_type = AEMC0;
  s.read_data = 0;
  s.write_data = regValue;
  s.copy_to_user_flag = 0;
  s.csr_offset = offset;
  s.field_mask = 0;
  s.set_clr_op = 0;

  if(ioctl(fd, WX_IOC_FW_DEV_ACCESS_RW, &s)){
    fprintf(stderr, "ioctl failed\n");
    exit(-1);
  }
}

uint64_t simif_convey_t::readCSR(unsigned int regInd) {
  uint64_t offset = 0x30000 + 0x8*regInd;

  wx_fw_dev_access_ctl_t s;
  s.ctl_cmd = WX_CSR_READ;
  s.mask = 0;
  s.fpga_type = AEMC0;
  s.read_data = 0;
  s.write_data = 0xDEADBEEF;
  s.copy_to_user_flag = 0;
  s.csr_offset = offset;
  s.field_mask = 0;
  s.set_clr_op = 0;

  if(ioctl(fd, WX_IOC_FW_DEV_ACCESS_RW, &s)){
    fprintf(stderr, "ioctl failed\n");
    exit(-1);
  }
  return s.read_data;
}

uint32_t simif_convey_t::read(size_t off)
{
    uint64_t tmp = (((uint64_t)(0x00000000UL | off)) << 32);
    writeCSR(CSR_ADAPT_AXI, tmp);
    uint64_t result = readCSR(CSR_ADAPT_AXI);
//    printf("R %x - %x\n", off, (uint32_t)result);
    return result;
}

void simif_convey_t::write(size_t off, uint32_t word)
{
//    printf("W %x - %x\n", off, word);
    uint64_t tmp = (((uint64_t)(0x80000000UL | off)) << 32) | word;
    writeCSR(CSR_ADAPT_AXI, tmp);
}
