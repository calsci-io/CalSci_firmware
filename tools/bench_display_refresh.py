import gc
import time

import st7565 as display
import tools

WIDTH = 128
PAGES = 8
BUF_SIZE = WIDTH * PAGES
DEFAULT_THRESHOLD = 200


def ensure_display_ready():
    probe = bytearray(BUF_SIZE)
    try:
        display.graphics(probe)
        return
    except Exception:
        pass

    # Known pin map for this board setup.
    display.init(9, 11, 10, 13, 12)
    display.graphics(probe)


def make_mutator(changed_bytes):
    if changed_bytes == 0:
        return lambda _buf, _i: None

    # Fixed pattern keeps test deterministic while spreading writes.
    def mutate(buf, frame):
        base = (frame * 37) % BUF_SIZE
        for k in range(changed_bytes):
            idx = (base + k * 131) % BUF_SIZE
            buf[idx] ^= 0xFF

    return mutate


def run_case(label, upload_fn, mutate_fn, frames=180, warmup=24):
    buf = bytearray(BUF_SIZE)
    upload_fn(buf)  # initial frame

    for i in range(warmup):
        mutate_fn(buf, i)
        upload_fn(buf)

    gc.collect()
    start = time.ticks_us()
    for i in range(frames):
        mutate_fn(buf, i + warmup)
        upload_fn(buf)
    total_us = time.ticks_diff(time.ticks_us(), start)
    avg_us = total_us / frames
    fps = 1000000.0 / avg_us if avg_us > 0 else 0.0
    return total_us, avg_us, fps


def compare():
    ensure_display_ready()
    print("Display benchmark start")
    print("resolution={}x{}, threshold={}".format(WIDTH, PAGES * 8, DEFAULT_THRESHOLD))

    base_graphics = display.graphics
    wrapped_graphics = tools.refresh(base_graphics, pixels_changed=DEFAULT_THRESHOLD)

    cases = [
        ("no_change", 0),
        ("small_8px", 1),      # 1 byte => 8 pixels
        ("small_64px", 8),     # 8 bytes => 64 pixels
        ("near_200px", 24),    # 24 bytes => 192 pixels
        ("over_200px", 32),    # 32 bytes => 256 pixels
        ("heavy_1024px", 128), # 128 bytes => 1024 pixels
    ]

    print("")
    print("case,old_avg_us,new_avg_us,speedup_x,old_fps,new_fps")
    for name, changed_bytes in cases:
        mutator = make_mutator(changed_bytes)
        old_total, old_avg, old_fps = run_case(name, base_graphics, mutator)
        wrapped_graphics.reset()
        new_total, new_avg, new_fps = run_case(name, wrapped_graphics, mutator)

        speedup = old_avg / new_avg if new_avg > 0 else 0.0
        print(
            "{},{:.1f},{:.1f},{:.2f},{:.2f},{:.2f}".format(
                name, old_avg, new_avg, speedup, old_fps, new_fps
            )
        )

        # Keep compiler from optimizing away totals and provide quick sanity hook.
        _ = old_total + new_total

    print("Display benchmark done")


compare()
