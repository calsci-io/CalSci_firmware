import builtins
import machine
import sys
import time as _pytime

try:
    import _thread as _hyb_thread
except Exception:
    _hyb_thread = None

try:
    import ujson as _json
except Exception:
    import json as _json

try:
    import ubinascii as _binascii
except Exception:
    import binascii as _binascii

try:
    import utime as _utime
except Exception:
    _utime = None

try:
    import hybrid_sim as _hyb_mod
except Exception:
    _hyb_mod = None

HYBRID_BAUDRATE = 115200
_HYB_GLOBAL_DEBOUNCE_SEC = 0.150
_HYB_GRAPH_DEBOUNCE_SEC = 0.001
_HYB_FRAME_PREFIX = "{{CALSCI_HYB:"
_HYB_FRAME_SUFFIX = "}}"


class _HybridKeypadProxy:
    def __init__(self, keypad, loop):
        object.__setattr__(self, "_keypad", keypad)
        object.__setattr__(self, "_loop", loop)
        object.__setattr__(self, "_overrides", {})
        object.__setattr__(self, "_hyb_bridge_proxy", True)

    def __getattr__(self, name):
        overrides = object.__getattribute__(self, "_overrides")
        if name in overrides:
            return overrides[name]
        if name == "keypad_loop":
            return object.__getattribute__(self, "_loop")
        return getattr(self._keypad, name)

    def __setattr__(self, name, value):
        if name in ("_keypad", "_loop", "_overrides", "_hyb_bridge_proxy"):
            object.__setattr__(self, name, value)
            return
        if name == "keypad_loop":
            object.__getattribute__(self, "_overrides")[name] = value
            return
        try:
            setattr(self._keypad, name, value)
        except Exception:
            object.__getattribute__(self, "_overrides")[name] = value


class _HybridBridge:
    def __init__(self, data_bucket, typer, menu, form, text, nav, runtime, namespace=None):
        self.data_bucket = data_bucket
        self.typer = typer
        self.menu = menu
        self.form = form
        self.text = text
        self.nav = nav
        self.runtime = runtime
        self.namespace = namespace
        self.module = _hyb_mod
        self.key_queue = []
        self.local_keypad_loop = None
        self.compat_state = {
            "mode": "local",
            "hybrid_requested": False,
            "stream_enabled": False,
            "protocol_enabled": False,
            "accept_protocol_stdin": False,
            "keypad_mode": "local",
        }
        self.tx_lock = None
        if _hyb_thread is not None and hasattr(_hyb_thread, "allocate_lock"):
            try:
                self.tx_lock = _hyb_thread.allocate_lock()
            except Exception:
                self.tx_lock = None

    def _export(self, name, value):
        try:
            setattr(builtins, name, value)
        except Exception:
            pass
        if self.namespace is not None:
            try:
                self.namespace[name] = value
            except Exception:
                pass

    def _norm_delay(self, value, fallback):
        try:
            parsed = float(value)
            if parsed > 0:
                return parsed
        except Exception:
            pass
        return float(fallback)

    def delay_set_global(self, sec):
        sec = self._norm_delay(sec, self.data_bucket.get("hyb_delay_global_sec", _HYB_GLOBAL_DEBOUNCE_SEC))
        self.data_bucket["hyb_delay_global_sec"] = sec
        self.data_bucket["hyb_global_debounce_sec"] = sec
        return sec

    def delay_set_local(self, name, sec):
        key = str(name).strip().lower()
        if not key:
            return None
        if not isinstance(self.data_bucket.get("hyb_delay_local_map"), dict):
            self.data_bucket["hyb_delay_local_map"] = {}
        sec = self._norm_delay(sec, self.data_bucket.get("hyb_delay_global_sec", _HYB_GLOBAL_DEBOUNCE_SEC))
        self.data_bucket["hyb_delay_local_map"][key] = sec
        if key == "graph":
            self.data_bucket["hyb_graph_fast_debounce_sec"] = sec
        return sec

    def delay_use_global(self):
        sec = self._norm_delay(
            self.data_bucket.get("hyb_delay_global_sec", _HYB_GLOBAL_DEBOUNCE_SEC),
            _HYB_GLOBAL_DEBOUNCE_SEC,
        )
        self.typer.debounce_delay_time = sec
        self.data_bucket["hyb_delay_active"] = "global"
        self.data_bucket["hyb_delay_active_sec"] = sec
        return sec

    def delay_use_local(self, name):
        key = str(name).strip().lower()
        if not key:
            return self.delay_use_global()
        local_map = self.data_bucket.get("hyb_delay_local_map")
        if not isinstance(local_map, dict):
            return self.delay_use_global()
        sec = self._norm_delay(
            local_map.get(key, self.data_bucket.get("hyb_delay_global_sec", _HYB_GLOBAL_DEBOUNCE_SEC)),
            self.data_bucket.get("hyb_delay_global_sec", _HYB_GLOBAL_DEBOUNCE_SEC),
        )
        self.typer.debounce_delay_time = sec
        self.data_bucket["hyb_delay_active"] = key
        self.data_bucket["hyb_delay_active_sec"] = sec
        return sec

    def _apply_compat_state(
        self,
        mode=None,
        hybrid_requested=None,
        stream_enabled=None,
        protocol_enabled=None,
        accept_protocol_stdin=None,
        keypad_mode=None,
    ):
        if mode is not None:
            self.compat_state["mode"] = str(mode)
        if hybrid_requested is not None:
            self.compat_state["hybrid_requested"] = bool(hybrid_requested)
        if stream_enabled is not None:
            self.compat_state["stream_enabled"] = bool(stream_enabled)
        if protocol_enabled is not None:
            self.compat_state["protocol_enabled"] = bool(protocol_enabled)
        if accept_protocol_stdin is not None:
            self.compat_state["accept_protocol_stdin"] = bool(accept_protocol_stdin)
        if keypad_mode is not None:
            self.compat_state["keypad_mode"] = str(keypad_mode)

        self.data_bucket["hyb_mode"] = self.compat_state["mode"]
        self.data_bucket["hyb_requested"] = self.compat_state["hybrid_requested"]
        self.data_bucket["hyb_stream_enabled"] = self.compat_state["stream_enabled"]
        self.data_bucket["hyb_protocol_enabled"] = self.compat_state["protocol_enabled"]
        self.data_bucket["hyb_accept_protocol_stdin"] = self.compat_state["accept_protocol_stdin"]
        self.data_bucket["hyb_keypad_mode"] = self.compat_state["keypad_mode"]
        return dict(self.compat_state)

    def _sleep_ms(self, ms):
        if _utime is not None and hasattr(_utime, "sleep_ms"):
            _utime.sleep_ms(ms)
        else:
            _pytime.sleep(ms / 1000.0)

    def _frame_text(self, text):
        payload = str(text)
        if payload.endswith("}"):
            payload += " "
        return _HYB_FRAME_PREFIX + payload + _HYB_FRAME_SUFFIX

    def _write_line(self, text):
        framed = self._frame_text(text) + "\n"
        wrote = False
        try:
            if self.tx_lock is not None:
                self.tx_lock.acquire()
            start = 0
            while start < len(framed):
                sys.stdout.write(framed[start : start + 96])
                start += 96
            wrote = True
            try:
                flush = getattr(sys.stdout, "flush", None)
                if flush is not None:
                    flush()
            except Exception:
                pass
        except Exception:
            if not wrote:
                try:
                    print(framed)
                except Exception:
                    pass
        finally:
            if self.tx_lock is not None:
                try:
                    self.tx_lock.release()
                except Exception:
                    pass

    def _clean_line(self, text):
        try:
            return str(text).replace("𖤓", "_")
        except Exception:
            return ""

    def _nav_state(self):
        try:
            if self.nav is not None and hasattr(self.nav, "current_state"):
                return str(self.nav.current_state())
        except Exception:
            pass
        return ""

    def _menu_lines(self):
        try:
            menu_obj = self.menu
            if menu_obj is None or not hasattr(menu_obj, "buffer"):
                return []
            buf = menu_obj.buffer()
            if not isinstance(buf, (list, tuple)) or not buf:
                return []
            if all(self._clean_line(item).startswith("label_") for item in buf):
                return []
            cur = -1
            if hasattr(menu_obj, "cursor"):
                try:
                    cur = int(menu_obj.cursor())
                except Exception:
                    cur = -1
            lines = []
            for index, row in enumerate(buf):
                prefix = ">" if index == cur else " "
                lines.append(prefix + self._clean_line(row))
            return lines[:7]
        except Exception:
            return []

    def _form_lines(self):
        try:
            form_obj = self.form
            if form_obj is None or not hasattr(form_obj, "buffer"):
                return []
            buf = form_obj.buffer()
            if not isinstance(buf, (list, tuple)) or not buf:
                return []
            if all(self._clean_line(item).startswith("label_") for item in buf):
                return []
            cur = -1
            if hasattr(form_obj, "cursor"):
                try:
                    cur = int(form_obj.cursor())
                except Exception:
                    cur = -1
            inp_list = {}
            if hasattr(form_obj, "inp_list"):
                try:
                    inp_list = form_obj.inp_list() or {}
                except Exception:
                    inp_list = {}
            inp_start = 0
            if hasattr(form_obj, "inp_display_position"):
                try:
                    inp_start = int(form_obj.inp_display_position())
                except Exception:
                    inp_start = 0
            inp_cols = 19
            if hasattr(form_obj, "inp_cols"):
                try:
                    inp_cols = int(form_obj.inp_cols())
                except Exception:
                    inp_cols = 19
            lines = []
            for index, row in enumerate(buf):
                name = self._clean_line(row)
                if name.startswith("inp_"):
                    value = self._clean_line(inp_list.get(name, ""))
                    line = "=>" + value[inp_start : inp_start + inp_cols]
                else:
                    line = name
                prefix = ">" if index == cur and not name.startswith("inp_") else " "
                lines.append(prefix + line)
            return lines[:7]
        except Exception:
            return []

    def _text_lines(self):
        try:
            text_obj = self.text
            if text_obj is None or not hasattr(text_obj, "buffer"):
                return []
            buf = text_obj.buffer()
            if not isinstance(buf, (list, tuple)) or not buf:
                return []
            return [self._clean_line(row) for row in buf[:7]]
        except Exception:
            return []

    def _lines_snapshot(self):
        for producer in (self._text_lines, self._form_lines, self._menu_lines):
            lines = producer()
            if lines:
                return lines
        return []

    def _fb_to_b64(self, raw_fb):
        if raw_fb is None:
            return ""
        try:
            if isinstance(raw_fb, memoryview):
                raw_fb = raw_fb.tobytes()
            elif not isinstance(raw_fb, (bytes, bytearray)):
                raw_fb = bytes(raw_fb)
            encoded = _binascii.b2a_base64(raw_fb)
            if isinstance(encoded, bytes):
                return encoded.decode().strip()
            return str(encoded).strip()
        except Exception:
            return ""

    def _capture_mode(self):
        if self.module is None or not hasattr(self.module, "mode"):
            return False
        try:
            return bool(self.module.mode())
        except Exception:
            return False

    def _capture_enabled(self):
        if self.module is None or not hasattr(self.module, "enabled"):
            return False
        try:
            return bool(self.module.enabled())
        except Exception:
            return False

    def _state_payload(self, state, include_fb=False):
        payload = {}
        if isinstance(state, dict):
            try:
                payload.update(state)
            except Exception:
                payload = {}
        try:
            payload["frame_id"] = int(payload.get("frame_id", -1))
        except Exception:
            payload["frame_id"] = -1
        payload["fb_seq"] = payload["frame_id"] & 0x7F if payload["frame_id"] >= 0 else 0
        payload["mode"] = bool(payload.get("mode", self._capture_mode()))
        payload["capture_enabled"] = bool(payload.get("capture_enabled", self._capture_enabled()))
        payload["fb_seen"] = bool(payload.get("fb_seen", payload["capture_enabled"]))
        payload["nav"] = self._nav_state()
        payload["lines"] = self._lines_snapshot()
        raw_fb = payload.pop("fb", None)
        if include_fb or raw_fb is not None:
            if raw_fb is None and self.module is not None and hasattr(self.module, "read_fb"):
                try:
                    raw_fb = self.module.read_fb()
                except Exception:
                    raw_fb = None
            fb_b64 = self._fb_to_b64(raw_fb)
            if fb_b64:
                payload["fb"] = fb_b64
                payload["fb_full"] = True
        return payload

    def _emit_state(self, last_frame=-1, force_full=False):
        if self.module is None or not hasattr(self.module, "status") or not hasattr(self.module, "poll_state"):
            self._write_line("HYBRID_SYNC_ERR:MODULE_MISSING")
            return
        try:
            last_frame = int(last_frame)
        except Exception:
            last_frame = -1
        try:
            if force_full:
                state = self.module.status()
            else:
                state = self.module.poll_state(last_frame)
            payload = self._state_payload(state, include_fb=force_full)
            self._write_line("STATE:" + _json.dumps(payload))
        except Exception as exc:
            self._write_line("HYBRID_SYNC_ERR:%s" % exc)

    def _ping(self, token=""):
        self._write_line("ECHO:%s" % str(token).strip())

    def _mode(self, enabled=None):
        if self.module is None or not hasattr(self.module, "mode"):
            if enabled is not None:
                self._write_line("HYBRID_MODE_ERR:MODULE_MISSING")
                return
            return False
        if enabled is None:
            try:
                return bool(self.module.mode())
            except Exception:
                return False
        try:
            self.module.mode(bool(enabled))
        except Exception as exc:
            self._write_line("HYBRID_MODE_ERR:%s" % exc)

    def _status(self):
        if self.module is None or not hasattr(self.module, "status"):
            self._write_line("HYBRID_STATUS_ERR:MODULE_MISSING")
            return
        try:
            payload = self._state_payload(self.module.status(), include_fb=False)
            self._write_line("STATE:" + _json.dumps(payload))
        except Exception as exc:
            self._write_line("HYBRID_STATUS_ERR:%s" % exc)

    def _queue_key(self, col, row):
        cols = getattr(getattr(self.typer, "keypad", None), "cols", ())
        rows = getattr(getattr(self.typer, "keypad", None), "rows", ())
        max_col = len(cols) - 1 if cols else 4
        max_row = len(rows) - 1 if rows else 9
        try:
            col = int(col)
            row = int(row)
            if not (0 <= col <= max_col and 0 <= row <= max_row):
                return False
            self.key_queue.append((col, row))
            if len(self.key_queue) > 1:
                del self.key_queue[:-1]
            return True
        except Exception:
            return False

    def _key(self, col, row):
        if self._queue_key(col, row):
            self._write_line("HYBRID_KEY_OK:%d,%d" % (int(col), int(row)))
            return
        self._write_line("HYBRID_KEY_ERR:RANGE")

    def _key_enqueue(self, col, row):
        return self._queue_key(col, row)

    def _poll_state(self, last_frame=-1):
        self._emit_state(last_frame, False)

    def _sync_full(self):
        self._emit_state(-1, True)

    def _emit_hybrid_config(self):
        try:
            debounce_ms = int(float(getattr(self.typer, "debounce_delay_time", 0.100)) * 1000)
            if debounce_ms > 0:
                self._write_line("HYB_KEY_DEB_MS:%d" % debounce_ms)
        except Exception:
            pass
        try:
            graph_sec = self.data_bucket.get("hyb_graph_fast_debounce_sec", None)
            if graph_sec is None:
                graph_sec = self.data_bucket.get("hyb_delay_local_map", {}).get("graph", _HYB_GRAPH_DEBOUNCE_SEC)
            graph_ms = int(float(graph_sec) * 1000)
            if graph_ms > 0:
                self._write_line("HYB_GRAPH_FAST_MS:%d" % graph_ms)
        except Exception:
            pass

    def stream_set_enabled(self, enabled):
        enabled = bool(enabled)
        self._apply_compat_state(stream_enabled=enabled)
        return enabled

    def stream_is_enabled(self):
        return bool(self.compat_state.get("stream_enabled"))

    def bridge_status(self):
        return {
            "mode": self.compat_state.get("mode", "local"),
            "hybrid_requested": bool(self.compat_state.get("hybrid_requested", False)),
            "stream_enabled": bool(self.compat_state.get("stream_enabled", False)),
            "protocol_enabled": bool(self.compat_state.get("protocol_enabled", False)),
            "accept_protocol_stdin": bool(self.compat_state.get("accept_protocol_stdin", False)),
            "delay_active": self.data_bucket.get("hyb_delay_active", "global"),
            "delay_active_sec": self.data_bucket.get("hyb_delay_active_sec", self.data_bucket.get("hyb_delay_global_sec")),
        }

    def enter_local_mode(self):
        self._apply_compat_state(
            mode="local",
            hybrid_requested=False,
            stream_enabled=False,
            protocol_enabled=False,
            accept_protocol_stdin=False,
            keypad_mode="local",
        )
        self._mode(False)
        self._write_line("CTRL:HYBRID_DISABLED:OK")
        return True

    def enter_command_mode(self):
        self._apply_compat_state(
            mode="command",
            hybrid_requested=self.compat_state.get("hybrid_requested", False),
            stream_enabled=False,
            protocol_enabled=False,
            accept_protocol_stdin=False,
            keypad_mode="local",
        )
        self._mode(False)
        self._write_line("CTRL:COMMAND:OK")
        return True

    def enter_exec_mode(self):
        self._apply_compat_state(
            mode="exec",
            hybrid_requested=self.compat_state.get("hybrid_requested", False),
            stream_enabled=False,
            protocol_enabled=False,
            accept_protocol_stdin=False,
            keypad_mode="local",
        )
        self._mode(False)
        self._write_line("CTRL:HYBRID_OFF:OK")
        return True

    def enter_hybrid_mode(self, stream_enabled=False):
        self._apply_compat_state(
            mode="hybrid",
            hybrid_requested=True,
            stream_enabled=bool(stream_enabled),
            protocol_enabled=False,
            accept_protocol_stdin=False,
            keypad_mode="hybrid",
        )
        self.delay_use_global()
        self._mode(True)
        self._write_line("CTRL:HYBRID_ON:OK")
        if stream_enabled:
            self._sync_full()
        return True

    def stream_updated_buffer(self):
        while True:
            if self.stream_is_enabled():
                self._sleep_ms(100)
            else:
                self._sleep_ms(250)

    def _release_rows(self, rows):
        for row_pin in rows:
            try:
                machine.Pin(row_pin, machine.Pin.OUT).value(1)
            except Exception:
                pass

    def _wait_while_keypad_blocked(self, rows):
        if self.runtime is None:
            return False
        if not hasattr(self.runtime, "calsci_keypad_blocked") or not hasattr(self.runtime, "wait_if_repl_busy"):
            return False
        if not self.runtime.calsci_keypad_blocked():
            return False
        if self.key_queue:
            del self.key_queue[:]
        self.runtime.wait_if_repl_busy(lambda: self._release_rows(rows))
        return True

    def keypad_loop(self):
        rows = getattr(self.typer.keypad, "rows", [])
        cols = getattr(self.typer.keypad, "cols", [])
        while True:
            if self._wait_while_keypad_blocked(rows):
                continue
            if self.key_queue:
                return self.key_queue.pop(0)
            if not rows or not cols:
                if self.local_keypad_loop is not None and self.local_keypad_loop is not self.keypad_loop:
                    try:
                        return self.local_keypad_loop()
                    except Exception:
                        pass
                self._sleep_ms(10)
                continue
            for row in range(len(rows)):
                if self._wait_while_keypad_blocked(rows):
                    break
                machine.Pin(rows[row], machine.Pin.OUT).value(0)
                hit = None
                for col in range(len(cols)):
                    if self._wait_while_keypad_blocked(rows):
                        hit = None
                        break
                    if machine.Pin(cols[col], machine.Pin.IN, machine.Pin.PULL_UP).value() == 0:
                        hit = (col, row)
                        break
                machine.Pin(rows[row], machine.Pin.OUT).value(1)
                if self.runtime is not None and hasattr(self.runtime, "calsci_keypad_blocked"):
                    if self.runtime.calsci_keypad_blocked():
                        break
                if hit is not None:
                    return hit
            self._sleep_ms(5)

    def _install_keypad_hook(self):
        keypad = getattr(self.typer, "keypad", None)
        if keypad is None:
            return
        current_loop = getattr(keypad, "keypad_loop", None)
        if self.local_keypad_loop is None and callable(current_loop) and current_loop is not self.keypad_loop:
            self.local_keypad_loop = current_loop
        try:
            keypad.keypad_loop = self.keypad_loop
            return
        except Exception:
            pass
        if getattr(keypad, "_hyb_bridge_proxy", False):
            object.__setattr__(keypad, "_loop", self.keypad_loop)
            return
        self.typer.keypad = _HybridKeypadProxy(keypad, self.keypad_loop)

    def install(self):
        if not isinstance(self.data_bucket.get("hyb_delay_local_map"), dict):
            self.data_bucket["hyb_delay_local_map"] = {}

        self.data_bucket["hyb_delay_global_sec"] = self._norm_delay(
            self.data_bucket.get("hyb_delay_global_sec", _HYB_GLOBAL_DEBOUNCE_SEC),
            _HYB_GLOBAL_DEBOUNCE_SEC,
        )
        self.data_bucket["hyb_delay_local_map"]["graph"] = self._norm_delay(
            self.data_bucket["hyb_delay_local_map"].get("graph", _HYB_GRAPH_DEBOUNCE_SEC),
            _HYB_GRAPH_DEBOUNCE_SEC,
        )
        self.data_bucket["hyb_global_debounce_sec"] = self.data_bucket["hyb_delay_global_sec"]
        self.data_bucket["hyb_graph_fast_debounce_sec"] = self.data_bucket["hyb_delay_local_map"]["graph"]

        self.data_bucket["hyb_stream_enabled"] = False
        self.data_bucket["hyb_protocol_enabled"] = False
        self.data_bucket["hyb_accept_protocol_stdin"] = False
        self.data_bucket["hyb_mode"] = "local"
        self.data_bucket["hyb_requested"] = False
        self.data_bucket["hyb_keypad_mode"] = "local"

        self._export("hyb_delay_set_global", self.delay_set_global)
        self._export("hyb_delay_set_local", self.delay_set_local)
        self._export("hyb_delay_use_global", self.delay_use_global)
        self._export("hyb_delay_use_local", self.delay_use_local)
        self.delay_use_global()

        self._export("hyb_stream_set_enabled", self.stream_set_enabled)
        self._export("hyb_stream_is_enabled", self.stream_is_enabled)
        self._export("hyb_bridge_status", self.bridge_status)
        self._export("hyb_enter_local_mode", self.enter_local_mode)
        self._export("hyb_enter_command_mode", self.enter_command_mode)
        self._export("hyb_enter_exec_mode", self.enter_exec_mode)
        self._export("hyb_enter_hybrid_mode", self.enter_hybrid_mode)
        self._export("hyb_keypad_input", self.keypad_loop)
        self._export("hyb_stream_updated_buffer", self.stream_updated_buffer)

        self._export("_hyb_ping", self._ping)
        self._export("_hyb_mode", self._mode)
        self._export("_hyb_status", self._status)
        self._export("_hyb_key", self._key)
        self._export("_hyb_key_enqueue", self._key_enqueue)
        self._export("_hyb_poll_state", self._poll_state)
        self._export("_hyb_sync_full", self._sync_full)
        self._export("_hyb_emit_hybrid_config", self._emit_hybrid_config)

        self._install_keypad_hook()
        self._export("calsci_hybrid_bridge", self)

        if self.module is not None:
            try:
                if hasattr(self.module, "enable"):
                    self.module.enable(True)
            except Exception:
                pass
            try:
                if hasattr(self.module, "mode") and bool(self.module.mode()):
                    self.module.mode(False)
            except Exception:
                pass

        self._write_line("HYBRID_PROTO:POLL_V1")
        self._emit_hybrid_config()
        self._write_line("HYBRID_READY")
        self._write_line("HYBRID_BAUD:%d" % HYBRID_BAUDRATE)
        return self


def install(data_bucket, typer, menu=None, form=None, text=None, nav=None, runtime=None, namespace=None):
    bridge = _HybridBridge(
        data_bucket=data_bucket,
        typer=typer,
        menu=menu,
        form=form,
        text=text,
        nav=nav,
        runtime=runtime,
        namespace=namespace,
    )
    return bridge.install()


__all__ = ("HYBRID_BAUDRATE", "install")
