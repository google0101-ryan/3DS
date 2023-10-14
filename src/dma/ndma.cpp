#include "ndma.h"

#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <memory/Bus.h>

struct NdmaChannel
{
    uint32_t repeat_count;
    union
    {
        uint32_t value;
        struct
        {
            uint32_t : 10;
            uint32_t dest_addr_update : 2;
            uint32_t dest_addr_reload : 1;
            uint32_t source_addr_update : 2;
            uint32_t source_addr_reload : 1;
            uint32_t phys_block_size : 4;
            uint32_t : 4;
            uint32_t startup_mode : 5;
            uint32_t repeat_mode : 1;
            uint32_t ie : 1;
            uint32_t start : 1;
        };
    } ctrl = {0};
    uint32_t source_addr = 0, dest_addr = 0;
    uint32_t transfer_count = 0, write_count = 0;
    uint32_t fill_data;
    uint32_t int_source, int_dest;
} ndma_channels[8];

uint32_t NDMA::Read32(uint32_t addr)
{
    addr &= 0xFF;

    if (addr == 0x00)
    {
        return 0;
    }
    else if (addr >= 4)
    {
        int chan = (addr - 4) / 0x1C;
        int reg = (addr - 4) % 0x1C;

        if (chan > 7)
            return 0;

        switch (reg)
        {
        case 0x00:
            return ndma_channels[chan].source_addr;
        case 0x18:
            printf("[NDMA]: Reading DMACNT from channel %d\n", chan);
            return ndma_channels[chan].ctrl.value;
        default:
            printf("ERROR: Read from unknown NDMA register %x on channel %d\n", reg, chan);
            exit(1);
        }
    }
    return 0;
}

void RunNDMA(int chan_num)
{
    auto& chan = ndma_channels[chan_num];
    int dest_multiplier, src_multiplier;

    switch (chan.ctrl.dest_addr_update)
    {
    case 0: dest_multiplier = 4; break;
    case 1: dest_multiplier = -4; break;
    case 2: dest_multiplier = 0; break;
    default:
        printf("[NDMA]: Unhandled destination update mode %d\n", chan.ctrl.dest_addr_reload);
        exit(1);
    }

    switch (chan.ctrl.source_addr_update)
    {
    case 0: src_multiplier = 4; break;
    case 1: src_multiplier = -4; break;
    case 2: src_multiplier = 0; break;
    default:
        printf("[NDMA]: Unhandled source update mode %d\n", chan.ctrl.dest_addr_reload);
        exit(1);
    }

    uint32_t block_size = chan.write_count;

    for (int i = 0; i < block_size; i++)
    {
        uint32_t word = Bus::ARM9::Read32(chan.int_source + (i * src_multiplier));
        Bus::ARM9::Write32(chan.int_dest + (i * dest_multiplier), word);
    }

    if (!chan.ctrl.dest_addr_reload)
        chan.int_dest += (block_size * dest_multiplier);

    if (!chan.ctrl.source_addr_reload)
        chan.int_source += (block_size * src_multiplier);

    chan.ctrl.start = 0;
    if (chan.ctrl.ie)
        Bus::SetInterruptPending9(chan_num);
}

void NDMA::Write32(uint32_t addr, uint32_t  data)
{
    addr &= 0xFF;

    if (addr == 0x00)
    {
        return;
    }
    else if (addr >= 4)
    {
        int chan = (addr - 4) / 0x1C;
        int reg = (addr - 4) % 0x1C;

        if (chan > 7)
            return;

        switch (reg)
        {
        case 0x00:
            printf("[NDMA]: Setting channel %d source address to 0x%08x\n", chan, data);
            ndma_channels[chan].source_addr = data;
            break;
        case 0x04:
            printf("[NDMA]: Setting channel %d dest address to 0x%08x\n", chan, data);
            ndma_channels[chan].dest_addr = data;
            break;
        case 0x08:
            printf("[NDMA]: Setting channel %d transfer count to 0x%08x\n", chan, data);
            ndma_channels[chan].transfer_count = data;
            break;
        case 0x0C:
            printf("[NDMA]: Setting channel %d write count to 0x%08x\n", chan, data);
            ndma_channels[chan].write_count = data;
            break;
        case 0x10:
            break;
        case 0x14:
            printf("[NDMA]: Setting channel %d fill data to 0x%08x\n", chan, data);
            ndma_channels[chan].fill_data = data;
            break;
        case 0x18:
        {
            printf("[NDMA]: Writing 0x%08x to DMACNT on channel %d\n", data, chan);

            bool old_busy = ndma_channels[chan].ctrl.start;
            ndma_channels[chan].ctrl.value = data;

            if (!old_busy && ndma_channels[chan].ctrl.start)
            {
                ndma_channels[chan].int_dest = ndma_channels[chan].dest_addr;
                ndma_channels[chan].int_source = ndma_channels[chan].int_dest;
                assert(ndma_channels[chan].ctrl.startup_mode >= 0x10);
                RunNDMA(chan);
            }

            break;
        }
        default:
            printf("ERROR: Write to unknown NDMA register %x on channel %d\n", reg, chan);
            exit(1);
        }
    }
}
