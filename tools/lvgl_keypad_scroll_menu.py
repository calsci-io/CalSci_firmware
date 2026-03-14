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

MENU_ITEMS = [
    "Calculator",
    "Graph",
    "Algebra",
    "Matrix",
    "Statistics",
    "Unit Convert",
    "Settings",
    "Bluetooth",
    "WiFi",
    "About",
]

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


def make_text_token(key):
    if key in CONTROL_KEYS:
        return None
    if key == "off":
        return " "
    if key == "tab":
        return "    "
    return key


def tail(text, limit):
    if len(text) <= limit:
        return text
    return text[-limit:]


def build_menu_screen(items):
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
    title.set_text("LVGL MENU + KEYPAD")
    title.set_style_text_color(lv.color_white(), 0)
    title.align(lv.ALIGN.CENTER, 0, 0)

    lst = lv.list(scr)
    lst.set_size(WIDTH, 34)
    lst.align(lv.ALIGN.TOP_MID, 0, 10)
    lst.set_scrollbar_mode(lv.SCROLLBAR_MODE.OFF)
    lst.set_style_bg_opa(lv.OPA.COVER, 0)
    lst.set_style_bg_color(lv.color_white(), 0)
    lst.set_style_border_width(1, 0)
    lst.set_style_border_color(lv.color_black(), 0)
    lst.set_style_pad_row(1, 0)
    lst.set_style_pad_all(1, 0)

    status = lv.label(scr)
    status.set_style_text_color(lv.color_black(), 0)
    status.align(lv.ALIGN.TOP_LEFT, 0, 46)

    text_line = lv.label(scr)
    text_line.set_style_text_color(lv.color_black(), 0)
    text_line.align(lv.ALIGN.TOP_LEFT, 0, 54)

    buttons = []
    for item in items:
        btn = lst.add_button(lv.SYMBOL.RIGHT, item)
        btn.set_style_bg_opa(lv.OPA.COVER, lv.PART.MAIN)
        btn.set_style_bg_color(lv.color_white(), lv.PART.MAIN)
        btn.set_style_text_color(lv.color_black(), lv.PART.MAIN)
        btn.set_style_bg_color(lv.color_black(), lv.PART.MAIN | lv.STATE.FOCUSED)
        btn.set_style_text_color(lv.color_white(), lv.PART.MAIN | lv.STATE.FOCUSED)
        btn.set_style_bg_color(lv.color_black(), lv.PART.MAIN | lv.STATE.PRESSED)
        btn.set_style_text_color(lv.color_white(), lv.PART.MAIN | lv.STATE.PRESSED)
        buttons.append(btn)

    return scr, buttons, status, text_line


def main():
    ensure_st7565_ready()
    build_lvgl_display()

    keypad = MatrixKeypad(ROW_PINS, COL_PINS, KEYMAP_LAYERS)
    scr, buttons, status_label, input_label = build_menu_screen(MENU_ITEMS)

    app_state = {
        "input": "",
        "last_key": "-",
    }

    def update_labels():
        layer_name = LAYER_NAMES[keypad.layer]
        status_label.set_text("L:%s K:%s" % (layer_name, tail(app_state["last_key"], 12)))
        input_label.set_text("IN:%s" % tail(app_state["input"], 18))

    callback_refs = []

    def make_menu_cb(item_name):
        def menu_cb(e):
            code = e.get_code()
            if code == lv.EVENT.FOCUSED:
                print("selected", item_name)
            elif code == lv.EVENT.CLICKED:
                print("open", item_name)
        return menu_cb

    group = lv.group_create()
    group.set_wrap(True)
    for i, btn in enumerate(buttons):
        cb = make_menu_cb(MENU_ITEMS[i])
        callback_refs.append(cb)
        btn.add_event_cb(cb, lv.EVENT.ALL, None)
        group.add_obj(btn)

    try:
        lv.group_focus_obj(buttons[0])
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

    update_labels()
    lv.screen_load(scr)

    print("menu_ready")
    print("lv indev active: nav_u/nav_d/nav_l/nav_r + ok/exe")
    print("alpha/beta toggles layer, nav_b is backspace, back/home/on/AC exits")

    running = True
    while running:
        key = keypad.pop_event()
        if key is not None:
            app_state["last_key"] = key
            print("key", key, "layer", LAYER_NAMES[keypad.layer])

            if key in EXIT_KEYS:
                print("menu_exit")
                running = False
            elif key == "alpha":
                keypad.toggle_layer("a")
                print("layer", LAYER_NAMES[keypad.layer])
            elif key == "beta":
                keypad.toggle_layer("b")
                print("layer", LAYER_NAMES[keypad.layer])
            elif key == "nav_b":
                if app_state["input"]:
                    app_state["input"] = app_state["input"][:-1]
            else:
                token = make_text_token(key)
                if token:
                    app_state["input"] += token
                    if len(app_state["input"]) > 80:
                        app_state["input"] = app_state["input"][-80:]

            update_labels()

        lv.tick_inc(20)
        lv.timer_handler()
        time.sleep_ms(20)


main()
