// See LICENSE for license details.

#ifndef __SIMIF_CONVEY_H
#define __SIMIF_CONVEY_H

#include "simif.h"
#include <stdint.h>
extern "C"{
    #include <wdm_user.h>
}

class simif_convey_t: public virtual simif_t
{
  public:
    simif_convey_t();
    virtual ~simif_convey_t() { }

  private:
    wdm_coproc_t m_coproc;
    int fd;
  
  protected:
    virtual void write(size_t addr, uint32_t data);
    virtual uint32_t read(size_t addr);
    virtual size_t pread(size_t addr, char* data, size_t size) {
      // Not supported
      return 0;
    }
    void writeCSR(unsigned int regInd, uint64_t regValue);
    uint64_t readCSR(unsigned int regInd);
};

#endif // __SIMIF_CONVEY_H
