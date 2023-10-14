#include "rsa.h"

#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <string>
#include <memory/Bus.h>

struct RsaCnt
{
    bool irqen;
    uint8_t keyslot;
    bool byte_order;
    bool word_order;
} rsa_cnt;

struct RsaKey
{
    uint8_t exp[0x100];
    uint8_t mod[0x100];

    uint8_t mod_ctr;
    uint8_t exp_ctr;
} keys[4];

uint8_t msg[0x100];
int msg_ctr;

void convert_to_bignum(uint8_t *src, mpz_t dest)
{
    mpz_t base, temp;

    mpz_init(temp);
    mpz_init_set_ui(dest, 0);
    mpz_init_set_ui(base, 1);
    for (int i = 0xFF; i >= 0; i--)
    {
        mpz_set_ui(temp, src[i]);
        mpz_mul(temp, temp, base);
        mpz_add(dest, dest, temp);
        mpz_mul_ui(base, base, 256);
    }
}

void convert_from_bignum(mpz_t src, uint8_t* dest)
{
    std::string str = mpz_get_str(NULL, 16, src);

    while (str.length() < 0x200)
        str = '0' + str;
    
    for (int i = 0; i < 0x100; i++)
    {
        uint8_t chr1 = str[(i * 2)];
        uint8_t chr2 = str[(i * 2) + 1];

        if (chr1 >= 'a')
            chr1 = (chr1 - 'a') + 10;
        else
            chr1 -= '0';

        if (chr2 >= 'a')
            chr2 = (chr2 - 'a') + 10;
        else
            chr2 -= '0';

        uint8_t byte = (chr1 << 4) | chr2;

        dest[i] = byte;
    }
}

void do_rsa_op()
{
    mpz_t gmp_msg, gmp_b, gmp_e, gmp_m;

    RsaKey* key = &keys[rsa_cnt.keyslot];
    mpz_inits(gmp_msg, NULL);
    convert_to_bignum((uint8_t*)msg, gmp_b);
    convert_to_bignum((uint8_t*)key->exp, gmp_e);
    convert_to_bignum((uint8_t*)key->mod, gmp_m);

    printf("Msg: %s\n", mpz_get_str(NULL, 16, gmp_b));
    printf("Exp: %s\n", mpz_get_str(NULL, 16, gmp_e));
    printf("Mod: %s\n", mpz_get_str(NULL, 16, gmp_m));

    mpz_powm(gmp_msg, gmp_b, gmp_e, gmp_m);
    printf("Result: %s\n", mpz_get_str(NULL, 16, gmp_msg));

    convert_from_bignum(gmp_msg, msg);
    Bus::SetInterruptPending9(22);
}

uint8_t RSA::Read8(uint32_t addr)
{
    if (addr >= 0x1000B800 && addr < 0x1000B900)
    {
        int index = addr & 0xFF;
        if (!rsa_cnt.word_order)
            index = 0xFF - index;
        printf("[RSA]: Read result 0x%02x\n", msg[index]);
        return msg[index];
    }
    printf("[RSA] Unrecognized read8 0x%08x\n", addr);
    return 0;
}

uint32_t RSA::Read32(uint32_t addr)
{
    if (addr >= 0x1000B100 && addr < 0x1000B140)
    {
        int index = (addr / 0x10) & 0x3;
        int reg = (addr / 4) & 0x3;

        switch (reg)
        {
        case 0:
            return 1;
        case 1:
            printf("[RSA]: Read key%d size\n", index);
            return 0x40;
        }
    }

    switch (addr)
    {
    case 0x1000b000:
    {
        uint32_t reg = (rsa_cnt.irqen << 1);
        reg |= (rsa_cnt.keyslot << 4);
        reg |= (rsa_cnt.byte_order << 8);
        reg |= (rsa_cnt.word_order << 9);
        printf("[RSA]: Read cnt: 0x%02x\n", reg);
        return reg;
    }
    case 0x1000b130:
        return 1;
    default:
        printf("[RSA]: Read from unknown addr 0x%08x\n", addr);
        exit(1);
    }
}

void RSA::Write8(uint32_t addr, uint8_t data)
{
    if (addr >= 0x1000B200 && addr < 0x1000B300)
    {
        printf("[RSA] Write8 key%d exp: $%02X\n", rsa_cnt.keyslot, data);
        RsaKey* key = &keys[rsa_cnt.keyslot];

        key->exp[key->exp_ctr] = data;
        key->exp_ctr++;
        if (key->exp_ctr >= 0x100)
            key->exp_ctr = 0;
        return;
    }

    if (addr >= 0x1000B400 && addr < 0x1000B500)
    {
        printf("[RSA] Write8 key%d mod: $%02X\n", rsa_cnt.keyslot, data);
        RsaKey* key = &keys[rsa_cnt.keyslot];

        int index = key->mod_ctr;
        if (!rsa_cnt.word_order)
            index = 0xFF - index;
        
        key->mod[index] = data;
        key->mod_ctr++;
        if (key->mod_ctr >= 0x100)
            key->mod_ctr = 0;
        return;
    }

    if (addr >= 0x1000B800 && addr < 0x1000B900)
    {
        printf("[RSA] Write8 txt: 0x%02x (%d)\n", data, msg_ctr);
        if (!rsa_cnt.word_order)
            msg[0xFF - msg_ctr] = data;
        else
            msg[msg_ctr] = data;
        
        msg_ctr++;
        if (msg_ctr >= 0x100)
            msg_ctr = 0;
        return;
    }

    switch (addr)
    {
    default:
        printf("[RSA]: Write8 to unknown addr 0x%08x\n", addr);
        exit(1);
    }
}

void RSA::Write32(uint32_t addr, uint32_t data)
{
    if (addr >= 0x1000B200 && addr < 0x1000B300)
    {
        printf("[RSA]: Writing 0x%08x to key%d exp\n", data, rsa_cnt.keyslot);
        RsaKey* key = &keys[rsa_cnt.keyslot];

        if (!rsa_cnt.byte_order)
            data = __bswap_32(data);
        
        *(uint32_t*)&key->exp[key->exp_ctr] = data;
        key->exp_ctr += 4;
        if (key->exp_ctr >= 0x100)
            key->exp_ctr = 0;
        return;
    }
    // if (addr >= 0x1000B400 && addr < 0x1000B600)
    // {
    //     RsaKey* key = &keys[rsa_cnt.keyslot];

    //     if (!rsa_cnt.byte_order)
    //         data = __bswap_32(data);
        
    //     *(uint32_t*)&key->exp[key->exp_ctr] = data;
    //     key->exp_ctr += 4;
    //     if (key->exp_ctr >= 0x100)
    //     {
    //         key->exp_ctr = 0;
    //     }
    //     printf("[RSA]: Writing 0x%08x to key%d exp\n", data, rsa_cnt.keyslot);
    //     return;
    // }

    switch (addr)
    {
    case 0x1000b000:
    {
        rsa_cnt.irqen = (data >> 1) & 1;
        rsa_cnt.keyslot = (data >> 4) & 0x3;
        rsa_cnt.byte_order = (data >> 8) & 1;
        rsa_cnt.word_order = (data >> 9) & 1;

        if (data & 1)
            do_rsa_op();

        printf("[RSA]: Write 0x%08x to RSA_CNT\n", data);
        break;
    }
    case 0x1000b100:
    case 0x1000b110:
    case 0x1000b120:
    case 0x1000b130:
    case 0x1000b0f0:
        return;
    default:
        printf("[RSA]: Write to unknown addr 0x%08x\n", addr);
        exit(1);
    }
}