import time
from machine import Pin, time_pulse_us

class HCSR04:
    def __init__(self, trigger_pin, echo_pin, echo_timeout_us=30000):
        self.trigger = Pin(trigger_pin, mode=Pin.OUT, pull=None)
        self.echo = Pin(echo_pin, mode=Pin.IN, pull=None)
        self.echo_timeout_us = echo_timeout_us
        self.trigger.value(0)
        time.sleep_ms(50)  # settle sensor

    def _send_pulse_and_wait(self):
        self.trigger.value(0)
        time.sleep_us(2)
        self.trigger.value(1)
        time.sleep_us(10)
        self.trigger.value(0)

        try:
            pulse_time = time_pulse_us(self.echo, 1, self.echo_timeout_us)
            return pulse_time
        except OSError as ex:
            raise OSError("Echo pulse not received")

    def distance_cm(self):
        duration = self._send_pulse_and_wait()
        return (duration / 2) / 29.1

    def distance_mm(self):
        duration = self._send_pulse_and_wait()
        return int((duration / 2) / 0.291)
