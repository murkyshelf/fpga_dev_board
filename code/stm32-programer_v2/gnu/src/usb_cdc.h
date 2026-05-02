#ifndef USB_CDC_H
#define USB_CDC_H

#include <stddef.h>
#include <stdint.h>

void usb_cdc_init(void);
void usb_cdc_poll(void);
int usb_cdc_write(const uint8_t *data, size_t length);

#endif

