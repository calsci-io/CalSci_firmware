import time
from data_modules.object_handler import nav, keypad_state_manager, typer, keymap
from data_modules.object_handler import current_app
from process_modules import boot_up_data_update
from data_modules.object_handler import app
from hcsr04 import HCSR04

def dist_measure(sensor):
    try:
        distance = sensor.distance_cm()
        print('Distance:', distance, 'cm')
    except OSError as e:
        print('Sensor error:', e)
    time.sleep(1)

def ultra_sonic_sensor(db={}):
    sensor = HCSR04(trigger_pin=16, echo_pin=2)
    display.clear_display()
    menu_list=["distance:", "0"]
    menu.menu_list=menu_list
    menu.update()
    menu_refresh.refresh()
    try:
        while True:
            inp = typer.start_typing()
            
            if inp == "back":
                break
            
            elif inp == "alpha" or inp == "beta":                        
                keypad_state_manager(x=inp)
                menu.update_buffer("")
            elif inp =="ok":
                distance = sensor.distance_cm()
                menu.menu_list=["distance:", str(distance)]
                menu.update()
                menu_refresh.refresh()
            menu.update_buffer(inp)
            menu_refresh.refresh(state=nav.current_state())
            time.sleep(0.2)
    except Exception as e:
        print(f"Error: {e}")