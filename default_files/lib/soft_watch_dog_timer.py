from machine import Timer
import time

class SoftWatchdog:
    def __init__(self, timeout_ms, callback, timer_id):
        self.timeout_ms = timeout_ms
        self.callback = callback
        self.timer = Timer(timer_id)
        self._start_timer()

    def _start_timer(self):
        self.timer.init(mode=Timer.ONE_SHOT, period=self.timeout_ms, callback=self._timeout)

    def _timeout(self, t):
        if self.callback:
            self.callback()

    def feed(self):
        self.timer.deinit()
        self._start_timer()

    def stop(self):
        self.timer.deinit()
    
    def update_time(self, timeout_ms):
        self.timeout_ms = timeout_ms
        self.feed()

# Usage example
# def my_watchdog_callback():
#     print("Watchdog expired! Taking custom action...")

# wdt = SoftWatchdog(timeout_ms=5000, callback=my_watchdog_callback)

# # Simulate a loop feeding the watchdog
# try:
#     while True:
#         print("Running normally...")
#         time.sleep(2)
#         wdt.feed()  # Feed the watchdog every 2 seconds
# except KeyboardInterrupt:
#     wdt.stop()
#     print("Stopped")
