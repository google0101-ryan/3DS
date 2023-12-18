#include "i2c.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
	uint8_t data;

    int selectedDevice;
    bool deviceSelected = false;
    bool reading = false;
} busses[3];

struct Device
{
	uint8_t cur_reg;
	bool reg_selected;
} devices[3][0x100];

void WriteMCU(int reg, uint8_t byte)
{
    switch (reg)
    {
    case 0x4A:
        break;
    default:
        printf("[I2C/MCU]: Write 0x%02x to unknown reg %x\n", byte, reg);
        exit(1);
    }
}

uint8_t ReadMCU(int reg)
{
	switch (reg)
    {
	case 0x0F:
		return (1 << 1) /*Shell open*/;
    default:
        printf("[I2C/MCU]: Read from unknown reg %x\n", reg);
        exit(1);
    }
}

void SendByteDevice(int bus, int deviceNum, uint8_t byte)
{
    switch ((bus << 8) | deviceNum)
    {
    case 0x14A:
        return WriteMCU(devices[bus][deviceNum].cur_reg, byte);
    default:
        printf("[I2C%d]: Write to unknown device %x\n", bus, deviceNum);
        exit(1);
    }
}

uint8_t ReadByteDevice(int bus, int deviceNum)
{
	switch ((bus << 8) | deviceNum)
    {
	case 0x14A:
		return ReadMCU(devices[bus][deviceNum].cur_reg);
	default:
        printf("[I2C%d]: Read from unknown device %x\n", bus, deviceNum);
        exit(1);
    }
}

uint8_t I2C::Read8(uint32_t addr)
{
    uint8_t reg = addr & 0xFF;
    int bus = AddrToBusNum(addr);

    switch (reg)
    {
	case 0:
		return busses[bus].data;
    case 1:
        return busses[bus].ctrl.value;
    default:
        printf("[I2C_%d]: Read from unknown reg %d\n", bus, reg);
        exit(1);
    }
}

void DoTransfer(int id)
{
	auto& bus = busses[id];

	bus.ctrl.status = 0;
	if (bus.ctrl.start)
	{
		uint8_t dev = bus.data & ~1;
		if (!bus.deviceSelected)
		{
			devices[id][dev].reg_selected = false;
			printf("[I2C%d] Selecting device 0x%02x\n", id, dev);
		}
		bus.deviceSelected = true;
		bus.selectedDevice = dev;
	}
	else
	{
		uint8_t cur_dev = bus.selectedDevice;
		if (!devices[id][cur_dev].reg_selected)
		{
			devices[id][cur_dev].reg_selected = true;
			devices[id][cur_dev].cur_reg = bus.data;

			printf("[I2C%d]: Selecting device 0x%02x, reg 0x%02x\n", id, cur_dev, bus.data);
		}
		else
		{
			if (bus.ctrl.data_dir)
				bus.data = ReadByteDevice(id, cur_dev);
			else
				SendByteDevice(id, cur_dev, bus.data);
			devices[id][cur_dev].cur_reg++;
		}
		if (bus.ctrl.stop)
		{
			devices[id][cur_dev].reg_selected = false;
			bus.deviceSelected = false;
		}
	}
}

void I2C::Write8(uint32_t addr, uint8_t data)
{
    uint8_t reg = addr & 0xFF;
    int bus = AddrToBusNum(addr);

    switch (reg)
    {
    case 0:
		busses[bus].data = data;
        break;
    case 1:
    {
        busses[bus].ctrl.value = data;
        busses[bus].ctrl.status = 0;
		if (data & (1 << 7))
		{
			busses[bus].ctrl.ack = true;
			DoTransfer(bus);
		}
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
