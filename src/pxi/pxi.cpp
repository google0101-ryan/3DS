#include "pxi.h"

#include <stdio.h>
#include <stdlib.h>
#include <queue>

struct 
{
    uint8_t recv = 0;
    bool enable_remote_irq = 0;
} sync11, sync9;

struct
{
    bool send_fifo_empty;
    bool send_fifo_full;
    bool send_fifo_empty_irqen;
    bool recv_fifo_empty;
    bool recv_fifo_full;
    bool recv_fifo_not_empty_irqen;
    bool error;
    bool enable_send_recv = true;
} cnt9, cnt11;

std::queue<uint8_t> fifo11, fifo9;

void PXI::WriteSync11(uint32_t data)
{
    bool send_irq9 = (data >> 30) & 1;
    uint8_t send_data = (data >> 8) & 0xff;

    if (sync9.enable_remote_irq && send_irq9)
    {
        printf("TODO: Send IRQ to ARM9\n");
        exit(1);
    }

    sync9.recv = send_data;
    printf("[SYNC11]: Sent 0x%02x to ARM9\n", send_data);
}

uint32_t PXI::ReadSync11()
{
    return (sync11.enable_remote_irq << 31) | sync11.recv;
}

void PXI::WriteCnt11(uint16_t data)
{
    cnt11.send_fifo_empty_irqen = (data >> 2) & 1;
    cnt11.recv_fifo_not_empty_irqen = (data >> 10) & 1;
    cnt11.error &= ~((data >> 14) & 1);
    cnt11.enable_send_recv = (data >> 15) & 1;

    if ((data >> 3) & 1)
    {
        std::queue<uint8_t> empty;
        fifo11.swap(empty);
    }

    printf("[CNT11]: Wrote 0x%04x\n", data);
}

uint16_t PXI::ReadCnt11()
{
    cnt11.send_fifo_empty = fifo11.empty();
    cnt11.send_fifo_full = fifo11.size() == 16;
    cnt11.recv_fifo_empty = fifo9.empty();
    cnt11.recv_fifo_full = fifo9.size() == 16;

    uint32_t reg = cnt11.send_fifo_empty;
    reg |= (cnt11.send_fifo_full << 1);
    reg |= (cnt11.send_fifo_empty_irqen << 2);
    reg |= (cnt11.recv_fifo_empty << 8);
    reg |= (cnt11.recv_fifo_full << 9);
    reg |= (cnt11.recv_fifo_not_empty_irqen << 10);
    reg |= (cnt11.error << 14);
    reg |= (cnt11.enable_send_recv << 15);
    return reg;
}
