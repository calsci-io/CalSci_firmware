import time
# import json
import machine
from data_modules.object_handler import nav, keypad_state_manager, menu, menu_refresh, typer, keymap, display, app
from data_modules.object_handler import current_app
from process_modules import boot_up_data_update
from process_modules.app_downloader import Apps
from data_modules.object_handler import apps_installer

def installed_apps():
    time.sleep(0.1)
    # apps_installer=Apps()
    display.clear_display()
    # json_file = "/db/installed_apps.json"
    # with open(json_file, "r") as file:
    #     data = json.load(file)
    
    # app_list = apps.get_group_apps()
    

    # for apps in data:
    #     if apps["visibility"]:
    #         app_list.append(apps["name"])
    menu_list = apps_installer.get_group_apps()
    # print(menu_list)
    menu.menu_list=menu_list
    menu.update()
    menu_refresh.refresh()
    try:
        while True:
            inp = typer.start_typing()
            if inp == "back":
                # current_app[0] = "home"
                # current_app[1]="application_modules"
                app.set_app_name("home")
                app.set_group_name("root")
                break
            elif inp == "alpha" or inp == "beta":
                keypad_state_manager(x=inp)
                menu.update_buffer("")
            elif inp == "off":
                boot_up_data_update.main()
                machine.deepsleep()
            elif inp =="ok":
                # current_app[0]=menu.menu_list[menu.menu_cursor]
                app.set_app_name(menu.menu_list[menu.menu_cursor])
                app.set_group_name("installed_apps")
                break
            menu.update_buffer(inp)
            menu_refresh.refresh(state=nav.current_state())
            time.sleep(0.2)
    except Exception as e:
        print(f"Error: {e}")