import utime as time  # type:ignore
from math import *
import machine
from data_modules.object_handler import display, nav, typer, keypad_state_manager, form, form_refresh
from process_modules import boot_up_data_update
from data_modules.object_handler import app

def add_2_nums(db={}):
    form.input_list={"inp_0": " ", "inp_1": " "}
    form.form_list = ["subtraction", "1st num", "inp_0", "2nd num", "inp_1"]
    form.update()
    display.clear_display()
    form_refresh.refresh()

    while True:
        inp = typer.start_typing()
        if inp == "back":
            app.set_app_name("installed_apps")
            app.set_group_name("root")
            break
        
        if inp == "ok":
            num1=form.inp_list()["inp_0"]       
            num2=form.inp_list()["inp_1"]
            nums_sum=" = "+str(int(num1)-int(num2))
            form.form_list = ["subtraction", "first num", "inp_0", "second num", "inp_1", nums_sum]
            form.update()
            form_refresh.refresh()
            
        if inp == "alpha" or inp == "beta":
            keypad_state_manager(x=inp)
            form.update_buffer("")

        if inp == "off":
            boot_up_data_update.main()
            machine.deepsleep()
        if inp not in ["alpha", "beta", "ok"]:
            form.update_buffer(inp)
        form_refresh.refresh(state=nav.current_state())
        time.sleep(0.15)



