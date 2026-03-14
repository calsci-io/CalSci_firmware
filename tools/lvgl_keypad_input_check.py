import time
from machine import Pin

import lvgl as lv
import st7565 as display

WIDTH = 128
HEIGHT = 64
FB_SIZE = (WIDTH * HEIGHT) // 8
SRC_STRIDE = WIDTH // 8

ROW_PINS = [14, 21, 47, 48, 38, 39, 40, 41, 42, 1]
COL_PINS = [8, 18, 17, 15, 7]

KEYMAP_DEFAULT = [
    ["on", "alpha", "beta", "home", "wifi"],
    ["backlight", "back", "toolbox", "diff(", "ln"],
    ["nav_l", "nav_d", "nav_r", "ok", "nav_u"],
    ["module", "bluetooth", "sin(", "cos", "tan"],
    ["ingn(", "pi", "e", "summation", "fraction"],
    ["log", "pow(", "pow( ,0.5)", "pow( ,2)", "S_D"],
    ["7", "8", "9", "nav_b", "AC"],
    ["4", "5", "6", "*", "/"],
    ["1", "2", "3", "+", "-"],
    [".", "0", ",", "ans", "exe"],
]

NAV_KEYS = ("nav_u", "nav_d", "nav_l", "nav_r", "ok")
DIGITS = "0123456789"
EXIT_KEYS = ("AC", "home", "back", "on")


class MatrixKeypad:
    def __init__(self, rows, cols, keymap, debounce_ms=70, repeat_ms=170):
        self.row_pins = [Pin(p, Pin.OUT) for p in rows]
        self.col_pins = [Pin(p, Pin.IN, Pin.PULL_UP) for p in cols]
        for p in self.row_pins:
            p.value(1)

        self.keymap = keymap
        self.debounce_ms = debounce_ms
        self.repeat_ms = repeat_ms

        self._sample_key = None
        self._sample_since = time.ticks_ms()
        self._active_key = None
        self._next_repeat = time.ticks_ms()

    def _scan_once(self):
        for r, row_pin in enumerate(self.row_pins):
            row_pin.value(0)
            for c, col_pin in enumerate(self.col_pins):
                if col_pin.value() == 0:
                    row_pin.value(1)
                    return self.keymap[r][c]
            row_pin.value(1)
        return None

    def get_event(self):
        now = time.ticks_ms()
        key = self._scan_once()

        if key != self._sample_key:
            self._sample_key = key
            self._sample_since = now
            return None
        if time.ticks_diff(now, self._sample_since) < self.debounce_ms:
            return None

        if key != self._active_key:
            self._active_key = key
            if key is None:
                return None
            self._next_repeat = time.ticks_add(now, self.repeat_ms)
            return key

        if key is not None and time.ticks_diff(now, self._next_repeat) >= 0:
            self._next_repeat = time.ticks_add(now, self.repeat_ms)
            return key
        return None


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

    def i1_to_st7565(src_bytes, dst_bytes):
        for i in range(FB_SIZE):
            dst_bytes[i] = 0
        for y in range(HEIGHT):
            page = y >> 3
            bit = 1 << (y & 7)
            src_row = y * SRC_STRIDE
            dst_row = page * WIDTH
            for xb in range(SRC_STRIDE):
                b = src_bytes[src_row + xb]
                x = xb * 8
                if b & 0x80:
                    dst_bytes[dst_row + x + 0] |= bit
                if b & 0x40:
                    dst_bytes[dst_row + x + 1] |= bit
                if b & 0x20:
                    dst_bytes[dst_row + x + 2] |= bit
                if b & 0x10:
                    dst_bytes[dst_row + x + 3] |= bit
                if b & 0x08:
                    dst_bytes[dst_row + x + 4] |= bit
                if b & 0x04:
                    dst_bytes[dst_row + x + 5] |= bit
                if b & 0x02:
                    dst_bytes[dst_row + x + 6] |= bit
                if b & 0x01:
                    dst_bytes[dst_row + x + 7] |= bit

    def flush_cb(_disp, _area, color_p):
        raw = color_p.__dereference__(len(draw_buf))
        i1_to_st7565(raw[8 : 8 + FB_SIZE], tx_buf)
        display.graphics(tx_buf)
        disp.flush_ready()

    disp.set_flush_cb(flush_cb)


def build_screen():
    scr = lv.obj()
    scr.set_size(WIDTH, HEIGHT)
    scr.set_style_bg_opa(lv.OPA.COVER, 0)
    scr.set_style_bg_color(lv.color_white(), 0)

    title = lv.label(scr)
    title.set_text("KEYPAD INPUT CHECK")
    title.set_style_text_color(lv.color_black(), 0)
    title.align(lv.ALIGN.TOP_LEFT, 0, 0)

    nav_label = lv.label(scr)
    nav_label.set_style_text_color(lv.color_black(), 0)
    nav_label.align(lv.ALIGN.TOP_LEFT, 0, 12)

    num_label = lv.label(scr)
    num_label.set_style_text_color(lv.color_black(), 0)
    num_label.align(lv.ALIGN.TOP_LEFT, 0, 24)

    last_label = lv.label(scr)
    last_label.set_style_text_color(lv.color_black(), 0)
    last_label.align(lv.ALIGN.TOP_LEFT, 0, 36)

    pass_label = lv.label(scr)
    pass_label.set_style_text_color(lv.color_black(), 0)
    pass_label.align(lv.ALIGN.TOP_LEFT, 0, 48)

    return scr, nav_label, num_label, last_label, pass_label


def nav_text(nav_seen):
    return "NAV:" + (
        ("U" if nav_seen["nav_u"] else "_")
        + ("D" if nav_seen["nav_d"] else "_")
        + ("L" if nav_seen["nav_l"] else "_")
        + ("R" if nav_seen["nav_r"] else "_")
        + ("O" if nav_seen["ok"] else "_")
    )


def num_text(digits_seen):
    s = "NUM:"
    for d in DIGITS:
        s += d if d in digits_seen else "_"
    return s


def update_ui(nav_label, num_label, last_label, pass_label, nav_seen, digits_seen, last_key, passed):
    nav_label.set_text(nav_text(nav_seen))
    num_label.set_text(num_text(digits_seen))
    last_label.set_text("KEY:" + (last_key if last_key else "-"))
    pass_label.set_text("PASS: YES" if passed else "PASS: NO")


def main():
    ensure_st7565_ready()
    build_lvgl_display()

    keypad = MatrixKeypad(ROW_PINS, COL_PINS, KEYMAP_DEFAULT)
    nav_seen = {"nav_u": False, "nav_d": False, "nav_l": False, "nav_r": False, "ok": False}
    digits_seen = set()
    last_key = None
    passed = False

    scr, nav_label, num_label, last_label, pass_label = build_screen()
    update_ui(nav_label, num_label, last_label, pass_label, nav_seen, digits_seen, last_key, passed)
    lv.screen_load(scr)

    print("keypad_check_ready")
    print("check nav_u/nav_d/nav_l/nav_r/ok and 0..9")
    print("exit keys: AC/home/back/on")

    while True:
        key = keypad.get_event()
        if key is not None:
            last_key = key
            print("key", key)

            if key in NAV_KEYS:
                nav_seen[key] = True
            elif key == "exe":
                nav_seen["ok"] = True

            if key in DIGITS:
                digits_seen.add(key)

            if key in EXIT_KEYS:
                update_ui(nav_label, num_label, last_label, pass_label, nav_seen, digits_seen, last_key, passed)
                print("keypad_check_exit")
                break

            if (not passed) and all(nav_seen.values()) and len(digits_seen) == 10:
                passed = True
                print("KEYPAD_CHECK_PASS")

            update_ui(nav_label, num_label, last_label, pass_label, nav_seen, digits_seen, last_key, passed)

        lv.tick_inc(20)
        lv.timer_handler()
        time.sleep_ms(20)


main()
