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

#include "vcd_writer.h"
using namespace vcd;

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
  fflush(stdout);
  // allocate 32GB alligned to 64bytes
  int aloc_resp = wdm_posix_memalign(m_coproc, (void**)&cp_base, 64, 0x800000000UL);
  printf("response: %d\n", aloc_resp);
  printf("cp_base: %p\n", (void *)cp_base);

  wdm_dispatch_t ds;
  memset((void *)&ds, 0, sizeof(ds));
  uint64_t args[1];
  uint64_t resp[1];
  // write base of allocated region
  args[0] = (uint64_t) cp_base;
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
  printf("badr_csr_value: %p cp_base: %p\n", (void *)badr_csr_value, (void *)cp_base);
}

simif_convey_t::~simif_convey_t() {
    // make dispatch finish
//    writeCSR(0, 0);
//    assert(readCSR(0)==0);
//        uint32_t * dst;
//      dst = (uint32_t*)cp_base;
//      for (size_t i = 0 ; i < 0x1000 ; i++) {
//        printf("%p - %x\n", &dst[i], dst[i]);
//      }
    uint32_t mem_error = readCSR(13);
    //if(mem_error){
        printf("mem_axi_error_reg: %u\n", mem_error);
    //}

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

std::string toBinary(int n) {
    if (n==0) return "0";
    else if (n==1) return "1";
    else if (n%2 == 0) return toBinary(n/2) + "0";
    else if (n%2 != 0) return toBinary(n/2) + "1";
}

int simif_convey_t::finish() {
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

	writer.change(clock,0,"x");
	writer.change(r_ready,0,"x");
    writer.change(r_resp,0,"x");
    writer.change(r_last,0,"x");
    writer.change(r_data,0,"x");
    writer.change(r_valid,0,"x");
    writer.change(r_id,0,"x");
    writer.change(ar_addr,0,"x");
    writer.change(ar_qos,0,"x");
    writer.change(ar_len,0,"x");
    writer.change(ar_prot,0,"x");
    writer.change(ar_burst,0,"x");
    writer.change(ar_ready,0,"x");
    writer.change(ar_id,0,"x");
    writer.change(ar_size,0,"x");
    writer.change(ar_lock,0,"x");
    writer.change(ar_valid,0,"x");
    writer.change(ar_cache,0,"x");
    writer.change(b_ready,0,"x");
    writer.change(b_id,0,"x");
    writer.change(b_resp,0,"x");
    writer.change(b_valid,0,"x");
    writer.change(w_last,0,"x");
    writer.change(w_data,0,"x");
    writer.change(w_valid,0,"x");
    writer.change(w_strb,0,"x");
    writer.change(w_ready,0,"x");
    writer.change(aw_id,0,"x");
    writer.change(aw_addr,0,"x");
    writer.change(aw_valid,0,"x");
    writer.change(aw_cache,0,"x");
    writer.change(aw_size,0,"x");
    writer.change(aw_qos,0,"x");
    writer.change(aw_len,0,"x");
    writer.change(aw_lock,0,"x");
    writer.change(aw_burst,0,"x");
    writer.change(aw_ready,0,"x");
    writer.change(aw_prot,0,"x");

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
//  ::finish();
  return exitcode;
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
    uint64_t tmp = (((uint64_t)(0x00000000UL | off*4)) << 32);
    writeCSR(CSR_ADAPT_AXI, tmp);
    uint64_t result = readCSR(CSR_ADAPT_AXI);
    //printf("R %d - %x\n", off, (uint32_t)result);
    return result;
}

void simif_convey_t::write(size_t off, uint32_t word)
{
    //printf("W %d - %x\n", off, word);
    uint64_t tmp = (((uint64_t)(0x80000000UL | off*4)) << 32) | word;
    writeCSR(CSR_ADAPT_AXI, tmp);
}


//void simif_convey_t::write_mem_chunk(size_t addr, mpz_t& value, size_t bytes) {
//  uint32_t * dst;
//  dst = (uint32_t*)(((void *)cp_base)+addr);
//  printf("write_mem_chunk to %x - %p - %d\n", addr, dst, bytes);
//  size_t size;
//  data_t* data = (data_t*)mpz_export(NULL, &size, -1, sizeof(data_t), 0, 0, value);
////  if(wdm_memcpy(m_coproc, dst, data, bytes)){
////    fprintf(stderr, "wdm_memcpy failed\n");
////    exit(-1);
////  }
//  for (size_t i = 0 ; i < bytes ; i++) {
//    dst[i] = i < size ? data[i] : 0;
//  }
//}
