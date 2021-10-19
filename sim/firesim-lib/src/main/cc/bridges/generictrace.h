//See LICENSE for license details
#ifndef __GENERICTRACE_H_
#define __GENERICTRACE_H_

#include "bridges/bridge_driver.h"
#include "bridges/clock_info.h"


#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <queue>
#include <atomic>
#include <chrono>
#include <functional>
// Add some more workers
// If you add/remove workers, they must be registered/deregistered in the
// workerRegister map here in the header file in the generictrace_t class
#include "generictrace_worker.h"
#include "hardwaresamplers.h"

#ifdef GENERICTRACEBRIDGEMODULE_struct_guard

// Bridge Driver Instantiation Template
#define INSTANTIATE_GENERICTRACE(FUNC,IDX) \
     GENERICTRACEBRIDGEMODULE_ ## IDX ## _substruct_create; \
     FUNC(new generictrace_t( \
        this, \
        args, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _substruct, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _DMA_ADDR, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _queue_depth, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _token_width, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _trace_width, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _clock_domain_name, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _clock_multiplier, \
        GENERICTRACEBRIDGEMODULE_ ## IDX ## _clock_divisor, \
        IDX)); \



// https://stackoverflow.com/questions/7045576/using-more-than-one-mutex-with-a-conditional-variable
template<class T1, class T2> class Lock2 {
    bool own1_;
    bool own2_;
    T1 &m1_;
    T2 &m2_;
public:
    Lock2(T1 &m1, T2 &m2)
        : own1_(false), own2_(false), m1_(m1), m2_(m2)
    {
        lock();
    }

    ~Lock2() {
        unlock();
    }

    Lock2(const Lock2&) = delete;
    Lock2& operator=(const Lock2&) = delete;

    void lock() {
        if (!own1_ && !own2_) {
            own1_=true; own2_=true;
            std::lock(m1_, m2_);
        } else if (!own1_) {
            own1_=true;
            m1_.lock();
        } else if (!own2_) {
            own2_=true;
            m2_.lock();
        }
    }

    void unlock() {
        unlock_1();
        unlock_2();
    }

    void unlock_1() {
        if (own1_) {
            own1_=false;
            m1_.unlock();
        }
    }

    void unlock_2() {
        if (own2_) {
            own2_=false;
            m2_.unlock();
        }
    }
};

class spinlock {
        std::atomic<bool> lock_ = {false};
public:
    void lock() {
        for (;;) {
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                break;
            }
            while (lock_.load(std::memory_order_relaxed));
        }
    }

    void unlock() {
        lock_.store(false, std::memory_order_release);
    }

    bool try_lock() {
        return !lock_.exchange(true, std::memory_order_acquire);
    }
};

#define locktype_t spinlock
//#define locktype_t std::mutex


struct protectedWorker {
    locktype_t lock;
    std::shared_ptr<generictrace_worker> worker;
};

struct referencedBuffer {
    char *data;
    unsigned int tokens;
    std::atomic<unsigned int> refs;
};


class generictrace_t: public bridge_driver_t
{
public:
    generictrace_t(simif_t *sim,
                   std::vector<std::string> &args,
                   GENERICTRACEBRIDGEMODULE_struct * mmio_addrs,
                   long dma_addr,
                   const unsigned int queue_depth,
                   const unsigned int tokenWidth,
                   const unsigned int traceWidth,
                   const char* const  clock_domain_name,
                   const unsigned int clock_multiplier,
                   const unsigned int clock_divisor,
                   int tracerId);
    ~generictrace_t();

    virtual void init();
    virtual void tick();
    virtual bool terminate() { return false; }
    virtual int exit_code() { return 0; }
    virtual void finish() { flush(); };
    virtual void work(unsigned int threadIndex);

private:
    // Add you workers here:
    std::map<std::string,
             std::function<std::shared_ptr<generictrace_worker>(std::vector<std::string> &, struct traceInfo &)>> workerRegister = {
        {"dummy",    [](std::vector<std::string> &args, struct traceInfo &info){
                      (void) args; return std::make_shared<generictrace_worker>(info);
                  }},
        {"file",     [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<generictrace_filedumper>(args, info);
                  }},
        {"oracle",   [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<oracle_profiler>(args, info);
                  }},
        {"intervalplus", [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<interval_plus_profiler>(args, info);
                  }},
        {"interval", [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<interval_profiler>(args, info);
                  }},
        {"lynsyn",   [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<lynsyn_profiler>(args, info);
                  }},
        {"pebsplus",     [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<pebs_plus_profiler>(args, info);
                  }},
        {"pebs",     [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<pebs_profiler>(args, info);
                  }},
        {"ibs",      [](std::vector<std::string> &args, struct traceInfo &info){
                      return std::make_shared<ibs_profiler>(args, info);
                  }}
    };

    GENERICTRACEBRIDGEMODULE_struct * mmioAddrs;

    std::vector<std::thread> workerThreads;

    std::vector<std::shared_ptr<protectedWorker>> workers;
    std::vector<std::shared_ptr<referencedBuffer>> buffers;

    locktype_t workerQueueLock;
    locktype_t workerSyncLock;
    std::condition_variable_any workerQueueCond;
    std::queue<std::pair<std::shared_ptr<protectedWorker>, std::shared_ptr<referencedBuffer>>> workerQueue;

    unsigned int bufferIndex = 0;
    unsigned int bufferGrouping = 1;
    unsigned int bufferDepth = 1;
    unsigned int bufferTokenCapacity;
    unsigned int bufferTokenThreshold;
    unsigned int dmaTokenThreshold;
    double dmaThreshold = 0.5;
    unsigned long int totalTokens = 0;

    std::chrono::duration<double> tickTime = std::chrono::seconds(0);
    std::chrono::duration<double> dmaTime = std::chrono::seconds(0);


    ClockInfo clock_info;
    long dmaAddr;
    unsigned int dmaQueueDepth;
    unsigned int tokenWidth;
    unsigned int traceWidth;

    struct traceInfo info = {};

    bool traceEnabled = false;
    unsigned int traceTrigger = 0;
    unsigned int traceThreads = 0;
    bool exit = false;

    std::shared_ptr<protectedWorker> getWorker(std::string workername, std::vector<std::string> &args, struct traceInfo &info);
    void process_tokens(unsigned int const tokens, bool flush = false);
    void flush();
};
#endif // GENERICTRACEBRIDGEMODULE_struct_guard

#endif // __GENERICTRACE_H_
