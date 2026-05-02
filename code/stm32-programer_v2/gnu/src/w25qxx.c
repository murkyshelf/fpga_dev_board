#include "w25qxx.h"

#include "board.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

#define W25Q_PORT               GPIOA
#define W25Q_CS                 GPIO4
#define W25Q_SCK                GPIO5
#define W25Q_MISO               GPIO6
#define W25Q_MOSI               GPIO7

#define CMD_WRITE_ENABLE        0x06U
#define CMD_READ_STATUS1        0x05U
#define CMD_PAGE_PROGRAM        0x02U
#define CMD_READ_DATA           0x03U
#define CMD_SECTOR_ERASE        0x20U
#define CMD_JEDEC_ID            0x9FU

#define STATUS_BUSY             0x01U
#define ERASE_TIMEOUT_MS        5000U
#define PROGRAM_TIMEOUT_MS      1000U
#define INIT_RETRY_MS           2000U
#define INIT_RETRY_DELAY_MS     10U

static w25qxx_info_t info;

static void cs_low(void)
{
	gpio_clear(W25Q_PORT, W25Q_CS);
}

static void cs_high(void)
{
	gpio_set(W25Q_PORT, W25Q_CS);
}

static uint8_t spi_transfer(uint8_t value)
{
	return (uint8_t)spi_xfer(SPI1, value);
}

static uint32_t capacity_from_jedec(uint32_t jedec_id)
{
	const uint8_t capacity_power = (uint8_t)(jedec_id & 0xFFU);

	if (capacity_power < 16U || capacity_power > 24U) {
		return 0U;
	}

	return 1UL << capacity_power;
}

static w25qxx_status_t wait_while_busy(uint32_t timeout_ms)
{
	const uint32_t start = millis();

	do {
		cs_low();
		spi_transfer(CMD_READ_STATUS1);
		const uint8_t status = spi_transfer(0xFFU);
		cs_high();

		if ((status & STATUS_BUSY) == 0U) {
			return W25QXX_OK;
		}
	} while ((millis() - start) < timeout_ms);

	return W25QXX_TIMEOUT;
}

static void write_enable(void)
{
	cs_low();
	spi_transfer(CMD_WRITE_ENABLE);
	cs_high();
}

static int range_is_valid(uint32_t address, size_t length)
{
	if (length == 0U) {
		return 1;
	}
	if (info.capacity_bytes == 0U) {
		uint32_t jedec_id = 0;
		if (w25qxx_read_jedec_id(&jedec_id) != W25QXX_OK || info.capacity_bytes == 0U) {
			return 0;
		}
	}
	if (address >= info.capacity_bytes) {
		return 0;
	}
	return length <= (info.capacity_bytes - address);
}

void w25qxx_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_SPI1);
	rcc_periph_clock_enable(RCC_AFIO);

	gpio_set(W25Q_PORT, W25Q_CS);
	gpio_set_mode(W25Q_PORT, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, W25Q_CS);
	gpio_set_mode(W25Q_PORT, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, W25Q_SCK | W25Q_MOSI);
	gpio_set_mode(W25Q_PORT, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_FLOAT, W25Q_MISO);

	rcc_periph_reset_pulse(RST_SPI1);
	spi_init_master(SPI1,
			SPI_CR1_BAUDRATE_FPCLK_DIV_8,
			SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
			SPI_CR1_CPHA_CLK_TRANSITION_1,
			SPI_CR1_DFF_8BIT,
			SPI_CR1_MSBFIRST);
	spi_enable_software_slave_management(SPI1);
	spi_set_nss_high(SPI1);
	spi_enable(SPI1);

	const uint32_t start = millis();
	do {
		uint32_t jedec_id = 0;
		if (w25qxx_read_jedec_id(&jedec_id) == W25QXX_OK && info.capacity_bytes != 0U) {
			break;
		}
		delay_ms(INIT_RETRY_DELAY_MS);
	} while ((millis() - start) < INIT_RETRY_MS);
}

const w25qxx_info_t *w25qxx_info(void)
{
	return &info;
}

w25qxx_status_t w25qxx_read_jedec_id(uint32_t *jedec_id)
{
	if (jedec_id == 0) {
		return W25QXX_BAD_ARGUMENT;
	}

	cs_low();
	spi_transfer(CMD_JEDEC_ID);
	const uint8_t manufacturer = spi_transfer(0xFFU);
	const uint8_t memory_type = spi_transfer(0xFFU);
	const uint8_t capacity = spi_transfer(0xFFU);
	cs_high();

	*jedec_id = ((uint32_t)manufacturer << 16) |
		    ((uint32_t)memory_type << 8) |
		    capacity;
	info.jedec_id = *jedec_id;
	info.capacity_bytes = capacity_from_jedec(*jedec_id);
	return W25QXX_OK;
}

w25qxx_status_t w25qxx_read(uint32_t address, uint8_t *data, size_t length)
{
	if (data == 0 && length > 0U) {
		return W25QXX_BAD_ARGUMENT;
	}
	if (!range_is_valid(address, length)) {
		return W25QXX_BAD_ARGUMENT;
	}

	cs_low();
	spi_transfer(CMD_READ_DATA);
	spi_transfer((uint8_t)(address >> 16));
	spi_transfer((uint8_t)(address >> 8));
	spi_transfer((uint8_t)address);

	for (size_t i = 0; i < length; i++) {
		data[i] = spi_transfer(0xFFU);
	}

	cs_high();
	return W25QXX_OK;
}

w25qxx_status_t w25qxx_write(uint32_t address, const uint8_t *data, size_t length)
{
	if (data == 0 && length > 0U) {
		return W25QXX_BAD_ARGUMENT;
	}
	if (!range_is_valid(address, length)) {
		return W25QXX_BAD_ARGUMENT;
	}

	while (length > 0U) {
		const uint32_t page_offset = address % W25QXX_PAGE_SIZE;
		const uint32_t page_space = W25QXX_PAGE_SIZE - page_offset;
		const size_t chunk = (length < page_space) ? length : page_space;

		w25qxx_status_t status = wait_while_busy(PROGRAM_TIMEOUT_MS);
		if (status != W25QXX_OK) {
			return status;
		}

		write_enable();

		cs_low();
		spi_transfer(CMD_PAGE_PROGRAM);
		spi_transfer((uint8_t)(address >> 16));
		spi_transfer((uint8_t)(address >> 8));
		spi_transfer((uint8_t)address);
		for (size_t i = 0; i < chunk; i++) {
			spi_transfer(data[i]);
		}
		cs_high();

		address += chunk;
		data += chunk;
		length -= chunk;
	}

	return wait_while_busy(PROGRAM_TIMEOUT_MS);
}

w25qxx_status_t w25qxx_erase_sector(uint32_t address)
{
	address &= ~(W25QXX_SECTOR_SIZE - 1U);

	if (!range_is_valid(address, W25QXX_SECTOR_SIZE)) {
		return W25QXX_BAD_ARGUMENT;
	}

	w25qxx_status_t status = wait_while_busy(ERASE_TIMEOUT_MS);
	if (status != W25QXX_OK) {
		return status;
	}

	write_enable();

	cs_low();
	spi_transfer(CMD_SECTOR_ERASE);
	spi_transfer((uint8_t)(address >> 16));
	spi_transfer((uint8_t)(address >> 8));
	spi_transfer((uint8_t)address);
	cs_high();

	return wait_while_busy(ERASE_TIMEOUT_MS);
}

w25qxx_status_t w25qxx_erase_range(uint32_t address, size_t length)
{
	if (length == 0U) {
		return W25QXX_OK;
	}
	if (!range_is_valid(address, length)) {
		return W25QXX_BAD_ARGUMENT;
	}

	const uint32_t start = address & ~(W25QXX_SECTOR_SIZE - 1U);
	const uint32_t end = (uint32_t)((address + length + W25QXX_SECTOR_SIZE - 1U) &
				       ~(W25QXX_SECTOR_SIZE - 1U));

	for (uint32_t sector = start; sector < end; sector += W25QXX_SECTOR_SIZE) {
		w25qxx_status_t status = w25qxx_erase_sector(sector);
		if (status != W25QXX_OK) {
			return status;
		}
	}

	return W25QXX_OK;
}
