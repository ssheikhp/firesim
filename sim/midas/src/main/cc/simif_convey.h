// See LICENSE for license details.

#ifndef __SIMIF_CONVEY_H
#define __SIMIF_CONVEY_H

#include "simif.h"
#include <stdint.h>
#include <stdio.h>
extern "C"{
    #include <wdm_user.h>
}

class simif_convey_t: public virtual simif_t
{
  public:
    simif_convey_t(int argc, char** argv);
    virtual ~simif_convey_t();
    virtual void write(size_t addr, uint32_t data);
    virtual uint32_t read(size_t addr);
    virtual ssize_t pull(size_t addr, char* data, size_t size){
      fprintf(stderr, "DMA not supported \n");
      exit(-1);
      return 0;
    }
    virtual ssize_t push(size_t addr, char* data, size_t size){
      fprintf(stderr, "DMA not supported \n");
      exit(-1);
      return 0;
    }
//    virtual void write_mem_chunk(size_t addr, mpz_t& value, size_t bytes);

  private:
    wdm_coproc_t m_coproc;
    int fd;
  //allocate memmory
  uint64_t *cp_base;
  
  protected:
    void writeCSR(unsigned int regInd, uint64_t regValue);
    uint64_t readCSR(unsigned int regInd);
};

#endif // __SIMIF_CONVEY_H
