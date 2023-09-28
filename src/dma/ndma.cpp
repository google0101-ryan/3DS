#include "ndma.h"

#include <stdio.h>
#include <stdlib.h>

struct NdmaChannel
{
    uint32_t repeat_count;
} ndma_channels[8];

uint32_t NDMA::Read32(uint32_t addr)
{
    addr &= 0xFF;

    if (addr == 0x00)
    {
        printf("TODO: Read from global control\n");
        exit(1);
    }
    else
    {
        int chan = (addr - 4) / 0x1C;
        int reg = (addr - 4) % 0x1C;

        switch (reg)
        {
        default:
            printf("ERROR: Read from unknown NDMA register %x on channel %d\n", reg, chan);
            exit(1);
        }
    }
}