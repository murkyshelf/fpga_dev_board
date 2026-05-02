#include "w25qxx.h"

#include <string.h>

#define CMD_WRITE_ENABLE        0x06U
#define CMD_READ_STATUS1        0x05U
#define CMD_PAGE_PROGRAM        0x02U
#define CMD_READ_DATA           0x03U
#define CMD_SECTOR_ERASE        0x20U
#define CMD_JEDEC_ID            0x9FU

#define STATUS_BUSY             0x01U

#define SPI_TIMEOUT_MS          1000U
#define ERASE_TIMEOUT_MS        5000U
#define PROGRAM_TIMEOUT_MS      1000U
#define INIT_RETRY_MS           2000U
#define INIT_RETRY_DELAY_MS     10U

static W25QXX_Device g_w25q;

static void cs_low(void)
{
    HAL_GPIO_WritePin(g_w25q.cs_port, g_w25q.cs_pin, GPIO_PIN_RESET);
}

static void cs_high(void)
{
    HAL_GPIO_WritePin(g_w25q.cs_port, g_w25q.cs_pin, GPIO_PIN_SET);
}

static W25QXX_Status spi_tx(const uint8_t *data, uint16_t length)
{
    return (HAL_SPI_Transmit(g_w25q.hspi, (uint8_t *)data, length, SPI_TIMEOUT_MS) == HAL_OK)
               ? W25QXX_OK
               : W25QXX_ERROR;
}

static W25QXX_Status spi_rx(uint8_t *data, uint16_t length)
{
    return (HAL_SPI_Receive(g_w25q.hspi, data, length, SPI_TIMEOUT_MS) == HAL_OK)
               ? W25QXX_OK
               : W25QXX_ERROR;
}

static W25QXX_Status write_enable(void)
{
    const uint8_t cmd = CMD_WRITE_ENABLE;

    cs_low();
    W25QXX_Status status = spi_tx(&cmd, 1);
    cs_high();

    return status;
}

static uint32_t capacity_from_jedec(uint32_t jedec_id)
{
    const uint8_t capacity_power = (uint8_t)(jedec_id & 0xFFU);

    if (capacity_power < 16U || capacity_power > 24U) {
        return 0U;
    }

    return 1UL << capacity_power;
}

W25QXX_Status W25QXX_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
    if (hspi == NULL || cs_port == NULL) {
        return W25QXX_BAD_ARGUMENT;
    }

    memset(&g_w25q, 0, sizeof(g_w25q));
    g_w25q.hspi = hspi;
    g_w25q.cs_port = cs_port;
    g_w25q.cs_pin = cs_pin;
    cs_high();

    W25QXX_Status status = W25QXX_ERROR;
    const uint32_t start = HAL_GetTick();
    do {
        uint32_t jedec_id = 0;
        status = W25QXX_ReadJedecId(&jedec_id);
        if (status == W25QXX_OK && g_w25q.capacity_bytes != 0U) {
            return W25QXX_OK;
        }
        HAL_Delay(INIT_RETRY_DELAY_MS);
    } while ((HAL_GetTick() - start) < INIT_RETRY_MS);

    return (status != W25QXX_OK) ? status : W25QXX_ERROR;
}

W25QXX_Device *W25QXX_GetDevice(void)
{
    return &g_w25q;
}

W25QXX_Status W25QXX_ReadJedecId(uint32_t *jedec_id)
{
    if (jedec_id == NULL) {
        return W25QXX_BAD_ARGUMENT;
    }

    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t id[3] = {0};

    cs_low();
    W25QXX_Status status = spi_tx(&cmd, 1);
    if (status == W25QXX_OK) {
        status = spi_rx(id, sizeof(id));
    }
    cs_high();

    if (status != W25QXX_OK) {
        return status;
    }

    *jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    g_w25q.jedec_id = *jedec_id;
    g_w25q.capacity_bytes = capacity_from_jedec(*jedec_id);
    return W25QXX_OK;
}

W25QXX_Status W25QXX_WaitWhileBusy(uint32_t timeout_ms)
{
    const uint32_t start = HAL_GetTick();
    const uint8_t cmd = CMD_READ_STATUS1;
    uint8_t status_reg = 0xFFU;

    do {
        cs_low();
        W25QXX_Status status = spi_tx(&cmd, 1);
        if (status == W25QXX_OK) {
            status = spi_rx(&status_reg, 1);
        }
        cs_high();

        if (status != W25QXX_OK) {
            return status;
        }

        if ((status_reg & STATUS_BUSY) == 0U) {
            return W25QXX_OK;
        }
    } while ((HAL_GetTick() - start) < timeout_ms);

    return W25QXX_TIMEOUT;
}

W25QXX_Status W25QXX_Read(uint32_t address, uint8_t *data, size_t length)
{
    if (data == NULL && length > 0U) {
        return W25QXX_BAD_ARGUMENT;
    }
    if (length == 0U) {
        return W25QXX_OK;
    }
    if (g_w25q.capacity_bytes == 0U) {
        uint32_t jedec_id = 0;
        if (W25QXX_ReadJedecId(&jedec_id) != W25QXX_OK || g_w25q.capacity_bytes == 0U) {
            return W25QXX_BAD_ARGUMENT;
        }
    }
    if (g_w25q.capacity_bytes != 0U &&
        (address >= g_w25q.capacity_bytes || length > (g_w25q.capacity_bytes - address))) {
        return W25QXX_BAD_ARGUMENT;
    }

    uint8_t cmd[4] = {
        CMD_READ_DATA,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address,
    };

    cs_low();
    W25QXX_Status status = spi_tx(cmd, sizeof(cmd));
    while (status == W25QXX_OK && length > 0U) {
        const uint16_t chunk = (length > 65535U) ? 65535U : (uint16_t)length;
        status = spi_rx(data, chunk);
        data += chunk;
        length -= chunk;
    }
    cs_high();

    return status;
}

W25QXX_Status W25QXX_Write(uint32_t address, const uint8_t *data, size_t length)
{
    if (data == NULL && length > 0U) {
        return W25QXX_BAD_ARGUMENT;
    }
    if (length == 0U) {
        return W25QXX_OK;
    }
    if (g_w25q.capacity_bytes == 0U) {
        uint32_t jedec_id = 0;
        if (W25QXX_ReadJedecId(&jedec_id) != W25QXX_OK || g_w25q.capacity_bytes == 0U) {
            return W25QXX_BAD_ARGUMENT;
        }
    }
    if (g_w25q.capacity_bytes != 0U &&
        (address >= g_w25q.capacity_bytes || length > (g_w25q.capacity_bytes - address))) {
        return W25QXX_BAD_ARGUMENT;
    }

    while (length > 0U) {
        const uint32_t page_offset = address % W25QXX_PAGE_SIZE;
        const uint32_t page_space = W25QXX_PAGE_SIZE - page_offset;
        const uint16_t chunk = (length < page_space) ? (uint16_t)length : (uint16_t)page_space;

        W25QXX_Status status = W25QXX_WaitWhileBusy(PROGRAM_TIMEOUT_MS);
        if (status != W25QXX_OK) {
            return status;
        }

        status = write_enable();
        if (status != W25QXX_OK) {
            return status;
        }

        uint8_t cmd[4] = {
            CMD_PAGE_PROGRAM,
            (uint8_t)(address >> 16),
            (uint8_t)(address >> 8),
            (uint8_t)address,
        };

        cs_low();
        status = spi_tx(cmd, sizeof(cmd));
        if (status == W25QXX_OK) {
            status = spi_tx(data, chunk);
        }
        cs_high();

        if (status != W25QXX_OK) {
            return status;
        }

        address += chunk;
        data += chunk;
        length -= chunk;
    }

    return W25QXX_WaitWhileBusy(PROGRAM_TIMEOUT_MS);
}

W25QXX_Status W25QXX_EraseSector(uint32_t address)
{
    address &= ~(W25QXX_SECTOR_SIZE - 1U);

    if (g_w25q.capacity_bytes == 0U) {
        uint32_t jedec_id = 0;
        if (W25QXX_ReadJedecId(&jedec_id) != W25QXX_OK || g_w25q.capacity_bytes == 0U) {
            return W25QXX_BAD_ARGUMENT;
        }
    }
    if (g_w25q.capacity_bytes != 0U && address >= g_w25q.capacity_bytes) {
        return W25QXX_BAD_ARGUMENT;
    }

    W25QXX_Status status = W25QXX_WaitWhileBusy(ERASE_TIMEOUT_MS);
    if (status != W25QXX_OK) {
        return status;
    }

    status = write_enable();
    if (status != W25QXX_OK) {
        return status;
    }

    uint8_t cmd[4] = {
        CMD_SECTOR_ERASE,
        (uint8_t)(address >> 16),
        (uint8_t)(address >> 8),
        (uint8_t)address,
    };

    cs_low();
    status = spi_tx(cmd, sizeof(cmd));
    cs_high();

    if (status != W25QXX_OK) {
        return status;
    }

    return W25QXX_WaitWhileBusy(ERASE_TIMEOUT_MS);
}

W25QXX_Status W25QXX_EraseRange(uint32_t address, size_t length)
{
    if (length == 0U) {
        return W25QXX_OK;
    }

    const uint32_t start = address & ~(W25QXX_SECTOR_SIZE - 1U);
    if (g_w25q.capacity_bytes == 0U) {
        uint32_t jedec_id = 0;
        if (W25QXX_ReadJedecId(&jedec_id) != W25QXX_OK || g_w25q.capacity_bytes == 0U) {
            return W25QXX_BAD_ARGUMENT;
        }
    }
    if (g_w25q.capacity_bytes != 0U &&
        (address >= g_w25q.capacity_bytes || length > (g_w25q.capacity_bytes - address))) {
        return W25QXX_BAD_ARGUMENT;
    }

    const uint32_t end = (uint32_t)((address + length + W25QXX_SECTOR_SIZE - 1U) &
                                   ~(W25QXX_SECTOR_SIZE - 1U));

    if (g_w25q.capacity_bytes != 0U && end > g_w25q.capacity_bytes) {
        return W25QXX_BAD_ARGUMENT;
    }

    for (uint32_t sector = start; sector < end; sector += W25QXX_SECTOR_SIZE) {
        W25QXX_Status status = W25QXX_EraseSector(sector);
        if (status != W25QXX_OK) {
            return status;
        }
    }

    return W25QXX_OK;
}
