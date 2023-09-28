#include "cdma.h"

#include <string.h>
#include <stdio.h>

void CDMA::ExecChannel(Channel &chan)
{
    while (chan.chan_status.status == EXECUTING)
    {
        printf("0x%08x: ", chan.pc);
        uint8_t opcode = read8_func(chan.pc++);

        switch (opcode)
        {
        case 0x00:
        {
            printf("DMAEND\n");
            chan.chan_status.status = STOPPED;
            break;
        }
        case 0x35:
        {
            uint8_t peripheral = read8_func(chan.pc++);
            printf("DMAFLUSHP %d\n", peripheral);
            break;
        }
        default:
            printf("Unknown CDMA opcode 0x%02x\n", opcode);
            exit(1);
        }
    }
}

CDMA::CDMA(write_func_t write, read_func_t read, read8_func_t read8)
{
    write32_func = write;
    read32_func = read;
    read8_func = read8;
    
    memset(chans, 0, sizeof(chans));
}

enum Opcodes
{
    DMAKILL = 1,
    DMAGO = 0xA2,
};

void CDMA::run()
{
    uint8_t chan = (instr0 >> 8) & 0x7;
    uint8_t instr = (instr0 >> 16) & 0xFF;

    switch (instr)
    {
    case DMAKILL:
        chans[chan].chan_status.status = STOPPED;
        printf("[DBG_CDMA]: Killing channel %d\n", chan);
        break;
    case DMAGO:
        chans[chan].pc = instr1;
        chans[chan].chan_status.status = EXECUTING;
        printf("[DBG_CDMA]: DMAGO at address 0x%08x\n", instr1);
        break;
    default:
        printf("[DBG_CDMA]: Unknown instr 0x%02x\n", instr);
        exit(1);
    }
}

void CDMA::Tick()
{
    for (int i = 0; i < 8; i++)
    {
        if (chans[i].chan_status.status == EXECUTING)
        {
            ExecChannel(chans[i]);
        }
    }
}

uint32_t CDMA::Read32(uint32_t addr)
{
    int chan = (addr >> 4) & 0x7;

    switch (addr & 0xFFF)
    {
    case 0x02C:
        return 0;
    case 0x020:
        return inten;
    case 0x100:
        return chans[chan].chan_status.value;
    case 0xD00:
        return running;
    default:
        printf("Read from unknown addr 0x%08x\n", addr);
        exit(1);
    }
}

void CDMA::Write32(uint32_t addr, uint32_t data)
{
    int chan = (addr >> 4) & 0x7;

    switch (addr & 0xFFF)
    {
    case 0x02C:
        break;
    case 0x020:
        inten = data;
        break;
    case 0xD04:
        run();
        break;
    case 0xD08:
        instr0 = data;
        break;
    case 0xD0C:
        instr1 = data;
        break;
    default:
        printf("Read from unknown addr 0x%08x\n", addr);
        exit(1);
    }
}
