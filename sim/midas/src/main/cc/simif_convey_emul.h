// See LICENSE for license details.

#ifndef __SIMIF_CONVEY_EMUL_H
#define __SIMIF_CONVEY_EMUL_H

#include "simif.h"
#include <memory>
#include "emul/wolverine_interface.h"

// simif_emul_t is a concrete simif_t implementation for Software RTL simulators
// The basis for MIDAS-level simulation
class simif_convey_emul_t : public virtual simif_t
{
  public:
    simif_convey_emul_t();
    virtual ~simif_convey_emul_t();
    virtual void init(int argc, char** argv, bool log = false);
    virtual int finish();
    
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

  private:
    int maximum_host_delay = 1;
  protected:
    wolverine_interface * wi;
    
    void writeCSR(unsigned int regInd, uint64_t regValue);
    uint64_t readCSR(unsigned int regInd);
};

#endif // __SIMIF_CONVEY_EMUL_H
