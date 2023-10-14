#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cassert>

namespace AES
{

void Reset();
    
void Write32(uint32_t addr, uint32_t data);
uint32_t Read32(uint32_t addr);

uint8_t ReadKEYCNT();
void WriteKEYCNT(uint8_t data);

void WriteKEYSEL(uint8_t data);

void WriteBlockCount(uint16_t data);

}