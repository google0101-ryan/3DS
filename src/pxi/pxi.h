#pragma once

#include <stdint.h>

namespace PXI
{

void WriteSync11(uint32_t data);
uint32_t ReadSync11();
void WriteCnt11(uint16_t data);
uint16_t ReadCnt11();

}