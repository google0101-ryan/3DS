#include "aes.h"
#include "aes_lib.hpp"

#include <queue>
#include <string.h>
#include <fstream>
#include <memory/Bus.h>

const static uint8_t key_const[] = {0x1F, 0xF9, 0xE9, 0xAA, 0xC5, 0xFE, 0x04, 0x08, 0x02, 0x45,
                                     0x91, 0xDC, 0x5D, 0x52, 0x76, 0x8A};

const static uint8_t dsi_const[] = {0xFF, 0xFE, 0xFB, 0x4E, 0x29, 0x59, 0x02, 0x58, 0x2A, 0x68,
                                     0x0F, 0x5F, 0x1A, 0x4F, 0x3E, 0x79};

struct aes_cnt
{
     uint8_t dma_write_size;
    uint8_t dma_read_size;
    uint8_t mac_size;
    bool mac_input_ctrl;
    bool mac_status;
    bool out_big_endian;
    bool in_big_endian;
    bool out_word_order;
    bool in_word_order;
    uint8_t mode;
    bool irq_enable;
    bool busy;
} aes_cnt;

struct AesKey
{
    uint8_t normal[16];
    uint8_t x[16];
    uint8_t y[16];
} aes_keys[0x40], *key_current;

uint8_t normal_fifo[16], x_fifo[16], y_fifo[16];
uint8_t normal_ctr = 0, x_ctr, y_ctr;

AES_ctx lib_aes_ctx;

uint8_t keycnt = 0, keysel = 0;
uint16_t block_count = 0, mac_count = 0;

uint8_t temp_input_fifo[16];
int temp_input_ctr;

std::queue<uint32_t> output_fifo, input_fifo;

void WriteAesCnt(uint32_t value)
{
    if ((aes_cnt.in_word_order << 25) ^ (value & (1 << 25)))
    {
        normal_ctr = 0;
        x_ctr = 0;
        y_ctr = 0;
    }

    aes_cnt.dma_write_size = (value >> 12) & 0x3;
    aes_cnt.dma_read_size = (value >> 14) & 0x3;
    aes_cnt.mac_size = (value >> 16) & 0x7;
    aes_cnt.mac_input_ctrl = value & (1 << 20);
    aes_cnt.out_big_endian = value & (1 << 22);
    aes_cnt.in_big_endian = value & (1 << 23);
    aes_cnt.out_word_order = value & (1 << 24);
    aes_cnt.in_word_order = value & (1 << 25);
    aes_cnt.mode = (value >> 27) & 0x7;
    aes_cnt.irq_enable = value & (1 << 30);
    aes_cnt.busy = value & (1 << 31);

    if (value & (1 << 26))
    {
        key_current = &aes_keys[keysel & 0x3F];
        AES_init_ctx(&lib_aes_ctx, (uint8_t*)key_current->normal);
    }

    printf("[AES]: Wrote 0x%08x to aes_cnt\n", value);
}

uint32_t ReadAesCnt()
{
    uint32_t reg = 0;
    reg |= input_fifo.size();
    reg |= output_fifo.size() << 5;
    reg |= aes_cnt.dma_write_size << 12;
    reg |= aes_cnt.dma_read_size << 14;
    reg |= aes_cnt.mac_size << 16;
    reg |= aes_cnt.mac_input_ctrl << 20;
    reg |= aes_cnt.mac_status << 21;
    reg |= aes_cnt.out_big_endian << 22;
    reg |= aes_cnt.in_big_endian << 23;
    reg |= aes_cnt.out_word_order << 24;
    reg |= aes_cnt.in_word_order << 25;
    reg |= aes_cnt.mode << 27;
    reg |= aes_cnt.irq_enable << 30;
    reg |= aes_cnt.busy << 31;
    return reg;
}

uint8_t AES_CTR[16];
uint8_t crypt_results[16];

void AES::Reset()
{
    keysel = 0;
    keycnt = 0;
    key_current = nullptr;
    normal_ctr = 0;

    AES_init_ctx(&lib_aes_ctx, (uint8_t*)aes_keys[0x3F].normal);
}

uint32_t bswp32(uint32_t value)
{
    return (value >> 24) |
            (((value >> 16) & 0xFF) << 8) |
            (((value >> 8) & 0xFF) << 16) |
            (value << 24);
}

void input_vector(uint8_t* vector, int index, uint32_t value, int max_words, bool force_order)
{
    if (!aes_cnt.in_word_order && !force_order)
        index = (max_words - 1) - index;
    
    if (!aes_cnt.in_big_endian)
        value = bswp32(value);
    index <<= 2;
    vector[index] = value & 0xFF;
    vector[index + 1] = (value >> 8) & 0xFF;
    vector[index + 2] = (value >> 16) & 0xFF;
    vector[index + 3] = value >> 24;
}

void decrypt_cbc()
{
    printf("[AES]: Decrypt CBC\n");
    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i*4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CBC_decrypt_buffer(&lib_aes_ctx, (uint8_t*)crypt_results, 16);
}

void encrypt_cbc()
{
    printf("[AES] Encrypt CBC\n");
    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CBC_encrypt_buffer(&lib_aes_ctx, (uint8_t*)crypt_results, 16);
}

void crypt_ctr()
{
    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_CTR_xcrypt_buffer(&lib_aes_ctx, (uint8_t*)crypt_results, 16);
}

void decrypt_ecb()
{
    printf("[AES] Decrypt ECB\n");

    for (int i = 0; i < 4; i++)
    {
        *(uint32_t*)&crypt_results[i * 4] = input_fifo.front();
        input_fifo.pop();
    }

    AES_ECB_decrypt(&lib_aes_ctx, (uint8_t*)crypt_results);
}

void crypt_check()
{
    if (input_fifo.size() >= 4 && output_fifo.size() <= 12 && aes_cnt.busy)
    {
        switch (aes_cnt.mode)
        {
        case 0x2:
        case 0x3:
            crypt_ctr();
            break;
        case 0x4:
            decrypt_cbc();
            break;
        case 0x5:
            encrypt_cbc();
            break;
        case 0x6:
            decrypt_ecb();
            break;
        default:
            printf("[AES]: Unhandled mode %d\n", aes_cnt.mode);
            exit(1);
        }

        for (int i = 0; i < 4; i++)
        {
            int index = i << 2;
            if (!aes_cnt.out_word_order)
            {
                index = 12 - index;
            }

            uint32_t value = *(uint32_t*)&crypt_results[index];
            if (!aes_cnt.out_big_endian)
                value = bswp32(value);
            output_fifo.push(value);
        }

        block_count--;

        if (!block_count)
        {
            aes_cnt.busy = false;
            if (aes_cnt.irq_enable)
                Bus::SetInterruptPending9(15);
        }
    }
}

void write_input_fifo(uint32_t value)
{
    input_vector((uint8_t*)temp_input_fifo, temp_input_ctr, value, 4, false);
    temp_input_ctr++;
    if (temp_input_ctr == 4)
    {
        temp_input_ctr = 0;
        for (int i = 0; i < 4; i++)
        {
            input_fifo.push(*(uint32_t*)&temp_input_fifo[i*4]);
            printf("[AES]: Input fifo 0x%08x\n", *(uint32_t*)&temp_input_fifo[i*4]);
        }
    }

    crypt_check();
}

void n128_lrot(unsigned char *num, unsigned long shift)
{
    unsigned long tmpshift;
    unsigned int i;
    unsigned char tmp[16];

    while(shift)
    {
        tmpshift = shift;
        if(tmpshift>=8)tmpshift = 8;

        if(tmpshift==8)
        {
            for(i=0; i<16; i++)tmp[i] = num[i == 15 ? 0 : i+1];

            memcpy(num, tmp, 16);
        }
        else
        {
            for(i=0; i<16; i++)tmp[i] = (num[i] << tmpshift) | (num[i == 15 ? 0 : i+1] >> (8-tmpshift));
            memcpy(num, tmp, 16);
        }

        shift-=tmpshift;
    }
}

void n128_rrot(unsigned char *num, unsigned long shift)
{
    unsigned long tmpshift;
    unsigned int i;
    unsigned char tmp[16];

    while(shift)
    {
        tmpshift = shift;
        if(tmpshift>=8)tmpshift = 8;

        if(tmpshift==8)
        {
            for(i=0; i<16; i++)tmp[i] = num[i == 0 ? 15 : i-1];

            memcpy(num, tmp, 16);
        }
        else
        {
            for(i=0; i<16; i++)tmp[i] = (num[i] >> tmpshift) | (num[i == 0 ? 15 : i-1] << (8-tmpshift));
            memcpy(num, tmp, 16);
        }

        shift-=tmpshift;
    }
}

void n128_add(unsigned char *a, unsigned char *b)
{
    unsigned int i, carry=0, val;
    unsigned char tmp[16];
    unsigned char tmp2[16];
    unsigned char *out = (unsigned char*)a;

    memcpy(tmp, (unsigned char*)a, 16);
    memcpy(tmp2, (unsigned char*)b, 16);

    for(i=0; i<16; i++)
    {
        val = tmp[15-i] + tmp2[15-i] + carry;
        out[15-i] = (unsigned char)val;
        carry = val >> 8;
    }
}

void gen_normal_key(int slot)
{
    uint8_t normal[16];
    memcpy(normal, aes_keys[slot].x, 16);

    n128_lrot((uint8_t*)normal, 2);

    for (int i = 0; i < 16; i++)
        normal[i] ^= aes_keys[slot].y[i];
    
    n128_add((uint8_t*)normal, (uint8_t*)key_const);
    n128_rrot((uint8_t*)normal, 41);

    printf("[AES]: Generated key: ");
    for (int i = 0; i < 16; i++)
        printf("%02x", normal[i]);
    printf("\n");

    memcpy(aes_keys[slot].normal, normal, 16);
}

void gen_dsi_key(int slot)
{
    uint8_t normal[16];
    memcpy(normal, aes_keys[slot].x, 16);

    for (int i = 0; i < 16; i++)
        normal[i] ^= aes_keys[slot].y[i];
    
    n128_add((uint8_t*)normal, (uint8_t*)dsi_const);
    n128_lrot((uint8_t*)normal, 42);

    printf("[AES]: Generated DSi key is 0x");
    for (int i = 0; i < 16; i++)
        printf("%02x", normal[i]);
    printf("\n");

    memcpy(aes_keys[slot].normal, normal, 16);
}

void AES::Write32(uint32_t addr, uint32_t data)
{
    if (addr == 0x10009100)
    {
        printf("[AES]: Write 0x%08x to keyfifo\n", data);
        input_vector((uint8_t*)normal_fifo, normal_ctr, data, 4, false);
        normal_ctr++;

        if (normal_ctr >= 4)
        {
            normal_ctr = 0;
            memcpy(aes_keys[keycnt & 0x3F].normal, normal_fifo, 16);
        }
        return;
    }

    if (addr >= 0x10009020 && addr < 0x10009030)
    {
        printf("[AES]: Write 0x%08x to ctr: 0x%08x\n", data, addr);

        input_vector((uint8_t*)AES_CTR, 3 - ((addr / 4) & 0x3), data, 4, true);
        AES_ctx_set_iv(&lib_aes_ctx, (uint8_t*)AES_CTR);
        return;
    }

    if (addr >= 0x10009040 && addr < 0x10009100)
    {
        addr -= 0x10009040;
        int key = (addr / 48) & 0x3;
        int fifo_id = (addr / 16) % 3;
        int offset = 3 - ((addr / 4) & 0x3);

        switch (fifo_id)
        {
        case 0:
            printf("[AES] Write to DSi KEY%d NORMAL: 0x%08x\n", key, data);
            input_vector((uint8_t*)aes_keys[key].normal, offset, data, 4, true);
            break;
        case 1:
            printf("[AES] Write to DSi KEY%d X: 0x%08x\n", key, data);
            input_vector((uint8_t*)aes_keys[key].x, offset, data, 4, true);
            break;
        case 2:
            printf("[AES] Write to DSi KEY%d Y: 0x%08x\n", key, data);
            input_vector((uint8_t*)aes_keys[key].y, offset, data, 4, true);
            gen_dsi_key(key);
            break;
        default:
            printf("ERROR: Write to unknown DSi register 0x%08x (%d)\n", addr + 0x10009040, fifo_id);
            exit(1);
        }

        return;
    }

    switch (addr)
    {
    case 0x10009000:
        WriteAesCnt(data);
        break;
    case 0x10009004:
        mac_count = data & 0xffff;
        block_count = (data >> 16);
        printf("[AES] MAC count: $%08X\n", mac_count);
        return;
    case 0x10009104:
        printf("[AES]: Write xfifo: 0x%08x\n", data);
        input_vector((uint8_t*)x_fifo, x_ctr, data, 4, false);
        x_ctr++;

        if (x_ctr >= 4)
        {
            x_ctr = 0;
            memcpy(aes_keys[keycnt & 0x3F].x, x_fifo, 16);
        }
        return;
    case 0x10009108:
        printf("[AES]: Write yfifo: 0x%08x\n", data);
        input_vector((uint8_t*)y_fifo, y_ctr, data, 4, false);
        y_ctr++;

        if (y_ctr >= 4)
        {
            y_ctr = 0;
            memcpy(aes_keys[keycnt & 0x3F].y, y_fifo, 16);
            gen_normal_key(keycnt & 0x3F);
        }
        return;
    case 0x10009008:
        write_input_fifo(data);
        break;
    default:
        printf("[AES]: Write to unknown register 0x%08x\n", addr);
        exit(1);
    }
}

uint32_t most_recent_output = 0;

std::ofstream otp_file("otp.out");

uint32_t AES::Read32(uint32_t addr)
{
    uint32_t reg = 0;
    switch (addr)
    {
    case 0x10009000:
        reg = ReadAesCnt();
        break;
    case 0x1000900C:
        if (output_fifo.size())
        {
            most_recent_output = output_fifo.front();
            output_fifo.pop();
        }
        reg = most_recent_output;
        crypt_check();
        otp_file.write((char*)&reg, sizeof(reg));
        break;
    default:
        printf("[AES]: Read from unknown register 0x%08x\n", addr);
        exit(1);
    }

    return reg;
}

uint8_t AES::ReadKEYCNT()
{
    return keycnt;
}

void AES::WriteKEYCNT(uint8_t data)
{
    printf("[AES]: Writing 0x%02x to keycnt\n", data);
    keycnt = data;
}

void AES::WriteKEYSEL(uint8_t data)
{
    printf("[AES]: Writing 0x%02x to keysel\n", data);
    keysel = data;
}

void AES::WriteBlockCount(uint16_t data)
{
    block_count = data;
}
