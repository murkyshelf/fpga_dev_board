#ifndef W25QXX_H
#define W25QXX_H

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define W25QXX_PAGE_SIZE        256U
#define W25QXX_SECTOR_SIZE      4096U
#define W25QXX_BLOCK32_SIZE     32768U
#define W25QXX_BLOCK64_SIZE     65536U

typedef enum {
    W25QXX_OK = 0,
    W25QXX_ERROR,
    W25QXX_TIMEOUT,
    W25QXX_BAD_ARGUMENT,
} W25QXX_Status;

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    uint32_t jedec_id;
    uint32_t capacity_bytes;
} W25QXX_Device;

W25QXX_Status W25QXX_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
W25QXX_Device *W25QXX_GetDevice(void);

W25QXX_Status W25QXX_ReadJedecId(uint32_t *jedec_id);
W25QXX_Status W25QXX_Read(uint32_t address, uint8_t *data, size_t length);
W25QXX_Status W25QXX_Write(uint32_t address, const uint8_t *data, size_t length);
W25QXX_Status W25QXX_EraseSector(uint32_t address);
W25QXX_Status W25QXX_EraseRange(uint32_t address, size_t length);
W25QXX_Status W25QXX_WaitWhileBusy(uint32_t timeout_ms);

#endif
