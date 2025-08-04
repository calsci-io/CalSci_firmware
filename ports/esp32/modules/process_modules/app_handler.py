from data_modules.object_handler import data_bucket, current_app, text, menu, form, keypad_state_manager_reset
import gc
# from process_modules.app_list import installed_app_dict
import json
from process_modules.app_runner import app_runner

# def app_handler(last_used_app, last_used_app_folder):
def app_handler():
    # flag = 1
    while True:
        keypad_state_manager_reset()
        
        # if flag:
        #     current_app[0] = last_used_app
        #     current_app[1] = last_used_app_folder
        #     flag = 0
        # try:
       
        #     exec(f"from {current_app[1]}.{current_app[0]} import {current_app[0]}")
        #     eval(current_app[0])(data_bucket)
        # except:
        #     current_app[0] = "home"
        #     current_app[1] = "application_modules"
        # Simulate a loop feeding the watchdog
        # try:
        #     while True:
        #         print("Running normally...")
        #         time.sleep(2)
        #         wdt.feed()  # Feed the watchdog every 2 seconds
        # except KeyboardInterrupt:
        #     wdt.stop()
        #     print("Stopped")
        app_runner()
        gc.collect()
        print(gc.mem_free(), gc.mem_alloc())