#include "hybrid_sim_capture.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

typedef struct {
    bool enabled;
    uint8_t fb[HYBRID_SIM_FB_LEN];
    uint8_t visible_fb[HYBRID_SIM_FB_LEN];
    uint8_t cursor_page;
    uint8_t cursor_col;
    uint32_t frame_id;
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
    bool expect_contrast;
    bool expect_booster_ratio;
} hybrid_sim_state_t;

static inline void hybrid_sim_reset_display_state_locked(void);
static inline void hybrid_sim_set_bool_locked(bool *field, bool value);
static inline void hybrid_sim_set_u8_locked(uint8_t *field, uint8_t value);
static inline void hybrid_sim_copy_display_state_locked(hybrid_sim_display_state_t *out_state);
static void hybrid_sim_render_visible_fb_locked(void);

static hybrid_sim_state_t s_state = {
    .enabled = true,
    .display_on = true,
    .invert = false,
    .all_points_on = false,
    .sleep_mode = false,
    .adc_reverse = false,
    .com_reverse = true,
    .start_line = 0,
    .contrast = 0x02,
    .v0_ratio = 0x07,
    .power_ctrl = 0x07,
    .booster_ratio = 0x00,
    .expect_contrast = false,
    .expect_booster_ratio = false,
    .cursor_page = 0,
    .cursor_col = 0,
    .frame_id = 0,
};

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static inline void hybrid_sim_touch_locked(void) {
    s_state.frame_id += 1;
}

static inline void hybrid_sim_set_bool_locked(bool *field, bool value) {
    if (*field != value) {
        *field = value;
        hybrid_sim_touch_locked();
    }
}

static inline void hybrid_sim_set_u8_locked(uint8_t *field, uint8_t value) {
    if (*field != value) {
        *field = value;
        hybrid_sim_touch_locked();
    }
}

static inline void hybrid_sim_reset_display_state_locked(void) {
    s_state.display_on = true;
    s_state.invert = false;
    s_state.all_points_on = false;
    s_state.sleep_mode = false;
    s_state.adc_reverse = false;
    s_state.com_reverse = true;
    s_state.start_line = 0;
    s_state.contrast = 0x02;
    s_state.v0_ratio = 0x07;
    s_state.power_ctrl = 0x07;
    s_state.booster_ratio = 0x00;
    s_state.expect_contrast = false;
    s_state.expect_booster_ratio = false;
}

static inline void hybrid_sim_copy_display_state_locked(hybrid_sim_display_state_t *out_state) {
    out_state->display_on = s_state.display_on;
    out_state->invert = s_state.invert;
    out_state->all_points_on = s_state.all_points_on;
    out_state->sleep_mode = s_state.sleep_mode;
    out_state->adc_reverse = s_state.adc_reverse;
    out_state->com_reverse = s_state.com_reverse;
    out_state->start_line = s_state.start_line;
    out_state->contrast = s_state.contrast;
    out_state->v0_ratio = s_state.v0_ratio;
    out_state->power_ctrl = s_state.power_ctrl;
    out_state->booster_ratio = s_state.booster_ratio;
}

static void hybrid_sim_render_visible_fb_locked(void) {
    memset(s_state.visible_fb, 0, sizeof(s_state.visible_fb));

    if (!s_state.display_on || s_state.sleep_mode) {
        return;
    }

    if (s_state.all_points_on) {
        memset(s_state.visible_fb, 0xFF, sizeof(s_state.visible_fb));
        return;
    }

    const bool flip_x = s_state.adc_reverse;
    const bool flip_y = !s_state.com_reverse;
    const uint8_t start_line = (uint8_t)(s_state.start_line & 0x3Fu);

    if (!flip_x && !flip_y && start_line == 0) {
        memcpy(s_state.visible_fb, s_state.fb, sizeof(s_state.fb));
    } else {
        for (uint8_t page = 0; page < HYBRID_SIM_PAGES; ++page) {
            const size_t base = (size_t)page * HYBRID_SIM_WIDTH;
            for (uint8_t src_x = 0; src_x < HYBRID_SIM_WIDTH; ++src_x) {
                const uint8_t data = s_state.fb[base + src_x];
                if (data == 0) {
                    continue;
                }
                const uint8_t dst_x = flip_x ? (uint8_t)(HYBRID_SIM_WIDTH - 1 - src_x) : src_x;
                for (uint8_t bit = 0; bit < 8; ++bit) {
                    if ((data & (1u << bit)) == 0) {
                        continue;
                    }
                    const uint8_t src_y = (uint8_t)((page << 3) | bit);
                    uint8_t dst_y = (uint8_t)((src_y + HYBRID_SIM_HEIGHT - start_line) & 0x3Fu);
                    if (flip_y) {
                        dst_y = (uint8_t)(HYBRID_SIM_HEIGHT - 1 - dst_y);
                    }
                    const size_t dst_idx = ((size_t)(dst_y >> 3) * HYBRID_SIM_WIDTH) + dst_x;
                    s_state.visible_fb[dst_idx] |= (uint8_t)(1u << (dst_y & 0x07u));
                }
            }
        }
    }

    if (s_state.invert) {
        for (size_t i = 0; i < HYBRID_SIM_FB_LEN; ++i) {
            s_state.visible_fb[i] = (uint8_t)~s_state.visible_fb[i];
        }
    }
}

void hybrid_sim_capture_enable(bool enabled) {
    portENTER_CRITICAL(&s_lock);
    s_state.enabled = enabled;
    portEXIT_CRITICAL(&s_lock);
}

bool hybrid_sim_capture_enabled(void) {
    portENTER_CRITICAL(&s_lock);
    bool enabled = s_state.enabled;
    portEXIT_CRITICAL(&s_lock);
    return enabled;
}

void hybrid_sim_capture_reset(void) {
    portENTER_CRITICAL(&s_lock);
    memset(s_state.fb, 0, sizeof(s_state.fb));
    memset(s_state.visible_fb, 0, sizeof(s_state.visible_fb));
    hybrid_sim_reset_display_state_locked();
    s_state.cursor_page = 0;
    s_state.cursor_col = 0;
    s_state.frame_id = 0;
    portEXIT_CRITICAL(&s_lock);
}

uint32_t hybrid_sim_capture_frame_id(void) {
    portENTER_CRITICAL(&s_lock);
    uint32_t frame_id = s_state.frame_id;
    portEXIT_CRITICAL(&s_lock);
    return frame_id;
}

void hybrid_sim_capture_set_page(uint8_t page) {
    if (page >= HYBRID_SIM_PAGES) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    s_state.cursor_page = page;
    portEXIT_CRITICAL(&s_lock);
}

void hybrid_sim_capture_set_column(uint8_t column) {
    if (column >= HYBRID_SIM_WIDTH) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    s_state.cursor_col = column;
    portEXIT_CRITICAL(&s_lock);
}

void hybrid_sim_capture_write_instruction(uint8_t cmd) {
    portENTER_CRITICAL(&s_lock);

    if (s_state.expect_contrast) {
        s_state.expect_contrast = false;
        hybrid_sim_set_u8_locked(&s_state.contrast, (uint8_t)(cmd & 0x3Fu));
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    if (s_state.expect_booster_ratio) {
        s_state.expect_booster_ratio = false;
        if (cmd == 0x00u || cmd == 0x01u || cmd == 0x03u) {
            hybrid_sim_set_u8_locked(&s_state.booster_ratio, cmd);
        }
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    if ((cmd & 0xF8u) == 0xB0u) {
        s_state.cursor_page = (uint8_t)(cmd & 0x07u);
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    if ((cmd & 0xF0u) == 0x10u) {
        s_state.cursor_col = (uint8_t)(((cmd & 0x0Fu) << 4) | (s_state.cursor_col & 0x0Fu));
        if (s_state.cursor_col >= HYBRID_SIM_WIDTH) {
            s_state.cursor_col = HYBRID_SIM_WIDTH - 1;
        }
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    if ((cmd & 0xF0u) == 0x00u) {
        s_state.cursor_col = (uint8_t)((s_state.cursor_col & 0xF0u) | (cmd & 0x0Fu));
        if (s_state.cursor_col >= HYBRID_SIM_WIDTH) {
            s_state.cursor_col = HYBRID_SIM_WIDTH - 1;
        }
    }

    if ((cmd & 0xC0u) == 0x40u) {
        hybrid_sim_set_u8_locked(&s_state.start_line, (uint8_t)(cmd & 0x3Fu));
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    switch (cmd) {
        case 0xAE:  // display off
            hybrid_sim_set_bool_locked(&s_state.display_on, false);
            break;
        case 0xAF:  // display on
            hybrid_sim_set_bool_locked(&s_state.display_on, true);
            break;
        case 0xA6:  // normal display
            hybrid_sim_set_bool_locked(&s_state.invert, false);
            break;
        case 0xA7:  // reverse display
            hybrid_sim_set_bool_locked(&s_state.invert, true);
            break;
        case 0xA4:  // all points off (normal RAM display)
            hybrid_sim_set_bool_locked(&s_state.all_points_on, false);
            break;
        case 0xA5:  // all points on
            hybrid_sim_set_bool_locked(&s_state.all_points_on, true);
            break;
        case 0xA0:  // ADC normal
            hybrid_sim_set_bool_locked(&s_state.adc_reverse, false);
            break;
        case 0xA1:  // ADC reverse
            hybrid_sim_set_bool_locked(&s_state.adc_reverse, true);
            break;
        case 0xC0:  // COM normal
            hybrid_sim_set_bool_locked(&s_state.com_reverse, false);
            break;
        case 0xC8:  // COM reverse
            hybrid_sim_set_bool_locked(&s_state.com_reverse, true);
            break;
        case 0x81:  // next command byte carries contrast value
            s_state.expect_contrast = true;
            break;
        case 0xF8:  // next command byte carries booster ratio
            s_state.expect_booster_ratio = true;
            break;
        case 0xAC:  // sleep mode
            hybrid_sim_set_bool_locked(&s_state.sleep_mode, true);
            break;
        case 0xAD:  // normal mode
            hybrid_sim_set_bool_locked(&s_state.sleep_mode, false);
            break;
        case 0xE2:  // internal reset
            hybrid_sim_set_bool_locked(&s_state.display_on, true);
            hybrid_sim_set_bool_locked(&s_state.invert, false);
            hybrid_sim_set_bool_locked(&s_state.all_points_on, false);
            hybrid_sim_set_bool_locked(&s_state.sleep_mode, false);
            hybrid_sim_set_bool_locked(&s_state.adc_reverse, false);
            hybrid_sim_set_bool_locked(&s_state.com_reverse, true);
            hybrid_sim_set_u8_locked(&s_state.start_line, 0);
            hybrid_sim_set_u8_locked(&s_state.contrast, 0x02);
            hybrid_sim_set_u8_locked(&s_state.v0_ratio, 0x07);
            hybrid_sim_set_u8_locked(&s_state.power_ctrl, 0x07);
            hybrid_sim_set_u8_locked(&s_state.booster_ratio, 0x00);
            s_state.expect_contrast = false;
            s_state.expect_booster_ratio = false;
            break;
        default:
            if ((cmd & 0xF8u) == 0x28u) {
                hybrid_sim_set_u8_locked(&s_state.power_ctrl, (uint8_t)(cmd & 0x07u));
            } else if ((cmd & 0xF8u) == 0x20u) {
                hybrid_sim_set_u8_locked(&s_state.v0_ratio, (uint8_t)(cmd & 0x07u));
            }
            break;
    }

    portEXIT_CRITICAL(&s_lock);
}

void hybrid_sim_capture_write_byte(uint8_t value) {
    portENTER_CRITICAL(&s_lock);

    if (!s_state.enabled ||
        s_state.cursor_page >= HYBRID_SIM_PAGES ||
        s_state.cursor_col >= HYBRID_SIM_WIDTH) {
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    size_t idx = ((size_t)s_state.cursor_page * HYBRID_SIM_WIDTH) + s_state.cursor_col;
    bool changed = s_state.fb[idx] != value;
    s_state.fb[idx] = value;
    if (s_state.cursor_col < (HYBRID_SIM_WIDTH - 1)) {
        s_state.cursor_col += 1;
    }
    if (changed) {
        hybrid_sim_touch_locked();
    }
    portEXIT_CRITICAL(&s_lock);
}

void hybrid_sim_capture_draw_region(const uint8_t *buf, uint8_t page, uint8_t column, uint8_t width, uint8_t pages) {
    if (buf == NULL || width == 0 || pages == 0 || page >= HYBRID_SIM_PAGES || column >= HYBRID_SIM_WIDTH) {
        return;
    }

    uint8_t clipped_pages = pages;
    uint8_t clipped_width = width;
    if ((uint16_t)page + clipped_pages > HYBRID_SIM_PAGES) {
        clipped_pages = (uint8_t)(HYBRID_SIM_PAGES - page);
    }
    if ((uint16_t)column + clipped_width > HYBRID_SIM_WIDTH) {
        clipped_width = (uint8_t)(HYBRID_SIM_WIDTH - column);
    }
    if (clipped_pages == 0 || clipped_width == 0) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    if (!s_state.enabled) {
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    bool changed = false;
    for (uint8_t p = 0; p < clipped_pages; ++p) {
        size_t dst_idx = ((size_t)(page + p) * HYBRID_SIM_WIDTH) + column;
        size_t src_idx = (size_t)p * width;
        if (memcmp(&s_state.fb[dst_idx], &buf[src_idx], clipped_width) != 0) {
            memcpy(&s_state.fb[dst_idx], &buf[src_idx], clipped_width);
            changed = true;
        }
    }
    if (changed) {
        hybrid_sim_touch_locked();
    }
    portEXIT_CRITICAL(&s_lock);
}

void hybrid_sim_capture_clear(void) {
    portENTER_CRITICAL(&s_lock);
    if (!s_state.enabled) {
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    bool had_pixels = false;
    for (size_t i = 0; i < HYBRID_SIM_FB_LEN; ++i) {
        if (s_state.fb[i] != 0) {
            had_pixels = true;
            break;
        }
    }
    if (had_pixels) {
        memset(s_state.fb, 0, sizeof(s_state.fb));
        hybrid_sim_touch_locked();
    }
    portEXIT_CRITICAL(&s_lock);
}

size_t hybrid_sim_capture_read_fb(uint8_t *dst, size_t dst_len) {
    if (dst == NULL || dst_len == 0) {
        return 0;
    }
    size_t n = dst_len < HYBRID_SIM_FB_LEN ? dst_len : HYBRID_SIM_FB_LEN;
    portENTER_CRITICAL(&s_lock);
    hybrid_sim_render_visible_fb_locked();
    memcpy(dst, s_state.visible_fb, n);
    portEXIT_CRITICAL(&s_lock);
    return n;
}

void hybrid_sim_capture_read_display_state(hybrid_sim_display_state_t *out_state) {
    if (out_state == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    hybrid_sim_copy_display_state_locked(out_state);
    portEXIT_CRITICAL(&s_lock);
}

void hybrid_sim_capture_read_snapshot(hybrid_sim_snapshot_t *out_snapshot) {
    if (out_snapshot == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    hybrid_sim_render_visible_fb_locked();
    out_snapshot->frame_id = s_state.frame_id;
    hybrid_sim_copy_display_state_locked(&out_snapshot->display_state);
    memcpy(out_snapshot->fb, s_state.visible_fb, sizeof(out_snapshot->fb));
    portEXIT_CRITICAL(&s_lock);
}
