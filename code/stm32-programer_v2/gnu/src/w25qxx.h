#ifndef W25QXX_H
#define W25QXX_H

#include <stddef.h>
#include <stdint.h>

#define W25QXX_PAGE_SIZE    256U
#define W25QXX_SECTOR_SIZE  4096U

typedef enum {
	W25QXX_OK = 0,
	W25QXX_ERROR,
	W25QXX_TIMEOUT,
	W25QXX_BAD_ARGUMENT,
} w25qxx_status_t;

typedef struct {
	uint32_t jedec_id;
	uint32_t capacity_bytes;
} w25qxx_info_t;

void w25qxx_init(void);
const w25qxx_info_t *w25qxx_info(void);
w25qxx_status_t w25qxx_read_jedec_id(uint32_t *jedec_id);
w25qxx_status_t w25qxx_read(uint32_t address, uint8_t *data, size_t length);
w25qxx_status_t w25qxx_write(uint32_t address, const uint8_t *data, size_t length);
w25qxx_status_t w25qxx_erase_sector(uint32_t address);
w25qxx_status_t w25qxx_erase_range(uint32_t address, size_t length);

#endif

