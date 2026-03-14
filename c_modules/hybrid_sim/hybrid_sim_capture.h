#ifndef HYBRID_SIM_CAPTURE_H
#define HYBRID_SIM_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HYBRID_SIM_WIDTH (128)
#define HYBRID_SIM_HEIGHT (64)
#define HYBRID_SIM_PAGES (HYBRID_SIM_HEIGHT / 8)
#define HYBRID_SIM_FB_LEN (HYBRID_SIM_WIDTH * HYBRID_SIM_PAGES)

typedef struct {
    bool display_on;
    bool invert;
    bool all_points_on;
    bool sleep_mode;
    bool adc_reverse;
    bool com_reverse;
    uint8_t start_line;
    uint8_t contrast;
    uint8_t v0_ratio;
    uint8_t power_ctrl;
    uint8_t booster_ratio;
} hybrid_sim_display_state_t;

void hybrid_sim_capture_enable(bool enabled);
bool hybrid_sim_capture_enabled(void);
void hybrid_sim_capture_reset(void);
uint32_t hybrid_sim_capture_frame_id(void);

void hybrid_sim_capture_set_page(uint8_t page);
void hybrid_sim_capture_set_column(uint8_t column);
void hybrid_sim_capture_write_instruction(uint8_t cmd);
void hybrid_sim_capture_write_byte(uint8_t value);
void hybrid_sim_capture_draw_region(const uint8_t *buf, uint8_t page, uint8_t column, uint8_t width, uint8_t pages);
void hybrid_sim_capture_clear(void);

size_t hybrid_sim_capture_read_fb(uint8_t *dst, size_t dst_len);
void hybrid_sim_capture_read_display_state(hybrid_sim_display_state_t *out_state);

#endif
