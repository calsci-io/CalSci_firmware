#ifndef ST7565_H
#define ST7565_H
/*****************************************************************************
 * ST7565 128×64 monochrome LCD – C driver (ESP-IDF / MicroPython user-module)
 ****************************************************************************/
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <stdbool.h>

#define ST7565_WIDTH   128
#define ST7565_HEIGHT   64
#define ST7565_PAGES   (ST7565_HEIGHT / 8)   /* 8 pages */

typedef struct {
    spi_device_handle_t spi;
    gpio_num_t cs, rs, rst;
} st7565_t;

/* Driver API – matches Python driver ***************************************/
void st7565_init          (st7565_t *dev, spi_host_device_t host,
                           gpio_num_t cs,  gpio_num_t rs,  gpio_num_t rst,
                           gpio_num_t sda, gpio_num_t sck);
void st7565_clear_display (st7565_t *dev);
void st7565_draw_buffer   (st7565_t *dev, const uint8_t *buf);
void st7565_draw_buffer_ex(st7565_t *dev, const uint8_t *buf,
                           uint8_t page, uint8_t column,
                           uint8_t width, uint8_t pages);


void st7565_contrast      (st7565_t *dev, uint8_t level);   /* 0-63 */
void st7565_invert        (st7565_t *dev, bool invert);
void st7565_power_on      (st7565_t *dev);
void st7565_power_off     (st7565_t *dev);

/* Low-level helpers now also exposed to Python ******************************/
void st7565_set_page_address   (st7565_t *dev, uint8_t page);   /* 0-7   */
void st7565_set_column_address (st7565_t *dev, uint8_t column); /* 0-127 */
void st7565_write_instruction  (st7565_t *dev, uint8_t cmd);
void st7565_write_data         (st7565_t *dev, uint8_t data);

#endif /* ST7565_H */
