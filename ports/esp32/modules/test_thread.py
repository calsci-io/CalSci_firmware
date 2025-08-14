import _thread
import time
import machine
# Shared flag to control threads
running = True
import network
import espnow # type: ignore

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

def espnow_test():
    global espnow_state
    print("mac_address = ",machine.unique_id())
    sta = network.WLAN(network.STA_IF)
    sta.active(True)
    e = espnow.ESPNow()
    e.active(True)
    while espnow_state:
        host, msg = e.recv()
        if msg:
            print("mac = ",host, "message = ",msg)
            if msg == b'end':
                break

def run_espnow_message():
    global espnow_state
    espnow_state = True
    _thread.start_new_thread(espnow_test, ())
    print("chat started")

def end_espnow_task():
    global espnow_state
    espnow_state = False
    print("chat closed")

