import time

import lvgl as lv
import st7565 as display

WIDTH = 128
HEIGHT = 64
FB_SIZE = (WIDTH * HEIGHT) // 8
SRC_STRIDE = WIDTH // 8

# Tune these in steps based on your YES/NO feedback.
MAP_INVERT = False
MAP_MIRROR_X = False
MAP_MIRROR_Y = False


def ensure_st7565_ready():
    probe = bytearray(FB_SIZE)
    try:
        display.graphics(probe)
    except Exception:
        display.init(9, 11, 10, 13, 12)
    display.on()
    display.invert(False)
    display.all_points_on(False)
    display.graphics(probe)


def build_lvgl_display():
    if not lv.is_initialized():
        lv.init()
    else:
        old = lv.display_get_default()
        if old is not None:
            try:
                old.delete()
            except Exception:
                pass

    draw_buf = bytearray(FB_SIZE + 8)
    tx_buf = bytearray(FB_SIZE)

    disp = lv.display_create(WIDTH, HEIGHT)
    disp.set_color_format(lv.COLOR_FORMAT.I1)
    disp.set_buffers(draw_buf, None, len(draw_buf), lv.DISPLAY_RENDER_MODE.FULL)
    disp.set_default()

    def src_bit(src, x, y):
        idx = y * SRC_STRIDE + (x >> 3)
        bit = 0x80 >> (x & 7)
        return 1 if (src[idx] & bit) else 0

    def i1_to_st7565(src, dst):
        for i in range(FB_SIZE):
            dst[i] = 0

        for y in range(HEIGHT):
            dy = HEIGHT - 1 - y if MAP_MIRROR_Y else y
            page = dy >> 3
            dmask = 1 << (dy & 7)
            base = page * WIDTH
            for x in range(WIDTH):
                sx = WIDTH - 1 - x if MAP_MIRROR_X else x
                on = src_bit(src, sx, y)
                if MAP_INVERT:
                    on ^= 1
                if on:
                    dst[base + x] |= dmask

    def flush_cb(_disp, _area, color_p):
        raw = color_p.__dereference__(len(draw_buf))
        src = raw[8 : 8 + FB_SIZE]
        i1_to_st7565(src, tx_buf)
        display.graphics(tx_buf)
        disp.flush_ready()

    disp.set_flush_cb(flush_cb)


def build_test_screen():
    scr = lv.obj()
    scr.set_size(WIDTH, HEIGHT)
    scr.set_style_bg_opa(lv.OPA.COVER, 0)
    scr.set_style_bg_color(lv.color_white(), 0)

    frame = lv.obj(scr)
    frame.set_size(WIDTH - 2, HEIGHT - 2)
    frame.align(lv.ALIGN.CENTER, 0, 0)
    frame.set_style_bg_opa(lv.OPA.COVER, 0)
    frame.set_style_bg_color(lv.color_white(), 0)
    frame.set_style_border_width(1, 0)
    frame.set_style_border_color(lv.color_black(), 0)
    frame.set_style_radius(0, 0)

    tl = lv.label(scr)
    tl.set_text("TL")
    tl.set_style_text_color(lv.color_black(), 0)
    tl.align(lv.ALIGN.TOP_LEFT, 2, 0)

    tr = lv.label(scr)
    tr.set_text("TR")
    tr.set_style_text_color(lv.color_black(), 0)
    tr.align(lv.ALIGN.TOP_RIGHT, -2, 0)

    bl = lv.label(scr)
    bl.set_text("BL")
    bl.set_style_text_color(lv.color_black(), 0)
    bl.align(lv.ALIGN.BOTTOM_LEFT, 2, -1)

    br = lv.label(scr)
    br.set_text("BR")
    br.set_style_text_color(lv.color_black(), 0)
    br.align(lv.ALIGN.BOTTOM_RIGHT, -2, -1)

    center = lv.label(scr)
    center.set_text("+")
    center.set_style_text_color(lv.color_black(), 0)
    center.align(lv.ALIGN.CENTER, 0, 0)

    cfg = lv.label(scr)
    cfg.set_text(
        "I{} X{} Y{}".format(
            1 if MAP_INVERT else 0,
            1 if MAP_MIRROR_X else 0,
            1 if MAP_MIRROR_Y else 0,
        )
    )
    cfg.set_style_text_color(lv.color_black(), 0)
    cfg.align(lv.ALIGN.BOTTOM_MID, 0, -1)

    return scr


def main():
    ensure_st7565_ready()
    build_lvgl_display()
    scr = build_test_screen()
    lv.screen_load(scr)

    print(
        "pixel_tuner cfg: invert={}, mirror_x={}, mirror_y={}".format(
            MAP_INVERT, MAP_MIRROR_X, MAP_MIRROR_Y
        )
    )
    print("check corners TL/TR/BL/BR and center '+'")

    for _ in range(1500):
        lv.timer_handler()
        time.sleep_ms(20)

    print("pixel_tuner_done")


main()
