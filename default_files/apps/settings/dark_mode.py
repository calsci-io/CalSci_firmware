dark_mode_state=False
from data_modules.object_handler import display
def dark_mode():
    global dark_mode_state
    # dm = dark_mode_state
    if dark_mode_state == False:
        dark_mode_state=True
        display.invert(True)
    else:
        dark_mode_state=False
        display.invert(False)

         