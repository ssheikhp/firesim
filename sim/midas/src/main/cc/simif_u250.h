#ifndef __SIMIF_F1_H
#define __SIMIF_F1_H

#include "simif.h"    // from midas

class simif_u250_t: public virtual simif_t
{
  public:
    simif_u250_t(int argc, char** argv);
    virtual ~simif_u250_t();
    virtual void write(size_t addr, uint32_t data);
    virtual uint32_t read(size_t addr);
    virtual ssize_t pull(size_t addr, char* data, size_t size);
    virtual ssize_t push(size_t addr, char* data, size_t size);
    uint32_t is_write_ready();
    void check_rc(int rc, char * infostr);
    void fpga_shutdown();
    void fpga_setup(int slot_id);
  private:
    void * fpga_pci_bar_get_mem_at_offset(uint64_t offset);
    int fpga_pci_poke(uint64_t offset, uint32_t value);
    int fpga_pci_peek(uint64_t offset, uint32_t *value);
    char in_buf[CTRL_BEAT_BYTES];
    char out_buf[CTRL_BEAT_BYTES];
#ifdef SIMULATION_XSIM
    char * driver_to_xsim = "/tmp/driver_to_xsim";
    char * xsim_to_driver = "/tmp/xsim_to_driver";
    int driver_to_xsim_fd;
    int xsim_to_driver_fd;
#else
//    int rc;
    int slot_id;
    int edma_write_fd;
    int edma_read_fd;
    void* bar0_base;
#endif
};

#endif // __SIMIF_F1_H
