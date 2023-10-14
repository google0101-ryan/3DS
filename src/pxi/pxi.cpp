#include "pxi.h"

#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <arm/mpcore_pmr.h>
#include <memory/Bus.h>

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

std::queue<uint32_t> fifo11, fifo9;
uint32_t last_read11, last_read9;

void PXI::WriteSync11(uint32_t data)
{
    bool send_irq9 = (data >> 31) & 1;
    uint8_t send_data = (data >> 8) & 0xff;

    if (sync9.enable_remote_irq && send_irq9)
        Bus::SetInterruptPending9(12);

    sync11.enable_remote_irq = (data >> 31) & 1;

    sync9.recv = send_data;
    printf("[SYNC11]: Write 0x%08x to ARM9\n", data);
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
        std::queue<uint32_t> empty;
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

uint32_t PXI::ReadRecv11()
{
    if (fifo9.size())
    {
        last_read11 = fifo9.front();
        fifo9.pop();
    }
    printf("[PXI] Reading from FIFO9 (0x%08x)\n", last_read11);
    return last_read11;
}

void PXI::WriteSync9(uint32_t data)
{
    bool send_irq11_51 = (data >> 30) & 1;
    bool send_irq11_50 = (data >> 29) & 1;
    uint8_t send_data = (data >> 8) & 0xff;

    sync9.enable_remote_irq = (data >> 31) & 1;

    if (sync11.enable_remote_irq && (send_irq11_50 || send_irq11_51))
    {
        if (send_irq11_50)
            MPCore_PMR::AssertHWIrq(0x50);
        else if (send_irq11_51)
            MPCore_PMR::AssertHWIrq(0x51);
    }

    sync11.recv = send_data;
    printf("[SYNC9]: Sent 0x%08x to ARM11\n", data);
}

uint32_t PXI::ReadSync9()
{
    return (sync9.enable_remote_irq << 31) | sync9.recv;
}

void PXI::WriteCnt9(uint16_t data)
{
    cnt9.send_fifo_empty_irqen = (data >> 2) & 1;
    cnt9.recv_fifo_not_empty_irqen = (data >> 10) & 1;
    cnt9.error &= ~((data >> 14) & 1);
    cnt9.enable_send_recv = (data >> 15) & 1;

    if ((data >> 3) & 1)
    {
        std::queue<uint32_t> empty;
        fifo9.swap(empty);
        last_read11 = 0;
    }

    printf("[CNT9]: Wrote 0x%04x\n", data);
}

uint16_t PXI::ReadCnt9()
{
    cnt9.send_fifo_empty = fifo9.empty();
    cnt9.send_fifo_full = fifo9.size() == 16;
    cnt9.recv_fifo_empty = fifo11.empty();
    cnt9.recv_fifo_full = fifo11.size() == 16;

    uint32_t reg = cnt9.send_fifo_empty;
    reg |= (cnt9.send_fifo_full << 1);
    reg |= (cnt9.send_fifo_empty_irqen << 2);
    reg |= (cnt9.recv_fifo_empty << 8);
    reg |= (cnt9.recv_fifo_full << 9);
    reg |= (cnt9.recv_fifo_not_empty_irqen << 10);
    reg |= (cnt9.error << 14);
    reg |= (cnt9.enable_send_recv << 15);
    return reg;
}

void PXI::WriteSend9(uint32_t data)
{
    printf("[PXI] Sending 0x%08x to ARM11 FIFO\n", data);
    fifo9.push(data);
}
