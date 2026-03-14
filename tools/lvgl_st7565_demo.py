import time

import lvgl as lv
import st7565 as display

WIDTH = 128
HEIGHT = 64
FB_SIZE = (WIDTH * HEIGHT) // 8


def ensure_st7565_ready():
    probe = bytearray(FB_SIZE)
    try:
        display.graphics(probe)
        return
    except Exception:
        pass

    # Board wiring used in this project.
    display.init(9, 11, 10, 13, 12)
    display.graphics(probe)


def main():
    ensure_st7565_ready()
    lv.init()

    # LVGL v9 I1 format uses an 8-byte palette prefix in the draw buffer.
    draw_buf = bytearray(FB_SIZE + 8)

    disp = lv.display_create(WIDTH, HEIGHT)
    disp.set_color_format(lv.COLOR_FORMAT.I1)
    disp.set_buffers(draw_buf, None, len(draw_buf), lv.DISPLAY_RENDER_MODE.FULL)

    def flush_cb(_disp, _area, color_p):
        raw = color_p.__dereference__(len(draw_buf))
        frame = raw[8:8 + FB_SIZE]
        display.graphics(frame)
        disp.flush_ready()

    disp.set_flush_cb(flush_cb)

    scr = lv.obj()
    label = lv.label(scr)
    label.set_text("LVGL OK")
    label.center()
    lv.screen_load(scr)

    for _ in range(40):
        lv.timer_handler()
        time.sleep_ms(20)

    print("LVGL_DRAW_OK")


main()
