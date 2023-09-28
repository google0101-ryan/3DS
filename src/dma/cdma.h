#pragma once

#include <functional>
#include <stdint-gcc.h>

typedef std::function<void(uint32_t, uint32_t)> write_func_t;
typedef std::function<uint32_t(uint32_t)> read_func_t;
typedef std::function<uint8_t(uint32_t)> read8_func_t;

class CDMA
{
private:
    write_func_t write32_func;
    read_func_t read32_func;
    read8_func_t read8_func;

    enum ChannelStatus
    {
        STOPPED,
        EXECUTING,
        CACHE_MISS,
        UPDATING_PC,
        WAITING_FOR_EVENT,
        AT_BARRIER,
        RESERVED0,
        WAIT_FOR_PERIPHERAL,
        KILLING,
        COMPLETING,
        RESERVED1,
        FAULTING_COMPLETING,
        FAULTING
    };

    struct Channel
    {
        union
        {
            uint32_t value;
            struct
            {
                uint32_t status : 3;
                uint32_t wakeup_number : 5;
                uint32_t : 5;
                uint32_t dmawfp_burst : 1;
                uint32_t dmawfp_with_periph : 1;
                uint32_t : 5;
                uint32_t non_secure : 1;
                uint32_t : 10;
            };
        } chan_status;

        uint32_t pc;
    } chans[8]; // Channels 0-7

    uint32_t inten;

    uint32_t instr0, instr1;
    bool running = false;

    void ExecChannel(Channel& chan);
public:
    CDMA(write_func_t write, read_func_t read, read8_func_t read8);

    void run();
    void Tick();

    uint32_t Read32(uint32_t addr);
    void Write32(uint32_t addr, uint32_t data);
};