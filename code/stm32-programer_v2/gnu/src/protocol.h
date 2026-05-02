#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

void protocol_init(void);
void protocol_rx(const uint8_t *data, uint16_t length);
void protocol_task(void);

#endif

