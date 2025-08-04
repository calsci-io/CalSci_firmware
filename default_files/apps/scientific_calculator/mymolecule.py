from data_modules.object_handler import display, form, nav, form_refresh, typer, keypad_state_manager, keypad_state_manager_reset
from data_modules.object_handler import current_app
import urequests # type: ignore
import gc
import time
# def search(term):

# url = "http://67e91d51e7f4f94a1ce3.appwrite.global/search_molecule"
# headers = {"molecule": "glucose"}

def mymolecule(db={}):
    print("start of mymolecule", gc.mem_free())
    keypad_state_manager_reset()
    global display, form, form_refresh, typer, nav, current_app
    display.clear_display()
    form.input_list={"inp_0": "sugar"}
    form.form_list=["enter compound: ", "inp_0"]
    form.update()
    form_refresh.refresh()
    while True:
        inp = typer.start_typing()
        if inp == "back":
            # del buffer1, fb1
            current_app[0]="home"
            break

        elif inp == "ok":
            form.form_list=["enter compound: ", "inp_0", "loading..."]
            form.update()
            form_refresh.refresh()
            molecule=form.inp_list()["inp_0"]
            url = "http://67e91d51e7f4f94a1ce3.appwrite.global/search_molecule"
            headers = {"molecule": molecule}
            try:
                response = urequests.get(url, headers=headers).text
            except:
                response = "connect to internet"
            print(response)
            display.clear_display()
            form.form_list=["enter compound: ", "inp_0", response]
            form.update()
            form_refresh.refresh()
        elif inp == "alpha" or inp == "beta":
            keypad_state_manager(x=inp)
            form.update_buffer("")
        elif inp not in ["alpha", "beta", "ok"]:
            form.update_buffer(inp)
        form_refresh.refresh(state=nav.current_state())
    
        time.sleep(0.1)
