/*****************************************************************************
 * ST7565 driver – ESP-IDF SPI implementation
 ****************************************************************************/
#include "st7565.h"
#include "freertos/task.h"

/* ───── Internal helpers ────────────────────────────────────────────────── */
static inline void _xfer(st7565_t *d, uint8_t v, bool data)
{
    gpio_set_level(d->rs, data);
    gpio_set_level(d->cs, 0);

    spi_transaction_t t = { .length = 8, .tx_buffer = &v };
    spi_device_transmit(d->spi, &t);

    gpio_set_level(d->cs, 1);
}
#define CMD(d,v)  _xfer((d),(v),false)
#define DAT(d,v)  _xfer((d),(v),true)

/* ───── Low-level public fns (for Python access) ────────────────────────── */
void st7565_write_instruction(st7565_t *d, uint8_t c){ CMD(d,c); }
void st7565_write_data       (st7565_t *d, uint8_t dta){ DAT(d,dta); }

void st7565_set_page_address(st7565_t *d, uint8_t p)   { CMD(d, 0xB0 | (p&0x0F)); }
void st7565_set_column_address(st7565_t *d, uint8_t c){
    CMD(d, 0x10 | (c >> 4));
    CMD(d, 0x00 | (c & 0x0F));
}

/* ───── Framebuffer helpers ─────────────────────────────────────────────── */
void st7565_clear_display(st7565_t *d)
{
    for (int p=0; p<ST7565_PAGES; ++p) {
        st7565_set_page_address(d,p);
        st7565_set_column_address(d,0);
        for (int i=0;i<ST7565_WIDTH;i++) DAT(d,0x00);
    }
}

// void st7565_draw_buffer(st7565_t *d, const uint8_t *buf)
// {
//     for (int p=0; p<ST7565_PAGES; ++p) {
//         st7565_set_page_address(d,p);
//         st7565_set_column_address(d,0);
//         const uint8_t *row = buf + p*ST7565_WIDTH;
//         for (int i=0;i<ST7565_WIDTH;i++) DAT(d,row[i]);
//     }
// }

void st7565_draw_buffer_ex(st7565_t *d, const uint8_t *buf,
                           uint8_t page, uint8_t col,
                           uint8_t width, uint8_t pages)
{
    for (int p = 0; p < pages; ++p) {
        st7565_set_page_address(d, page + p);
        st7565_set_column_address(d, col);
        const uint8_t *row = buf + p * width;
        for (int i = 0; i < width; ++i) {
            DAT(d, row[i]);
        }
    }
}

// Backward-compatible version
void st7565_draw_buffer(st7565_t *d, const uint8_t *buf)
{
    st7565_draw_buffer_ex(d, buf, 0, 0, ST7565_WIDTH, ST7565_PAGES);
}


/* ───── Extras (contrast, invert, power) ───────────────────────────────── */
void st7565_contrast(st7565_t *d, uint8_t lvl){
    CMD(d,0x81); CMD(d,lvl & 0x3F);
}
void st7565_invert(st7565_t *d, bool inv){ CMD(d, inv?0xA7:0xA6); }
void st7565_power_off(st7565_t *d){ CMD(d,0xAE); }
void st7565_power_on (st7565_t *d){ CMD(d,0xAF); }

/* ───── Init sequence ──────────────────────────────────────────────────── */
void st7565_init(st7565_t *d, spi_host_device_t host,
                 gpio_num_t cs,  gpio_num_t rs,  gpio_num_t rst,
                 gpio_num_t sda, gpio_num_t sck)
{
    d->cs = cs; d->rs = rs; d->rst = rst;

    gpio_set_direction(cs ,GPIO_MODE_OUTPUT);
    gpio_set_direction(rs ,GPIO_MODE_OUTPUT);
    gpio_set_direction(rst,GPIO_MODE_OUTPUT);

    spi_bus_config_t bus = {
        .mosi_io_num=sda, .sclk_io_num=sck, .miso_io_num=-1,
        .quadwp_io_num=-1,.quadhd_io_num=-1,
        .max_transfer_sz = ST7565_WIDTH*ST7565_PAGES
    };
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1*1000*1000, .mode=0,
        .spics_io_num=-1, .queue_size=1
    };
    spi_bus_initialize(host,&bus,SPI_DMA_DISABLED);
    spi_bus_add_device(host,&dev,&d->spi);

    gpio_set_level(rst,0); vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst,1); vTaskDelay(pdMS_TO_TICKS(50));

    /* Power-up sequence (matches Python driver) */
    CMD(d,0xAE); CMD(d,0xA2); CMD(d,0xA0); CMD(d,0xC8);
    CMD(d,0xA6); CMD(d,0x2F); CMD(d,0x27);
    CMD(d,0x81); CMD(d,0x02);
    CMD(d,0xAF);
}
