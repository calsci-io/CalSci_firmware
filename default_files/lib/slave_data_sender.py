import machine
import time
import _thread
import espnow
import network
from max31865 import MAX31865
# Assuming you are using an ESP32 and pin 34 for analog input
sensor = MAX31865(clk=14, miso=27, mosi=26, cs=25, rtd_type="3-wire")

adc_pin = machine.Pin(32)
adc = machine.ADC(adc_pin)

# Set the attenuation (optional)
adc.atten(machine.ADC.ATTN_11DB)

# Set the width (resolution) (optional)
adc.width(machine.ADC.WIDTH_12BIT)

def read_analog():
    global sensor
    # while True:
    # Read the analog value
    # adc_value = adc.read_u16()

    # # Convert to voltage (assuming 3.3V reference)
    # voltage = adc_value * (3.3 / 65535)

    # # Print the value
    # print("ADC Value:", adc_value, "Voltage:", round(voltage, 2), "V")
    temperature = sensor.read_temperature()
    print("temperature = ", temperature)
    return temperature
    # time.sleep(0.1)

# import _thread
# import time
# import machine
# Shared flag to control threads
running = True
# import network
# import espnow

espnow_state=True

def task1():
    global running
    while running:
        print("Task 1 running")
        time.sleep(1)
    print("Task 1 stopped")

def task2():
    global running
    try:
        while True:
            print("Task 2 running")
            time.sleep(2)
    except KeyboardInterrupt:
        print("Ctrl+C detected, stopping...")
        running = False  # Signal other thread to stop #from test_thread import running
        time.sleep(1)    # Give time for thread to exit cleanly

def run_thread():
    # Start task1 in background thread
    _thread.start_new_thread(task1, ())

    # Run task2 in main thread
    # task2()
sta = network.WLAN(network.STA_IF)
sta.active(True)
e = espnow.ESPNow()
e.active(True)
peer = b'\x98\xa3\x16\xd1!\xf8'
e.add_peer(peer)

def espnow_test():
    global espnow_state
    print("mac_address = ",machine.unique_id())

    while espnow_state:
        e.send(str(read_analog()))
        time.sleep(1)

def run_espnow_message():
    global espnow_state
    espnow_state = True
    _thread.start_new_thread(espnow_test, ())
    print("chat started")

def end_espnow_task():
    global espnow_state
    espnow_state = False
    print("chat closed")

run_espnow_message()
