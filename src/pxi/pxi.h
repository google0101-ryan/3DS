#pragma once

#include <stdint.h>

namespace PXI
{

void WriteSync11(uint32_t data);
uint32_t ReadSync11();
void WriteCnt11(uint16_t data);
uint16_t ReadCnt11();
uint32_t ReadRecv11();

void WriteSync9(uint32_t data);
uint32_t ReadSync9();
void WriteCnt9(uint16_t data);
uint16_t ReadCnt9();
void WriteSend9(uint32_t data);

}