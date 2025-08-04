import machine
# import time
import esp32
from tinydb import TinyDB, Query
from soft_watch_dog_timer import SoftWatchdog
db = TinyDB('db/settings.json')

def get_sleep_time():
    global db
    # db = TinyDB('db/settings.json')
    f=Query()
    res=db.search(f.feature=="sleep_timer")
    return res[0]["value"]

def test_deep_sleep_awake():
    opin = machine.Pin(14, machine.Pin.OUT, value=1, hold=True) # hold output level
    wakeup_pin = machine.Pin(8, mode=machine.Pin.IN, pull=machine.Pin.PULL_DOWN)
    esp32.wake_on_ext0(pin=wakeup_pin, level=esp32.WAKEUP_ANY_HIGH)
    esp32.gpio_deep_sleep_hold(True)
    machine.deepsleep()

swdt=SoftWatchdog(timeout_ms=get_sleep_time(), callback=test_deep_sleep_awake, timer_id=1)

def update_sleep_time(time):
    global swdt, db
    # db = TinyDB('db/settings.json')
    f=Query()
    db.update({'value': time}, f.feature == "sleep_timer")
    swdt.update_time(timeout_ms=time)

def keypad_normal():
    opin = machine.Pin(21, machine.Pin.OUT, value=1, hold=False)

