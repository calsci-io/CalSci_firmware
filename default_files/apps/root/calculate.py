import utime as time  # type:ignore
from math import *
import machine
from data_modules.object_handler import display, text, nav, text_refresh, typer, keypad_state_manager, keypad_state_manager_reset, current_app, app
from process_modules import boot_up_data_update
# import uasyncio as asyncio
# from test_async import main, cancel_task
# asyncio.run(main())
# from 
from test_thread import run_espnow_message, end_espnow_task
task=None
def calculate():
    global task
    keypad_state_manager_reset()
    display.clear_display()
    text.all_clear()
    text_refresh.refresh()
    # task=None
    try:
        while True:

            x = typer.start_typing()
            # print(f"x = {x}")
            if x == "back":
                current_app[0]="home"
                current_app[1] = "application_modules"
                break

            if x == "ok" and task == None:
                # asyncio.run(main())
                run_espnow_message()
                task=1
                # task=asyncio.create_task(read_gpio())


            elif x == "ok" and task != None:
                # asyncio.run(cancel_task())
                end_espnow_task()
                task=None

            if x == "ans" and text.text_buffer[0] != "𖤓":
                try:
                    res = str(eval(text.text_buffer[:text.text_buffer_nospace]))
                except Exception as e:
                    res = "Invalid Input"
                text.all_clear()
                display.clear_display()
                text.update_buffer(res)

            elif x == "alpha" or x == "beta":                        
                keypad_state_manager(x=x)
                text.update_buffer("")

            elif x == "off":
                boot_up_data_update.main()
                machine.deepsleep()

            elif x != "ans":
                text.update_buffer(x)

            if text.text_buffer[0] == "𖤓":
                display.clear_display()
                text.all_clear()

            text_refresh.refresh(state=nav.current_state())
            time.sleep(0.2)

    except Exception as e:
        print(f"Error: {e}")
