#include "extflash_protocol.h"

#include "usbd_cdc_if.h"
#include "w25qxx.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RX_RING_SIZE            2048U
#define LINE_SIZE               96U
#define WRITE_CHUNK_SIZE        1024U
#define READ_CHUNK_SIZE         256U
#define VERIFY_CHUNK_SIZE       256U
#define USB_TX_TIMEOUT_MS       1000U

typedef enum {
    PROTO_LINE = 0,
    PROTO_WRITE_DATA,
} ProtocolMode;

static uint8_t rx_ring[RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

static ProtocolMode mode;
static char line_buf[LINE_SIZE];
static uint16_t line_len;

static uint32_t write_addr;
static uint32_t write_len;
static uint32_t write_received;
static uint32_t write_crc_expected;
static uint32_t write_crc;
static uint8_t write_buf[WRITE_CHUNK_SIZE];
static uint16_t write_buf_len;
static bool write_failed;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    crc = ~crc;
    while (length-- > 0U) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8U; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return ~crc;
}

static void tx_raw(const uint8_t *data, uint16_t length)
{
    const uint32_t start = HAL_GetTick();

    while (CDC_Transmit_FS((uint8_t *)data, length) == USBD_BUSY) {
        if ((HAL_GetTick() - start) > USB_TX_TIMEOUT_MS) {
            return;
        }
    }
}

static void tx_line(const char *fmt, ...)
{
    char out[160];
    va_list args;

    va_start(args, fmt);
    const int written = vsnprintf(out, sizeof(out), fmt, args);
    va_end(args);

    if (written <= 0) {
        return;
    }

    const uint16_t length = (written >= (int)sizeof(out)) ? (uint16_t)(sizeof(out) - 1U) : (uint16_t)written;
    tx_raw((const uint8_t *)out, length);
}

static bool rx_pop(uint8_t *byte)
{
    if (rx_tail == rx_head) {
        return false;
    }

    *byte = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % RX_RING_SIZE);
    return true;
}

static uint32_t parse_hex_u32(const char *s, bool *ok)
{
    char *end = NULL;
    if (s == NULL || *s == '-') {
        *ok = false;
        return 0;
    }

    const unsigned long value = strtoul(s, &end, 16);
    *ok = (end != s && *end == '\0' && value <= 0xFFFFFFFFUL);
    return (uint32_t)value;
}

static void finish_write(void)
{
    if (write_buf_len > 0U && !write_failed) {
        if (W25QXX_Write(write_addr + write_received - write_buf_len, write_buf, write_buf_len) != W25QXX_OK) {
            write_failed = true;
        }
        write_buf_len = 0U;
    }

    if (write_failed) {
        tx_line("ERR WRITE_FAILED\r\n");
    } else if (write_crc != write_crc_expected) {
        tx_line("ERR CRC expected=%08lX got=%08lX\r\n",
                (unsigned long)write_crc_expected,
                (unsigned long)write_crc);
    } else {
        tx_line("OK WRITE len=%lu crc=%08lX\r\n",
                (unsigned long)write_len,
                (unsigned long)write_crc);
    }

    mode = PROTO_LINE;
}

static void process_write_byte(uint8_t byte)
{
    if (write_received >= write_len) {
        return;
    }

    write_crc = crc32_update(write_crc, &byte, 1);
    write_buf[write_buf_len++] = byte;
    write_received++;

    if (write_buf_len == WRITE_CHUNK_SIZE && !write_failed) {
        if (W25QXX_Write(write_addr + write_received - write_buf_len, write_buf, write_buf_len) != W25QXX_OK) {
            write_failed = true;
        }
        write_buf_len = 0U;
    }

    if (write_received == write_len) {
        finish_write();
    }
}

static void handle_ping(void)
{
    tx_line("OK PONG\r\n");
}

static void handle_readid(void)
{
    uint32_t jedec_id = 0;
    W25QXX_Status status = W25QXX_ReadJedecId(&jedec_id);

    if (status != W25QXX_OK) {
        tx_line("ERR READID status=%d\r\n", status);
        return;
    }

    W25QXX_Device *dev = W25QXX_GetDevice();
    tx_line("OK JEDEC=%06lX SIZE=%lu\r\n",
            (unsigned long)jedec_id,
            (unsigned long)dev->capacity_bytes);
}

static void handle_erase(char *args)
{
    if (args == NULL) {
        tx_line("ERR BAD_ERASE_ARGS\r\n");
        return;
    }

    char *addr_s = strtok(args, " ");
    char *len_s = strtok(NULL, " ");
    bool ok_addr = false;
    bool ok_len = false;
    const uint32_t addr = parse_hex_u32(addr_s ? addr_s : "", &ok_addr);
    const uint32_t len = parse_hex_u32(len_s ? len_s : "", &ok_len);

    if (!ok_addr || !ok_len || len == 0U) {
        tx_line("ERR BAD_ERASE_ARGS\r\n");
        return;
    }

    W25QXX_Status status = W25QXX_EraseRange(addr, len);
    if (status != W25QXX_OK) {
        tx_line("ERR ERASE status=%d\r\n", status);
        return;
    }

    tx_line("OK ERASE addr=%06lX len=%lu\r\n", (unsigned long)addr, (unsigned long)len);
}

static void handle_verify(char *args)
{
    if (args == NULL) {
        tx_line("ERR BAD_VERIFY_ARGS\r\n");
        return;
    }

    char *addr_s = strtok(args, " ");
    char *len_s = strtok(NULL, " ");
    char *crc_s = strtok(NULL, " ");
    bool ok_addr = false;
    bool ok_len = false;
    bool ok_crc = false;
    uint8_t buf[VERIFY_CHUNK_SIZE];

    uint32_t addr = parse_hex_u32(addr_s ? addr_s : "", &ok_addr);
    uint32_t remaining = parse_hex_u32(len_s ? len_s : "", &ok_len);
    const uint32_t expected_crc = parse_hex_u32(crc_s ? crc_s : "", &ok_crc);

    if (!ok_addr || !ok_len || !ok_crc) {
        tx_line("ERR BAD_VERIFY_ARGS\r\n");
        return;
    }

    uint32_t crc = 0;
    while (remaining > 0U) {
        const uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        W25QXX_Status status = W25QXX_Read(addr, buf, chunk);
        if (status != W25QXX_OK) {
            tx_line("ERR VERIFY_READ status=%d\r\n", status);
            return;
        }

        crc = crc32_update(crc, buf, chunk);
        addr += chunk;
        remaining -= chunk;
    }

    if (crc != expected_crc) {
        tx_line("ERR VERIFY_CRC expected=%08lX got=%08lX\r\n",
                (unsigned long)expected_crc,
                (unsigned long)crc);
        return;
    }

    tx_line("OK VERIFY crc=%08lX\r\n", (unsigned long)crc);
}

static void handle_read(char *args)
{
    if (args == NULL) {
        tx_line("ERR BAD_READ_ARGS\r\n");
        return;
    }

    char *addr_s = strtok(args, " ");
    char *len_s = strtok(NULL, " ");
    bool ok_addr = false;
    bool ok_len = false;
    uint8_t buf[READ_CHUNK_SIZE];

    uint32_t addr = parse_hex_u32(addr_s ? addr_s : "", &ok_addr);
    uint32_t remaining = parse_hex_u32(len_s ? len_s : "", &ok_len);
    const uint32_t total_len = remaining;

    if (!ok_addr || !ok_len || remaining == 0U) {
        tx_line("ERR BAD_READ_ARGS\r\n");
        return;
    }

    uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
    W25QXX_Status status = W25QXX_Read(addr, buf, chunk);
    if (status != W25QXX_OK) {
        tx_line("ERR READ status=%d\r\n", status);
        return;
    }

    tx_line("OK READ len=%lu\r\n", (unsigned long)total_len);

    uint32_t crc = 0;
    while (remaining > 0U) {
        crc = crc32_update(crc, buf, chunk);
        tx_raw(buf, (uint16_t)chunk);

        addr += chunk;
        remaining -= chunk;
        if (remaining == 0U) {
            break;
        }

        chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        status = W25QXX_Read(addr, buf, chunk);
        if (status != W25QXX_OK) {
            return;
        }
    }

    tx_line("OK READ crc=%08lX\r\n", (unsigned long)crc);
}

static void handle_write(char *args)
{
    if (args == NULL) {
        tx_line("ERR BAD_WRITE_ARGS\r\n");
        return;
    }

    char *addr_s = strtok(args, " ");
    char *len_s = strtok(NULL, " ");
    char *crc_s = strtok(NULL, " ");
    bool ok_addr = false;
    bool ok_len = false;
    bool ok_crc = false;

    write_addr = parse_hex_u32(addr_s ? addr_s : "", &ok_addr);
    write_len = parse_hex_u32(len_s ? len_s : "", &ok_len);
    write_crc_expected = parse_hex_u32(crc_s ? crc_s : "", &ok_crc);

    if (!ok_addr || !ok_len || !ok_crc || write_len == 0U) {
        tx_line("ERR BAD_WRITE_ARGS\r\n");
        return;
    }

    write_received = 0;
    write_crc = 0;
    write_buf_len = 0;
    write_failed = false;
    mode = PROTO_WRITE_DATA;
    tx_line("OK SEND\r\n");
}

static void process_line(char *line)
{
    while (*line == ' ') {
        line++;
    }

    char *cmd = strtok(line, " ");
    char *args = strtok(NULL, "");

    if (cmd == NULL) {
        return;
    }

    if (strcmp(cmd, "PING") == 0) {
        handle_ping();
    } else if (strcmp(cmd, "READID") == 0) {
        handle_readid();
    } else if (strcmp(cmd, "ERASE") == 0) {
        handle_erase(args);
    } else if (strcmp(cmd, "WRITE") == 0) {
        handle_write(args);
    } else if (strcmp(cmd, "READ") == 0) {
        handle_read(args);
    } else if (strcmp(cmd, "VERIFY") == 0) {
        handle_verify(args);
    } else {
        tx_line("ERR UNKNOWN_CMD\r\n");
    }
}

static void process_line_byte(uint8_t byte)
{
    if (byte == '\r') {
        return;
    }

    if (byte == '\n') {
        line_buf[line_len] = '\0';
        process_line(line_buf);
        line_len = 0;
        return;
    }

    if (line_len >= (LINE_SIZE - 1U)) {
        line_len = 0;
        tx_line("ERR LINE_TOO_LONG\r\n");
        return;
    }

    line_buf[line_len++] = (char)byte;
}

void ExtFlashProtocol_Init(void)
{
    rx_head = 0;
    rx_tail = 0;
    mode = PROTO_LINE;
    line_len = 0;
}

void ExtFlashProtocol_Rx(const uint8_t *data, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {
        const uint16_t next = (uint16_t)((rx_head + 1U) % RX_RING_SIZE);
        if (next == rx_tail) {
            break;
        }
        rx_ring[rx_head] = data[i];
        rx_head = next;
    }
}

void ExtFlashProtocol_Task(void)
{
    uint8_t byte;

    while (rx_pop(&byte)) {
        if (mode == PROTO_WRITE_DATA) {
            process_write_byte(byte);
        } else {
            process_line_byte(byte);
        }
    }
}
