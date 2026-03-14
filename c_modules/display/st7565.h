#ifndef ST7565_H
#define ST7565_H
/*****************************************************************************
 * ST7565 128x64 monochrome LCD – complete C driver
 *
 * All ST7565R datasheet commands implemented.
 * Features: bulk SPI, DMA, 8 MHz clock, error-checked init/deinit.
 ****************************************************************************/
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define ST7565_WIDTH   128
#define ST7565_HEIGHT   64
#define ST7565_PAGES   (ST7565_HEIGHT / 8)   /* 8 pages */

/* ───── ST7565R command bytes (from datasheet) ────────────────────────────── */
#define ST7565_CMD_DISPLAY_OFF          0xAE
#define ST7565_CMD_DISPLAY_ON           0xAF
#define ST7565_CMD_START_LINE           0x40  /* | line (0-63) */
#define ST7565_CMD_PAGE_ADDRESS         0xB0  /* | page (0-7)  */
#define ST7565_CMD_COLUMN_MSB           0x10  /* | col>>4       */
#define ST7565_CMD_COLUMN_LSB           0x00  /* | col&0x0F     */
#define ST7565_CMD_ADC_NORMAL           0xA0
#define ST7565_CMD_ADC_REVERSE          0xA1
#define ST7565_CMD_DISPLAY_NORMAL       0xA6
#define ST7565_CMD_DISPLAY_REVERSE      0xA7
#define ST7565_CMD_ALL_POINTS_OFF       0xA4
#define ST7565_CMD_ALL_POINTS_ON        0xA5
#define ST7565_CMD_BIAS_1_9             0xA2
#define ST7565_CMD_BIAS_1_7             0xA3
#define ST7565_CMD_COM_NORMAL           0xC0
#define ST7565_CMD_COM_REVERSE          0xC8
#define ST7565_CMD_POWER_CTRL           0x28  /* | flags (0-7) */
#define ST7565_CMD_V0_RATIO             0x20  /* | ratio (0-7) */
#define ST7565_CMD_ELECTRONIC_VOL       0x81  /* next byte = value */
#define ST7565_CMD_SLEEP_MODE           0xAC
#define ST7565_CMD_NORMAL_MODE          0xAD
#define ST7565_CMD_RESET                0xE2
#define ST7565_CMD_NOP                  0xE3
#define ST7565_CMD_READ_MODIFY_WRITE    0xE0
#define ST7565_CMD_END                  0xEE
#define ST7565_CMD_BOOSTER_RATIO_SET    0xF8
#define ST7565_CMD_BOOSTER_2X_3X_4X    0x00
#define ST7565_CMD_BOOSTER_5X           0x01
#define ST7565_CMD_BOOSTER_6X           0x03

typedef struct {
    spi_device_handle_t spi;
    spi_host_device_t   host;
    gpio_num_t          cs, rs, rst;
    bool                initialized;
} st7565_t;

typedef struct {
    uint8_t contrast;     /* 0-63 */
    uint8_t v0_ratio;     /* 0-7  */
    uint8_t power_ctrl;   /* 0-7: booster|reg|follower */
    uint8_t booster;      /* 0=2x-4x, 1=5x, 3=6x */
    uint8_t start_line;   /* 0-63 */
    bool    bias_1_7;     /* false=1/9, true=1/7 */
    bool    adc_reverse;  /* horizontal flip */
    bool    com_reverse;  /* vertical flip */
} st7565_init_profile_t;

extern const st7565_init_profile_t ST7565_INIT_PROFILE_DEFAULT;

/* ───── Init / deinit ─────────────────────────────────────────────────────── */
esp_err_t st7565_init_ex(st7565_t *dev, spi_host_device_t host,
                         gpio_num_t cs,  gpio_num_t rs,  gpio_num_t rst,
                         gpio_num_t sda, gpio_num_t sck,
                         const st7565_init_profile_t *profile);
esp_err_t st7565_init  (st7565_t *dev, spi_host_device_t host,
                        gpio_num_t cs,  gpio_num_t rs,  gpio_num_t rst,
                        gpio_num_t sda, gpio_num_t sck);
esp_err_t st7565_deinit(st7565_t *dev);
void      st7565_reset (st7565_t *dev);

/* ───── Framebuffer operations ────────────────────────────────────────────── */
void st7565_clear_display (st7565_t *dev);
void st7565_draw_buffer   (st7565_t *dev, const uint8_t *buf);
void st7565_draw_buffer_ex(st7565_t *dev, const uint8_t *buf,
                           uint8_t page, uint8_t column,
                           uint8_t width, uint8_t pages);

/* ───── Display control ───────────────────────────────────────────────────── */
void st7565_contrast       (st7565_t *dev, uint8_t level);    /* 0-63 */
void st7565_invert         (st7565_t *dev, bool invert);
void st7565_power_on       (st7565_t *dev);
void st7565_power_off      (st7565_t *dev);
void st7565_set_start_line (st7565_t *dev, uint8_t line);     /* 0-63: hw vertical scroll */
void st7565_all_points_on  (st7565_t *dev, bool on);          /* true=all on, false=normal */
void st7565_sleep          (st7565_t *dev);
void st7565_wake           (st7565_t *dev);

/* ───── Orientation ───────────────────────────────────────────────────────── */
void st7565_set_adc        (st7565_t *dev, bool reverse);     /* horizontal flip */
void st7565_set_com_dir    (st7565_t *dev, bool reverse);     /* vertical flip */

/* ───── Booster / bias / resistor ratio ───────────────────────────────────── */
void st7565_set_bias       (st7565_t *dev, bool bias_1_7);    /* false=1/9, true=1/7 */
void st7565_set_v0_ratio   (st7565_t *dev, uint8_t ratio);    /* 0-7 */
void st7565_set_booster    (st7565_t *dev, uint8_t ratio);    /* 0=2x-4x, 1=5x, 3=6x */
void st7565_set_power_ctrl (st7565_t *dev, uint8_t flags);    /* 0-7: booster|reg|follower */

/* ───── Read-Modify-Write ─────────────────────────────────────────────────── */
void st7565_rmw_start      (st7565_t *dev);
void st7565_rmw_end        (st7565_t *dev);

/* ───── Low-level single-byte access ──────────────────────────────────────── */
void st7565_set_page_address   (st7565_t *dev, uint8_t page);   /* 0-7   */
void st7565_set_column_address (st7565_t *dev, uint8_t column); /* 0-127 */
void st7565_write_instruction  (st7565_t *dev, uint8_t cmd);
void st7565_write_data         (st7565_t *dev, uint8_t data);
void st7565_nop                (st7565_t *dev);

#endif /* ST7565_H */
