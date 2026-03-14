/*****************************************************************************
 * ST7565 128x64 LCD driver – complete ESP-IDF SPI implementation
 *
 * All ST7565R datasheet commands implemented.
 * Optimized: bulk SPI transfers per page, DMA, 8 MHz clock,
 * error checking on init, proper deinit support.
 ****************************************************************************/
#include "st7565.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "st7565";

const st7565_init_profile_t ST7565_INIT_PROFILE_DEFAULT = {
    .contrast = 0x02,
    .v0_ratio = 0x07,
    .power_ctrl = 0x07,
    .booster = ST7565_CMD_BOOSTER_2X_3X_4X,
    .start_line = 0,
    .bias_1_7 = false,
    .adc_reverse = false,
    .com_reverse = true,
};

/* ───── Internal: single-byte command ─────────────────────────────────────── */
static inline esp_err_t _cmd(st7565_t *d, uint8_t v)
{
    if (d == NULL || d->spi == NULL) {
        ESP_LOGE(TAG, "invalid device handle");
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(d->rs, 0);
    gpio_set_level(d->cs, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &v };
    esp_err_t ret = spi_device_transmit(d->spi, &t);
    gpio_set_level(d->cs, 1);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit(cmd) failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/* ───── Internal: bulk data transfer (entire page in one SPI transaction) ── */
static esp_err_t _send_data(st7565_t *d, const uint8_t *buf, size_t len)
{
    if (d == NULL || d->spi == NULL || buf == NULL || len == 0) {
        ESP_LOGE(TAG, "invalid data transfer args");
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(d->rs, 1);
    gpio_set_level(d->cs, 0);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = buf,
    };
    esp_err_t ret = spi_device_transmit(d->spi, &t);
    gpio_set_level(d->cs, 1);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit(data) failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/* ───── Low-level single-byte access ──────────────────────────────────────── */
void st7565_write_instruction(st7565_t *d, uint8_t c) { (void)_cmd(d, c); }

void st7565_write_data(st7565_t *d, uint8_t v) { (void)_send_data(d, &v, 1); }

void st7565_nop(st7565_t *d) { (void)_cmd(d, ST7565_CMD_NOP); }

void st7565_set_page_address(st7565_t *d, uint8_t p)
{
    if (p >= ST7565_PAGES) {
        ESP_LOGW(TAG, "page out of range: %u", p);
        return;
    }
    (void)_cmd(d, ST7565_CMD_PAGE_ADDRESS | p);
}

void st7565_set_column_address(st7565_t *d, uint8_t c)
{
    if (c >= ST7565_WIDTH) {
        ESP_LOGW(TAG, "column out of range: %u", c);
        return;
    }
    (void)_cmd(d, ST7565_CMD_COLUMN_MSB | (c >> 4));
    (void)_cmd(d, ST7565_CMD_COLUMN_LSB | (c & 0x0F));
}

/* ───── Framebuffer operations (bulk transfers) ───────────────────────────── */
static const uint8_t _zero_page[ST7565_WIDTH] = {0};

void st7565_clear_display(st7565_t *d)
{
    if (d == NULL || d->spi == NULL) {
        return;
    }

    for (int p = 0; p < ST7565_PAGES; ++p) {
        st7565_set_page_address(d, p);
        st7565_set_column_address(d, 0);
        (void)_send_data(d, _zero_page, ST7565_WIDTH);
    }
}

void st7565_draw_buffer_ex(st7565_t *d, const uint8_t *buf,
                           uint8_t page, uint8_t column,
                           uint8_t width, uint8_t pages)
{
    if (d == NULL || d->spi == NULL || buf == NULL) {
        return;
    }
    if (width == 0 || pages == 0 || page >= ST7565_PAGES || column >= ST7565_WIDTH) {
        return;
    }
    if ((uint16_t)page + pages > ST7565_PAGES) {
        pages = ST7565_PAGES - page;
    }
    if ((uint16_t)column + width > ST7565_WIDTH) {
        width = ST7565_WIDTH - column;
    }

    for (int p = 0; p < pages; ++p) {
        st7565_set_page_address(d, page + p);
        st7565_set_column_address(d, column);
        (void)_send_data(d, buf + p * width, width);
    }
}

void st7565_draw_buffer(st7565_t *d, const uint8_t *buf)
{
    if (buf == NULL) {
        return;
    }
    st7565_draw_buffer_ex(d, buf, 0, 0, ST7565_WIDTH, ST7565_PAGES);
}

/* ───── Display control ───────────────────────────────────────────────────── */
void st7565_contrast(st7565_t *d, uint8_t lvl)
{
    if (lvl > 63) {
        ESP_LOGW(TAG, "contrast out of range (%u), clamping to 63", lvl);
        lvl = 63;
    }
    (void)_cmd(d, ST7565_CMD_ELECTRONIC_VOL);
    (void)_cmd(d, lvl);
}

void st7565_invert(st7565_t *d, bool inv)
{
    (void)_cmd(d, inv ? ST7565_CMD_DISPLAY_REVERSE : ST7565_CMD_DISPLAY_NORMAL);
}

void st7565_power_on(st7565_t *d)  { (void)_cmd(d, ST7565_CMD_DISPLAY_ON); }
void st7565_power_off(st7565_t *d) { (void)_cmd(d, ST7565_CMD_DISPLAY_OFF); }

void st7565_set_start_line(st7565_t *d, uint8_t line)
{
    if (line > 63) {
        ESP_LOGW(TAG, "start line out of range (%u), clamping to 63", line);
        line = 63;
    }
    (void)_cmd(d, ST7565_CMD_START_LINE | line);
}

void st7565_all_points_on(st7565_t *d, bool on)
{
    (void)_cmd(d, on ? ST7565_CMD_ALL_POINTS_ON : ST7565_CMD_ALL_POINTS_OFF);
}

void st7565_sleep(st7565_t *d)
{
    (void)_cmd(d, ST7565_CMD_DISPLAY_OFF);
    (void)_cmd(d, ST7565_CMD_ALL_POINTS_ON);
    (void)_cmd(d, ST7565_CMD_SLEEP_MODE);
}

void st7565_wake(st7565_t *d)
{
    (void)_cmd(d, ST7565_CMD_NORMAL_MODE);
    (void)_cmd(d, ST7565_CMD_ALL_POINTS_OFF);
    (void)_cmd(d, ST7565_CMD_DISPLAY_ON);
}

/* ───── Orientation ───────────────────────────────────────────────────────── */
void st7565_set_adc(st7565_t *d, bool reverse)
{
    (void)_cmd(d, reverse ? ST7565_CMD_ADC_REVERSE : ST7565_CMD_ADC_NORMAL);
}

void st7565_set_com_dir(st7565_t *d, bool reverse)
{
    (void)_cmd(d, reverse ? ST7565_CMD_COM_REVERSE : ST7565_CMD_COM_NORMAL);
}

/* ───── Booster / bias / resistor ratio ───────────────────────────────────── */
void st7565_set_bias(st7565_t *d, bool bias_1_7)
{
    (void)_cmd(d, bias_1_7 ? ST7565_CMD_BIAS_1_7 : ST7565_CMD_BIAS_1_9);
}

void st7565_set_v0_ratio(st7565_t *d, uint8_t ratio)
{
    if (ratio > 7) {
        ESP_LOGW(TAG, "v0 ratio out of range (%u), clamping to 7", ratio);
        ratio = 7;
    }
    (void)_cmd(d, ST7565_CMD_V0_RATIO | ratio);
}

void st7565_set_booster(st7565_t *d, uint8_t ratio)
{
    if (ratio != ST7565_CMD_BOOSTER_2X_3X_4X &&
        ratio != ST7565_CMD_BOOSTER_5X &&
        ratio != ST7565_CMD_BOOSTER_6X) {
        ESP_LOGW(TAG, "invalid booster ratio: %u (expected 0, 1, or 3)", ratio);
        return;
    }
    (void)_cmd(d, ST7565_CMD_BOOSTER_RATIO_SET);
    (void)_cmd(d, ratio);
}

void st7565_set_power_ctrl(st7565_t *d, uint8_t flags)
{
    if (flags > 7) {
        ESP_LOGW(TAG, "power control flags out of range (%u), clamping to 7", flags);
        flags = 7;
    }
    (void)_cmd(d, ST7565_CMD_POWER_CTRL | flags);
}

/* ───── Read-Modify-Write ─────────────────────────────────────────────────── */
void st7565_rmw_start(st7565_t *d) { (void)_cmd(d, ST7565_CMD_READ_MODIFY_WRITE); }
void st7565_rmw_end(st7565_t *d)   { (void)_cmd(d, ST7565_CMD_END); }

/* ───── Software reset ────────────────────────────────────────────────────── */
void st7565_reset(st7565_t *d) { (void)_cmd(d, ST7565_CMD_RESET); }

/* ───── Init ──────────────────────────────────────────────────────────────── */
static uint8_t _sanitize_booster(uint8_t booster)
{
    if (booster == ST7565_CMD_BOOSTER_2X_3X_4X ||
        booster == ST7565_CMD_BOOSTER_5X ||
        booster == ST7565_CMD_BOOSTER_6X) {
        return booster;
    }
    ESP_LOGW(TAG, "invalid booster ratio in init profile: %u; using default", booster);
    return ST7565_INIT_PROFILE_DEFAULT.booster;
}

esp_err_t st7565_init_ex(st7565_t *d, spi_host_device_t host,
                         gpio_num_t cs, gpio_num_t rs, gpio_num_t rst,
                         gpio_num_t sda, gpio_num_t sck,
                         const st7565_init_profile_t *profile)
{
    if (d == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const st7565_init_profile_t *cfg = profile ? profile : &ST7565_INIT_PROFILE_DEFAULT;
    st7565_init_profile_t sane = *cfg;
    if (sane.contrast > 63) {
        sane.contrast = 63;
    }
    if (sane.v0_ratio > 7) {
        sane.v0_ratio = 7;
    }
    if (sane.power_ctrl > 7) {
        sane.power_ctrl = 7;
    }
    if (sane.start_line > 63) {
        sane.start_line = 63;
    }
    sane.booster = _sanitize_booster(sane.booster);

    d->cs = cs;
    d->rs = rs;
    d->rst = rst;
    d->host = host;
    d->initialized = false;
    d->spi = NULL;

    gpio_set_direction(cs, GPIO_MODE_OUTPUT);
    gpio_set_direction(rs, GPIO_MODE_OUTPUT);
    gpio_set_direction(rst, GPIO_MODE_OUTPUT);

    spi_bus_config_t bus = {
        .mosi_io_num = sda,
        .sclk_io_num = sck,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7565_WIDTH * ST7565_PAGES,
    };

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    esp_err_t ret = spi_bus_initialize(host, &bus, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = spi_bus_add_device(host, &dev, &d->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        spi_bus_free(host);
        return ret;
    }

    /* Hardware reset */
    gpio_set_level(rst, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

#define ST7565_INIT_CMD(cmd_byte)                                             \
    do {                                                                      \
        ret = _cmd(d, (cmd_byte));                                            \
        if (ret != ESP_OK) {                                                  \
            goto init_fail;                                                   \
        }                                                                     \
    } while (0)

    /* ST7565 power-up command sequence */
    ST7565_INIT_CMD(ST7565_CMD_DISPLAY_OFF);
    ST7565_INIT_CMD(sane.bias_1_7 ? ST7565_CMD_BIAS_1_7 : ST7565_CMD_BIAS_1_9);
    ST7565_INIT_CMD(sane.adc_reverse ? ST7565_CMD_ADC_REVERSE : ST7565_CMD_ADC_NORMAL);
    ST7565_INIT_CMD(sane.com_reverse ? ST7565_CMD_COM_REVERSE : ST7565_CMD_COM_NORMAL);
    ST7565_INIT_CMD(ST7565_CMD_DISPLAY_NORMAL);
    ST7565_INIT_CMD(ST7565_CMD_POWER_CTRL | sane.power_ctrl);
    ST7565_INIT_CMD(ST7565_CMD_V0_RATIO | sane.v0_ratio);
    ST7565_INIT_CMD(ST7565_CMD_ELECTRONIC_VOL);
    ST7565_INIT_CMD(sane.contrast);
    ST7565_INIT_CMD(ST7565_CMD_BOOSTER_RATIO_SET);
    ST7565_INIT_CMD(sane.booster);
    ST7565_INIT_CMD(ST7565_CMD_START_LINE | sane.start_line);
    ST7565_INIT_CMD(ST7565_CMD_DISPLAY_ON);

#undef ST7565_INIT_CMD

    d->initialized = true;
    return ESP_OK;

init_fail:
    (void)spi_bus_remove_device(d->spi);
    d->spi = NULL;
    (void)spi_bus_free(d->host);
    return ret;
}

esp_err_t st7565_init(st7565_t *d, spi_host_device_t host,
                      gpio_num_t cs, gpio_num_t rs, gpio_num_t rst,
                      gpio_num_t sda, gpio_num_t sck)
{
    return st7565_init_ex(d, host, cs, rs, rst, sda, sck, &ST7565_INIT_PROFILE_DEFAULT);
}

/* ───── Deinit ────────────────────────────────────────────────────────────── */
esp_err_t st7565_deinit(st7565_t *d)
{
    if (d == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!d->initialized) {
        return ESP_OK;
    }

    (void)_cmd(d, ST7565_CMD_DISPLAY_OFF);

    esp_err_t ret = spi_bus_remove_device(d->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_remove_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = spi_bus_free(d->host);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_free failed: %s", esp_err_to_name(ret));
        return ret;
    }

    d->spi = NULL;
    d->initialized = false;
    return ESP_OK;
}
