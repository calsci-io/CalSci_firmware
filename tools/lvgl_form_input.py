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

FIELD_KEYS = ("name", "value", "unit")
FOCUS_CODES = ("N", "V", "U", "S")

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
    return TOKEN_MAP.get(key, key)


def build_form_screen():
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

    head_label = lv.label(header)
    head_label.set_style_text_color(lv.color_white(), 0)
    head_label.align(lv.ALIGN.CENTER, 0, 0)
    head_label.set_text("FORM")

    y_positions = (10, 24, 38)
    labels = ("N:", "V:", "U:")
    textareas = []

    for i, y in enumerate(y_positions):
        lbl = lv.label(scr)
        lbl.set_text(labels[i])
        lbl.set_style_text_color(lv.color_black(), 0)
        lbl.align(lv.ALIGN.TOP_LEFT, 1, y + 2)

        ta = lv.textarea(scr)
        ta.set_size(106, 12)
        ta.align(lv.ALIGN.TOP_LEFT, 20, y)
        ta.set_style_bg_opa(lv.OPA.COVER, 0)
        ta.set_style_bg_color(lv.color_white(), 0)
        ta.set_style_text_color(lv.color_black(), 0)
        ta.set_style_border_color(lv.color_black(), 0)
        ta.set_style_border_width(1, 0)
        ta.set_style_border_width(2, lv.PART.MAIN | lv.STATE.FOCUSED)
        ta.set_text("")
        try:
            ta.set_one_line(True)
        except Exception:
            pass
        textareas.append(ta)

    submit_btn = lv.button(scr)
    submit_btn.set_size(54, 12)
    submit_btn.align(lv.ALIGN.TOP_MID, 0, 51)
    submit_btn.set_style_bg_opa(lv.OPA.COVER, lv.PART.MAIN)
    submit_btn.set_style_bg_color(lv.color_white(), lv.PART.MAIN)
    submit_btn.set_style_border_color(lv.color_black(), lv.PART.MAIN)
    submit_btn.set_style_border_width(1, lv.PART.MAIN)
    submit_btn.set_style_bg_color(lv.color_black(), lv.PART.MAIN | lv.STATE.FOCUSED)
    submit_btn.set_style_text_color(lv.color_white(), lv.PART.MAIN | lv.STATE.FOCUSED)

    submit_label = lv.label(submit_btn)
    submit_label.set_text("SUBMIT")
    submit_label.center()

    return scr, head_label, textareas, submit_btn, submit_label


def main():
    ensure_st7565_ready()
    build_lvgl_display()

    keypad = MatrixKeypad(ROW_PINS, COL_PINS, KEYMAP_LAYERS)
    scr, head_label, textareas, submit_btn, submit_label = build_form_screen()

    form_state = {"name": "", "value": "", "unit": ""}
    focus_objs = [textareas[0], textareas[1], textareas[2], submit_btn]
    focus_idx = 0
    status_code = "--"

    group = lv.group_create()
    group.set_wrap(True)
    for obj in focus_objs:
        group.add_obj(obj)

    def update_header():
        head_label.set_text("%s F:%s %s" % (LAYER_NAMES[keypad.layer], FOCUS_CODES[focus_idx], status_code))

    def apply_form_state():
        textareas[0].set_text(form_state["name"])
        textareas[1].set_text(form_state["value"])
        textareas[2].set_text(form_state["unit"])

    def set_focus(new_idx):
        nonlocal focus_idx
        focus_idx = new_idx % len(focus_objs)
        try:
            lv.group_focus_obj(focus_objs[focus_idx])
        except Exception:
            pass
        print("focus", FOCUS_CODES[focus_idx])

    def validate_form():
        name = form_state["name"].strip()
        value_raw = form_state["value"].strip()
        unit = form_state["unit"].strip()

        if not name:
            return False, "ER:N"
        if not value_raw:
            return False, "ER:V"
        try:
            value = float(value_raw.replace(",", "."))
        except Exception:
            return False, "ER:V"
        if not unit:
            return False, "ER:U"

        payload = {"name": name, "value": value, "unit": unit}
        print("FORM_SUBMIT", payload)
        return True, "OK"

    def submit_form():
        nonlocal status_code
        ok, code = validate_form()
        status_code = code
        if ok:
            submit_label.set_text("SAVED")
        else:
            submit_label.set_text("SUBMIT")
            print("form_invalid", code)

    cb_refs = []

    def submit_clicked_cb(_e):
        submit_form()

    cb_refs.append(submit_clicked_cb)
    submit_btn.add_event_cb(submit_clicked_cb, lv.EVENT.CLICKED, None)

    apply_form_state()
    set_focus(0)
    update_header()
    lv.screen_load(scr)

    print("form_ready")
    print("nav_u/nav_d=focus, alpha/beta=layer, nav_b=backspace")
    print("ok/exe=next or submit, back/home/on/AC=exit")

    running = True
    while running:
        key = keypad.pop_event()
        if key is not None:
            status_code = "--"
            submit_label.set_text("SUBMIT")
            print("key", key, "layer", LAYER_NAMES[keypad.layer])

            if key in EXIT_KEYS:
                print("form_exit")
                running = False
            elif key == "alpha":
                keypad.toggle_layer("a")
            elif key == "beta":
                keypad.toggle_layer("b")
            elif key == "nav_u":
                set_focus(focus_idx - 1)
            elif key == "nav_d":
                set_focus(focus_idx + 1)
            elif key == "nav_b":
                if focus_idx < 3 and form_state[FIELD_KEYS[focus_idx]]:
                    current = form_state[FIELD_KEYS[focus_idx]]
                    form_state[FIELD_KEYS[focus_idx]] = current[:-1]
                    apply_form_state()
            elif key in ("ok", "exe"):
                if focus_idx == 3:
                    submit_form()
                else:
                    set_focus(focus_idx + 1)
            else:
                token = make_text_token(key)
                if token and focus_idx < 3:
                    k = FIELD_KEYS[focus_idx]
                    form_state[k] += token
                    if len(form_state[k]) > 80:
                        form_state[k] = form_state[k][-80:]
                    apply_form_state()

            update_header()

        keypad.tick()
        lv.tick_inc(20)
        lv.timer_handler()
        time.sleep_ms(20)


main()
