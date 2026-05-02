#ifndef EXTFLASH_PROTOCOL_H
#define EXTFLASH_PROTOCOL_H

#include <stdint.h>

void ExtFlashProtocol_Init(void);
void ExtFlashProtocol_Rx(const uint8_t *data, uint32_t length);
void ExtFlashProtocol_Task(void);

#endif
