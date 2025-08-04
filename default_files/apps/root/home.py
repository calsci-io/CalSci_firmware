import time
import json
import machine
# from data_modules.object_handler import nav, keypad_state_manager, menu, menu_refresh, typer, keymap, display
from data_modules.object_handler import nav, keypad_state_manager, typer, keymap
from data_modules.object_handler import current_app
from process_modules import boot_up_data_update
from data_modules.object_handler import app
# import machine
import esp32
# import time

# def test_deep_sleep_awake():
#     # -------- Hold GPIO32 HIGH --------
#     # hold_pin = machine.Pin(32, machine.Pin.OUT)
#     # hold_pin.value(0)  # Keep high
#     # pin = machine.Pin(32, machine.Pin.ALT, machine.Pin.ALT_OPEN_DRAIN)
#     # pin.hold(True)

#     # # Set initial value and direction (if needed)
#     # pin.value(0)
#     # pin.init(mode=machine.Pin.OUT)
#     opin = machine.Pin(32, machine.Pin.OUT, value=1, hold=True) # hold output level
#     # esp32.gpio_deep_sleep_hold(True)
#     # -------- Configure Wakeup Pin (GPIO33) --------
#     wakeup_pin = machine.Pin(33, mode=machine.Pin.IN, pull=machine.Pin.PULL_DOWN)

#     # Enable wakeup on high level (1)
#     esp32.wake_on_ext0(pin=wakeup_pin, level=esp32.WAKEUP_ANY_HIGH)
#     esp32.gpio_deep_sleep_hold(True)
#     # print("Going to deep sleep now...")
#     # time.sleep(1)  # Give time for message to print
#     machine.deepsleep()

def home(db={}):
    display.clear_display()
    json_file = "/db/application_modules_app_list.json" 
    with open(json_file, "r") as file:
        data = json.load(file)

    menu_list = []
    for apps in data:
        if apps["visibility"]:
            menu_list.append(apps["name"])

    menu.menu_list=menu_list
    menu.update()
    menu_refresh.refresh()
    try:
        while True:
            inp = typer.start_typing()
            
            if inp == "back":
                pass
            
            elif inp == "alpha" or inp == "beta":                        
                keypad_state_manager(x=inp)
                menu.update_buffer("")
            elif inp =="ok":
                # current_app[0]=menu.menu_list[menu.menu_cursor]
                app.set_app_name(menu.menu_list[menu.menu_cursor])
                app.set_group_name("root")
                break
            # elif inp == "off":
            #     # boot_up_data_update.main()
            #     # machine.deepsleep()
            #     test_deep_sleep_awake()

            menu.update_buffer(inp)
            menu_refresh.refresh(state=nav.current_state())
            time.sleep(0.2)
    except Exception as e:
        print(f"Error: {e}")