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

KEYMAP_ALPHA = [
    ["on", "alpha", "beta", "home", "wifi"],
    ["backlight", "back", "caps", "f", "l"],
    ["nav_l", "nav_d", "nav_r", "ok", "nav_u"],
    ["a", "b", "c", "d", "e"],
    ["g", "h", "i", "j", "k"],
    ["m", "n", "o", "p", "q"],
    ["r", "s", "t", "nav_b", "AC"],
    ["u", "v", "w", "*", "/"],
    ["x", "y", "z", "+", "-"],
    [" ", "off", "tab", "ans", "exe"],
]

KEYMAP_BETA = [
    ["on", "alpha", "beta", "home", "wifi"],
    ["backlight", "back", "undo", "=", "$"],
    ["nav_l", "nav_d", "nav_r", "ok", "nav_u"],
    ["copy", "paste", "asin(", "acos(", "atan("],
    ["&", "`", '"', "'", "shot"],
    ["^", "~", "!", "<", ">"],
    ["[", "]", "%", "nav_b", "AC"],
    ["{", "}", ":", "*", "/"],
    ["(", ")", ";", "+", "-"],
    ["@", "?", '"', "ans", "exe"],
]

KEYMAP_LAYERS = {"d": KEYMAP_DEFAULT, "a": KEYMAP_ALPHA, "b": KEYMAP_BETA}
LAYER_NAMES = {"d": "DEF", "a": "ALPHA", "b": "BETA"}

KEY_TO_LV = {
    "nav_u": lv.KEY.UP,
    "nav_d": lv.KEY.DOWN,
    "nav_l": lv.KEY.LEFT,
    "nav_r": lv.KEY.RIGHT,
    "ok": lv.KEY.ENTER,
    "exe": lv.KEY.ENTER,
}

REPEAT_KEYS = {"nav_u", "nav_d", "nav_l", "nav_r"}
EXIT_KEYS = {"back", "home", "on", "AC"}
CONTROL_KEYS = {
    "on",
    "home",
    "back",
    "AC",
    "alpha",
    "beta",
    "nav_u",
    "nav_d",
    "nav_l",
    "nav_r",
    "ok",
    "exe",
    "backlight",
    "wifi",
    "toolbox",
    "module",
    "bluetooth",
    "nav_b",
    "caps",
    "undo",
    "copy",
    "paste",
    "shot",
}

TOKEN_MAP = {
    "off": " ",
    "tab": "    ",
    "summation": "sum(",
    "fraction": "/",
    "pow( ,0.5)": "sqrt(",
    "pow( ,2)": "^2",
}


class MatrixKeypad:
    def __init__(self, rows, cols, layers, layer="d", debounce_ms=70, repeat_ms=170):
        self.row_pins = [Pin(p, Pin.OUT) for p in rows]
        self.col_pins = [Pin(p, Pin.IN, Pin.PULL_UP) for p in cols]
        for p in self.row_pins:
            p.value(1)

        self.layers = layers
        self.layer = layer
        self.debounce_ms = debounce_ms
        self.repeat_ms = repeat_ms

        now = time.ticks_ms()
        self._sample_raw = None
        self._sample_since = now
        self._stable_key = None
        self._next_repeat = now
        self._events = []

    def toggle_layer(self, target):
        if self.layer == target:
            self.layer = "d"
        else:
            self.layer = target

    def _scan_once(self):
        keymap = self.layers[self.layer]
        for r, row_pin in enumerate(self.row_pins):
            row_pin.value(0)
            for c, col_pin in enumerate(self.col_pins):
                if col_pin.value() == 0:
                    row_pin.value(1)
                    return keymap[r][c]
            row_pin.value(1)
        return None

    def tick(self):
        now = time.ticks_ms()
        raw = self._scan_once()

        if raw != self._sample_raw:
            self._sample_raw = raw
            self._sample_since = now
            return

        if time.ticks_diff(now, self._sample_since) < self.debounce_ms:
            return

        if raw != self._stable_key:
            self._stable_key = raw
            if raw is not None:
                self._events.append(raw)
                self._next_repeat = time.ticks_add(now, self.repeat_ms)
            return

        if raw in REPEAT_KEYS and time.ticks_diff(now, self._next_repeat) >= 0:
            self._events.append(raw)
            self._next_repeat = time.ticks_add(now, self.repeat_ms)

    def get_stable_key(self):
        return self._stable_key

    def pop_event(self):
        if not self._events:
            return None
        return self._events.pop(0)


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


def tail(text, limit):
    if len(text) <= limit:
        return text
    return text[-limit:]


def make_text_token(key):
    if key in CONTROL_KEYS:
        return None
    return TOKEN_MAP.get(key, key)


def build_text_screen():
    scr = lv.obj()
    scr.set_size(WIDTH, HEIGHT)
    scr.set_style_bg_opa(lv.OPA.COVER, 0)
    scr.set_style_bg_color(lv.color_white(), 0)

    header = lv.obj(scr)
    header.set_size(WIDTH, 10)
    header.align(lv.ALIGN.TOP_MID, 0, 0)
    header.set_style_bg_color(lv.color_black(), 0)
    header.set_style_border_width(0, 0)
    header.set_style_radius(0, 0)

    title = lv.label(header)
    title.set_text("LVGL TEXT INPUT")
    title.set_style_text_color(lv.color_white(), 0)
    title.align(lv.ALIGN.CENTER, 0, 0)

    ta = lv.textarea(scr)
    ta.set_size(WIDTH - 4, 37)
    ta.align(lv.ALIGN.TOP_MID, 0, 11)
    ta.set_style_bg_opa(lv.OPA.COVER, 0)
    ta.set_style_bg_color(lv.color_white(), 0)
    ta.set_style_border_color(lv.color_black(), 0)
    ta.set_style_border_width(1, 0)
    ta.set_style_text_color(lv.color_black(), 0)
    ta.set_text("")
    try:
        ta.set_one_line(False)
    except Exception:
        pass

    status = lv.label(scr)
    status.set_style_text_color(lv.color_black(), 0)
    status.align(lv.ALIGN.TOP_LEFT, 0, 50)

    hint = lv.label(scr)
    hint.set_style_text_color(lv.color_black(), 0)
    hint.align(lv.ALIGN.TOP_LEFT, 0, 57)
    hint.set_text("EXE=print  A/B=layer")

    return scr, ta, status


def main():
    ensure_st7565_ready()
    build_lvgl_display()

    keypad = MatrixKeypad(ROW_PINS, COL_PINS, KEYMAP_LAYERS)
    scr, ta, status = build_text_screen()

    state = {
        "text": "",
        "last_key": "-",
    }

    def sync_ui():
        status.set_text("L:%s K:%s" % (LAYER_NAMES[keypad.layer], tail(state["last_key"], 11)))
        ta.set_text(state["text"])

    group = lv.group_create()
    group.set_wrap(True)
    group.add_obj(ta)
    try:
        lv.group_focus_obj(ta)
    except Exception:
        pass

    indev = lv.indev_create()
    indev.set_type(lv.INDEV_TYPE.KEYPAD)
    indev.set_group(group)

    last_lv_key = lv.KEY.ENTER

    def keypad_read_cb(_indev, data):
        nonlocal last_lv_key
        keypad.tick()
        raw_key = keypad.get_stable_key()
        mapped = KEY_TO_LV.get(raw_key)
        if mapped is None:
            data.state = lv.INDEV_STATE.RELEASED
            data.key = last_lv_key
            return
        last_lv_key = mapped
        data.key = mapped
        data.state = lv.INDEV_STATE.PRESSED

    indev.set_read_cb(keypad_read_cb)

    sync_ui()
    lv.screen_load(scr)

    print("text_ready")
    print("type with keypad; alpha/beta switches layer")
    print("nav_b=backspace, ok/exe prints, back/home/on/AC exits")

    running = True
    while running:
        key = keypad.pop_event()
        if key is not None:
            state["last_key"] = key
            print("key", key, "layer", LAYER_NAMES[keypad.layer])

            if key in EXIT_KEYS:
                print("text_exit")
                running = False
            elif key == "alpha":
                keypad.toggle_layer("a")
                print("layer", LAYER_NAMES[keypad.layer])
            elif key == "beta":
                keypad.toggle_layer("b")
                print("layer", LAYER_NAMES[keypad.layer])
            elif key == "nav_b":
                if state["text"]:
                    state["text"] = state["text"][:-1]
            elif key in ("ok", "exe"):
                print("expr", state["text"])
            else:
                token = make_text_token(key)
                if token:
                    state["text"] += token
                    if len(state["text"]) > 256:
                        state["text"] = state["text"][-256:]

            sync_ui()

        lv.tick_inc(20)
        lv.timer_handler()
        time.sleep_ms(20)


main()
