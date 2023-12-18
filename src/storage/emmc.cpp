#include "emmc.h"

#include <queue>
#include <string.h>
#include <memory/Bus.h>

std::ifstream file, sdfile, *cur_transfer_drive;
std::ofstream dump;

uint32_t cid[4], sd_cid[4];

uint32_t regcsd[4] = {0};
uint8_t regscr[8] = {0};
uint32_t ocr_reg = 0x80FF8080;

uint16_t ctrl;

void eMMC::Initialize(std::string fileName)
{
	sdfile.open("sd.bin", std::ios::binary | std::ios::in);

	dump.open("nand_dump.bin");

    file.open(fileName, std::ios::binary);
    assert(file.is_open());
    file.seekg(0x200);

    char essentials[0x200];
    file.read((char*)essentials, 0x200);

    bool cid_found = false;

    int counter = 0;
    while (!cid_found && counter < 0x200)
    {
        uint32_t offs = *(uint32_t*)&essentials[counter + 8];

        offs += 0x400;
        if (!strncmp(essentials+counter, "nand_cid", 8))
        {
            file.seekg(offs);
            file.read((char*)cid, 16);
            cid_found = true;
        }
        counter += 0x10;
    }

    if (!cid_found)
    {
        printf("[SDMMC]: Couldn't find NAND CID\n");
        exit(1);
    }

	sd_cid[0] = 0xD71C65CD;
    sd_cid[1] = 0x4445147B;
    sd_cid[2] = 0x4D324731;
    sd_cid[3] = 0x00150100;

	regcsd[0] = 0xe9964040;
	regcsd[1] = 0xdff7db7f;
	regcsd[2] = 0x2a0f5901;
	regcsd[3] = 0x3f269001;

	memset(regscr, 0, 8);

	*(uint32_t*)&regscr[1] = 0x012a0000;
}

struct SD_DATA32_IRQ
{
    bool is32;
    bool rx32rdy_irq_flag;
    bool tx32rq_irq_flag;
    bool rx32rdy_irqen;
    bool tx32rq_irqen;
} sd_data32_irq;

std::queue<uint8_t> fifo32;
uint32_t data32_blocklen, data32_blockcount;
uint32_t irq_mask, irq_status;
uint16_t clockctrl;
uint32_t sd_cmd_param;
uint32_t cmd_block_len;
uint16_t data_blocklen, data_blockcount;
uint8_t regsd_status[64] = {0};

enum PORT
{
    SD,
    EMMC
} port = SD;

enum EMMCState
{
    IDLE,
    READY,
    IDENTIFY,
    STANDBY,
    TRANSFER,
    DATA,
    RECEIVE,
    PROGRAM
} state = IDLE;

uint32_t response[4];
int transfer_size;
uint8_t* transfer_buf = nullptr;
uint32_t transfer_pos = 0;
uint32_t transfer_blocks;
uint64_t transfer_start_addr;
bool block_transfer;
uint8_t nand_block[1024];

void SetIstat(uint32_t interrupt)
{
    uint32_t old_istat = irq_status;
    irq_status |= interrupt;

    printf("ISTAT: $%08X IMSK: $%08X COMB: $%08X\n", irq_status, irq_mask, irq_status & irq_mask);

    if (!(old_istat & irq_mask & interrupt) && (irq_status & irq_mask & interrupt))
        Bus::SetInterruptPending9(16);
}

void data_ready()
{
    sd_data32_irq.tx32rq_irq_flag = false;
    sd_data32_irq.rx32rdy_irq_flag = true;

    if (sd_data32_irq.rx32rdy_irqen)
        SetIstat(0x01000000);
}

void command_end()
{
    SetIstat(1);
}

void transfer_end()
{
    transfer_buf = nullptr;
	block_transfer = false;
    sd_data32_irq.rx32rdy_irq_flag = false;
    switch (state)
    {
    case DATA:
    case RECEIVE:
        state = TRANSFER;
        break;
	case TRANSFER:
		state = STANDBY;
		break;
    default:
        state = STANDBY;
        break;
    }

    irq_status &= ~0x1;
    SetIstat(4);
    command_end();
}

uint16_t read_fifo()
{
    if (transfer_size)
    {
        uint16_t value = *(uint16_t*)&transfer_buf[transfer_pos];
        transfer_pos += 2;
        transfer_size -= 2;

		printf("[EMMC]: Read FIFO16: 0x%04x\n", value);

        if (!transfer_size)
        {
            transfer_end();
        }

        return value;
    }
    return 0;
}

uint32_t eMMC::read_fifo32()
{
    if (transfer_size)
    {
        uint32_t value = *(uint32_t*)&transfer_buf[transfer_pos];
        transfer_pos += 4;
        transfer_size -= 4;

		dump.write((char*)&value, 4);

		printf("[EMMC]: Read FIFO32: 0x%08x\n", value);

        if (!transfer_size)
        {
            data_ready();
            transfer_pos = 0;
            if (block_transfer)
            {
                transfer_blocks--;
                if (!transfer_blocks)
                    transfer_end();
                else
                {
					printf("TODO: NDMA transfer\n");
                    transfer_size = data_blocklen;
                    cur_transfer_drive->read((char*)transfer_buf, transfer_size);
                }
            }
            else
                transfer_end();
        }
        return value;
    }
    return 0;
}

uint16_t eMMC::Read16(uint32_t addr)
{
    if (addr >= 0x1000600C && addr < 0x1000601C)
    {
        uint32_t reg = 0;
        int index = ((addr - 0x1000600C) / 4) & 0x3;
        if (addr % 4 == 2)
            reg = response[index] >> 16;
        else
            reg = response[index] & 0xFFFF;
        //printf("[EMMC] Read response $%08X: $%04X\n", addr, reg);
        return reg;
    }

    switch (addr)
    {
    case 0x10006002:
        return port;
    case 0x10006008:
        return 0;
    case 0x1000601C:
        return (irq_status & 0xFFFF) | (1 << 5) | (1 << 7);
    case 0x1000601E:
        return irq_status >> 16;
    case 0x10006020:
        return irq_mask & 0xFFFF;
    case 0x10006022:
        return irq_mask >> 16;
    case 0x10006024:
        return clockctrl;
    case 0x10006026:
        return data_blocklen;
    case 0x10006028:
        return 0;
    case 0x10006030:
        return read_fifo();
    case 0x100060D8:
		return ctrl;
    case 0x10006036:
    case 0x10006038:
    case 0x100060e0:
    case 0x100060f8:
    case 0x100060fa:
    case 0x100060fc:
    case 0x100060fe:
        return 0;
    case 0x10006100:
    {
        uint16_t reg = sd_data32_irq.is32 << 1;
        reg |= sd_data32_irq.rx32rdy_irq_flag << 8;
        reg |= sd_data32_irq.tx32rq_irq_flag << 9;
        reg |= sd_data32_irq.rx32rdy_irqen << 11;
        reg |= sd_data32_irq.tx32rq_irqen << 12;
        return reg;
    }
    default:
        printf("[SDMMC]: Read from unknown addr 0x%08x\n", addr);
        exit(1);
    }
}

bool acmd = false;

uint32_t get_r1_reply()
{
    uint32_t reg = acmd << 5;
    reg |= state << 9;
    if (!transfer_size)
        reg |= 1 << 8;
    return reg;
}

void DoCommand(uint8_t command)
{
    if (acmd)
    {
		irq_status &= ~1;

        switch (command)
        {
        case 6:
            printf("[SDMMC]: SET_BUS_WIDTH\n");
            *(uint32_t*)&regsd_status[60] = ((*(uint32_t*)&regsd_status[60] & ~3) << 30) | sd_cmd_param << 30;
            response[0] = get_r1_reply();
            command_end();
            break;
        case 13:
            printf("[SDMMC]: SD_STATUS\n");
            response[0] = get_r1_reply();
            sd_data32_irq.rx32rdy_irq_flag = true;
            SetIstat(0x01000000);

            transfer_buf = regsd_status;
            transfer_size = sizeof(regsd_status);
            transfer_pos = 0;
            command_end();
            // data_ready();
            break;
        case 41:
            printf("[SDMMC]: SD_SEND_OP_COND\n");
			if (port == 1)
	            response[0] = 0x80FF8080;
			else
				response[0] = 0x80FF8080 | (1 << 30);
            command_end();
            if (state == EMMCState::IDLE)
                state = EMMCState::READY;
            break;
        case 42:
            printf("[SDMMC]: SET_CLR_CARD_DETECT\n");
            response[0] = get_r1_reply();
            command_end();
            break;
        case 51:
            printf("[SDMMC]: GET_SCR\n");
            response[0] = get_r1_reply();
            sd_data32_irq.rx32rdy_irq_flag = true;
            SetIstat(0x01000000);

            transfer_buf = regscr;
            transfer_size = sizeof(regscr);
            transfer_pos = 0;
            command_end();
            data_ready();
            break;
        default:
            printf("[SDMMC]: Unknown acmd %d\n", command);
            exit(1);
        }

        acmd = false;
    }
    else
    {
        switch (command)
        {
        case 0:
            printf("[SDMMC]: GO_TO_IDLE\n");
			irq_status = 0;
			response[0] = 1 << 9;
			command_end();
            state = EMMCState::IDLE;
            break;
        case 1:
            printf("[SDMMC] SEND_OP_COND\n");
            response[0] = ocr_reg;
            command_end();
            break;
        case 2:
            printf("[SDMMC]: ALL_GET_CID\n");
			if (port == 1)
	            memcpy(response, cid, 16);
			else
				memcpy(response, sd_cid, 16);
            command_end();
            if (state == EMMCState::READY)
                state = EMMCState::IDENTIFY;
            break;
        case 3:
            printf("[SDMMC]: SET_RELATIVE_ADDR\n");
            response[0] = 0x10000 | get_r1_reply();
			command_end();
            if (state == EMMCState::IDENTIFY)
                state = EMMCState::STANDBY;
            break;
        case 6:
            printf("[SDMMC]: SWITCH_FUNC\n");
            response[0] = get_r1_reply();
            command_end();

            if (state == TRANSFER)
                state = PROGRAM;
            break;
        case 7:
            printf("[SDMMC]: SELECT_DESELECT_CARD\n");
            response[0] = get_r1_reply();
            command_end();
            break;
        case 8:
            printf("[SDMMC]: GET_EXT_CSD\n");
            response[0] = 0x1AA;
            command_end();
            break;
        case 9:
            printf("[SDMMC]: GET_CSD\n");
            memcpy(response, regcsd, 16);
            command_end();
            break;
        case 10:
            printf("[SDMMC]: GET_CID\n");
            memcpy(response, cid, 16);
            command_end();
            break;
        case 13:
            printf("[SDMMC]: GET_STATUS\n");
            response[0] = get_r1_reply();
            command_end();
            break;
        case 16:
            cmd_block_len = sd_cmd_param;
            printf("[SDMMC]: SET_BLOCKLEN (0x%08x)\n", sd_cmd_param);
            command_end();
            break;
        case 18:
            transfer_start_addr = sd_cmd_param;
            state = TRANSFER;
            response[0] = get_r1_reply();
            state = DATA;
            transfer_pos = 0;
            transfer_blocks = data_blockcount;
            transfer_size = data_blocklen;
            block_transfer = true;

			if (port == 1)
	            cur_transfer_drive = &file;
			else
			{
				cur_transfer_drive = &sdfile;
				transfer_start_addr *= data_blocklen;
			}

            printf("[EMMC] Read multiple blocks (%s) (start: $%lX blocks: $%08X)\n", (port == SD) ? "SD" : "NAND", transfer_start_addr, data_blockcount);

            if (cur_transfer_drive->eof())
                cur_transfer_drive->clear();
            
            cur_transfer_drive->seekg(transfer_start_addr, std::ios::beg);
            cur_transfer_drive->read((char*)nand_block, transfer_size);

            transfer_buf = (uint8_t*)nand_block;
            data_ready();
            break;
        case 55:
            printf("[SDMMC]: ACMD prefix\n");
            acmd = true;
            response[0] = get_r1_reply();
            command_end();
            break;
        default:
            printf("[SDMMC]: Unknown command %d\n", command);
            exit(1);
        }
    }
}

bool firstTime = true;

void eMMC::Write16(uint32_t addr, uint16_t data)
{
    switch (addr)
    {
    case 0x10006000:
        DoCommand(data & 0x3F);
        return;
    case 0x10006002:
        port = (PORT)(data & 1);
        // if (firstTime)
        //     firstTime = false;
        // else
        //     assert(port == EMMC);
        printf("[SDMMC]: Selected port %d (%s)\n", (int)port, port == SD ? "sd" : "emmc");
        return;
    case 0x10006004:
        sd_cmd_param &= ~0xFFFF;
        sd_cmd_param |= data;
        printf("[SDMMC]: Wrote 0x%04x to SD_CMD_PARAM0\n", data);
        return;
    case 0x10006006:
        sd_cmd_param &= 0xFFFF;
        sd_cmd_param |= (data << 16);
        printf("[SDMMC]: Wrote 0x%04x to SD_CMD_PARAM1\n", data);
        return;
    case 0x10006008:
        return;
    case 0x1000600A:
        data_blockcount = data;
        printf("[SDMMC]: Write 0x%04x to SD_DATA16_BLKCOUNT\n", data);
        return;
    case 0x1000601C:
        irq_status &= (0xFFFF0000 | data);
        printf("[SDMMC]: Wrote 0x%04x to SD_IRQ_STAT0\n", data);
        return;
    case 0x1000601E:
        irq_status &= ((data << 16) | 0xFFFF);
        printf("[SDMMC]: Wrote 0x%04x to SD_IRQ_STAT1\n", data);
        return;
    case 0x10006020:
        irq_mask &= ~0xFFFF;
        irq_mask |= data;
        printf("[SDMMC]: Wrote 0x%04x to SD_IRQ_MASK0\n", data);
        return;
    case 0x10006022:
        irq_mask &= 0xFFFF;
        irq_mask |= (data << 16);
        printf("[SDMMC]: Wrote 0x%04x to SD_IRQ_MASK1\n", data);
        return;
    case 0x10006024:
        clockctrl = data;
        return;
    case 0x10006026:
        data_blocklen = data;
        printf("[SDMMC]: Write 0x%04x to SD_DATA16_BLKLEN\n", data);
        return;
    case 0x100060D8:
		ctrl = data;
		return;
    case 0x10006028:
    case 0x100060e0:
    case 0x100060f8:
    case 0x100060fc:
    case 0x100060fe:
        return;
    case 0x10006100:
    {
        sd_data32_irq.is32 = (data >> 1) & 1;
        sd_data32_irq.rx32rdy_irqen = (data >> 11) & 1;
        sd_data32_irq.tx32rq_irqen = (data >> 12) & 1;
        return;
    }
    case 0x10006104:
        data32_blocklen = data & 0x3FF;
        printf("[SDMMC]: Write 0x%08x to blocklen\n", data);
        return;
    case 0x10006108:
        data32_blockcount = data;
        printf("[SDMMC]: Write 0x%08x to blockcount\n", data);
        return;
    default:
        printf("[SDMMC]: Write to unknown addr 0x%08x\n", addr);
        exit(1);
    }
}
