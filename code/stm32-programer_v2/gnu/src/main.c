#include "board.h"
#include "protocol.h"
#include "usb_cdc.h"
#include "w25qxx.h"

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>

static volatile uint32_t system_millis;

void sys_tick_handler(void)
{
	system_millis++;
}

uint32_t millis(void)
{
	return system_millis;
}

void delay_ms(uint32_t ms)
{
	const uint32_t start = millis();
	while ((millis() - start) < ms) {
		__asm__("nop");
	}
}

static void clock_setup(void)
{
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(72000U - 1U);
	systick_clear();
	systick_interrupt_enable();
	systick_counter_enable();
}

int main(void)
{
	clock_setup();
	w25qxx_init();
	protocol_init();
	usb_cdc_init();

	while (1) {
		usb_cdc_poll();
		protocol_task();
	}
}

