#include "i2c.h"

#include <stdio.h>
#include <stdlib.h>

int AddrToBusNum(uint32_t addr)
{
    uint8_t index = (addr >> 12) & 0xFF;

    switch (index)
    {
    case 0x61: return 0;
    case 0x44: return 1;
    case 0x48: return 2;
    }

    return -1;
}

union BusCtrl
{
    uint8_t value = 0;
    struct
    {
        uint8_t stop : 1;
        uint8_t start : 1;
        uint8_t pause : 1;
        uint8_t : 1;
        uint8_t ack : 1;
        uint8_t data_dir : 1;
        uint8_t ie : 1;
        uint8_t status : 1;
    };
};

union CntEx
{
    uint16_t value = 1;
    struct
    {
        uint16_t scl_state : 1;
        uint16_t wait_for_scl : 1;
        uint16_t : 12;
        uint16_t unknown0 : 1;
    };
};

struct Bus
{
    BusCtrl ctrl;
    CntEx cnt;
    int byteindex;
    uint8_t bytes[256];

    int selectedDevice;
    bool deviceSelected = false;
    bool reading = false;

    bool regSelected = false;
    uint8_t reg = 0;
} busses[3];

void WriteMCU(int reg, uint8_t byte)
{
    switch (reg)
    {
    case 0x4A:
        break;
    default:
        printf("[I2C/MCU]: Write to unknown reg %x\n", reg);
        exit(1);
    }

    busses[1].deviceSelected = false;
    busses[1].regSelected = false;
}

void SendByteDevice(int deviceNum, int regNum, uint8_t byte)
{
    switch (deviceNum)
    {
    case 0x4A:
        return WriteMCU(regNum, byte);
    default:
        printf("[I2C]: Write to unknown device %x\n", deviceNum);
        exit(1);
    }
}

uint8_t I2C::Read8(uint32_t addr)
{
    uint8_t reg = addr & 0xFF;
    int bus = AddrToBusNum(addr);

    switch (reg)
    {
    case 1:
        return busses[bus].ctrl.value;
    default:
        printf("[I2C_%d]: Read from unknown reg %d\n", bus, reg);
        exit(1);
    }
}

void I2C::Write8(uint32_t addr, uint8_t data)
{
    uint8_t reg = addr & 0xFF;
    int bus = AddrToBusNum(addr);

    switch (reg)
    {
    case 0:
        if (!busses[bus].deviceSelected)
        {
            busses[bus].selectedDevice = data & ~1;
            busses[bus].deviceSelected = true;
            busses[bus].reading = data & 1;
        }
        else if (!busses[bus].regSelected)
        {
            busses[bus].regSelected = true;
            busses[bus].reg = data;
            printf("[I2C]: Selecting reg %x, device %d:%x\n", busses[bus].reg, bus, busses[bus].selectedDevice);
        }
        else
        {
            SendByteDevice(busses[bus].selectedDevice, busses[bus].reg, data);
        }
        break;
    case 1:
    {
        busses[bus].ctrl.value = data;
        busses[bus].ctrl.status = 0;
        break;
    }
    default:
        printf("[I2C_%d]: Read from unknown reg %d\n", bus, reg);
        exit(1);
    }
}

void I2C::Write16(uint32_t addr, uint16_t data)
{
    uint8_t reg = addr & 0xFF;
    int bus = AddrToBusNum(addr);

    switch (reg)
    {
    case 2:
        busses[bus].cnt.value = data;
        break;
    case 4:
        break;
    default:
        printf("[I2C_%d]: Write16 to unknown reg %d\n", bus, reg);
        exit(1);
    }
}
