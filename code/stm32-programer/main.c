#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>

// PINOUT (STM32F103 BluePill)
// PA4  - SPI1 NSS (Chip Select for SPI Flash)
// PA5  - SPI1 SCK
// PA6  - SPI1 MISO
// PA7  - SPI1 MOSI
// PB0  - FPGA CRESET_B (Output, active low)
// PB1  - FPGA CDONE (Input)
// PA9  - USART1 TX
// PA10 - USART1 RX

#define SPI_FLASH SPI1
#define PORT_CS GPIOA
#define PIN_CS GPIO4

#define PORT_FPGA GPIOB
#define PIN_CRESET GPIO0

static void clock_setup(void) {
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_SPI1);
	rcc_periph_clock_enable(RCC_USART1);
}

static void usart_setup(void) {
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO9);
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO10);

	usart_set_baudrate(USART1, 115200);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
	usart_enable(USART1);
}

static void spi_setup(void) {
	// Setup SCK, MOSI
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO5 | GPIO7);
	// Setup MISO
	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO6);

	// Setup CS initially High
	gpio_set_mode(PORT_CS, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, PIN_CS);
	gpio_set(PORT_CS, PIN_CS);

	spi_init_master(SPI_FLASH, SPI_CR1_BAUDRATE_FPCLK_DIV_4, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);
	spi_enable_software_slave_management(SPI_FLASH);
	spi_set_nss_high(SPI_FLASH);
	spi_enable(SPI_FLASH);
}

static void fpga_setup(void) {
	// CDONE input
	gpio_set_mode(PORT_FPGA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO1);

	// CRESET_B output logic low initially (hold reset)
	gpio_set_mode(PORT_FPGA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, PIN_CRESET);
	gpio_clear(PORT_FPGA, PIN_CRESET);
}

static uint8_t spi_transfer(uint8_t data) {
	spi_send(SPI_FLASH, data);
	return spi_read(SPI_FLASH);
}

static void send_uart(uint8_t dat) {
	usart_send_blocking(USART1, dat);
}

static uint8_t recv_uart(void) {
	return usart_recv_blocking(USART1);
}

int main(void) {
	clock_setup();
	fpga_setup();
	spi_setup();
	usart_setup();

    // Release reset to let FPGA boot if flash is already programmed.
    // Programmer will pull it low when starting programming.
	gpio_set(PORT_FPGA, PIN_CRESET);

	while (1) {
		uint8_t cmd = recv_uart();
		
		switch (cmd) {
			case 0x01: // Identify
				send_uart(0x42); 
				break;
			case 0x10: // Set CRESET_B Low
				gpio_clear(PORT_FPGA, PIN_CRESET);
				send_uart(0x06); // ACK
				break;
			case 0x11: // Set CRESET_B High
				gpio_set(PORT_FPGA, PIN_CRESET);
				send_uart(0x06); // ACK
				break;
			case 0x20: // SPI Flash Write Enable (0x06)
				gpio_clear(PORT_CS, PIN_CS);
				spi_transfer(0x06);
				gpio_set(PORT_CS, PIN_CS);
				send_uart(0x06); // ACK
				break;
			case 0x21: // Transfer byte directly
			{
				uint8_t byte_out = recv_uart();
				uint8_t byte_in = spi_transfer(byte_out);
				send_uart(byte_in);
				break;
			}
			case 0x22: // Transfer block (CS handled by Python script)
			{
				uint16_t len = recv_uart();
				len |= ((uint16_t)recv_uart()) << 8;
				for (uint16_t i=0; i<len; i++) {
					uint8_t byte_out = recv_uart();
					uint8_t byte_in = spi_transfer(byte_out);
					send_uart(byte_in);
				}
				break;
			}
			case 0x23: // Set CS Low
				gpio_clear(PORT_CS, PIN_CS);
				send_uart(0x06);
				break;
			case 0x24: // Set CS High
				gpio_set(PORT_CS, PIN_CS);
				send_uart(0x06);
				break;
			default:
				send_uart(0x15); // NAK
				break;
		}
	}
	return 0;
}
