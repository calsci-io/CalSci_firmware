# from data_modules.object_handler import test_deep_sleep_awake
from sleeping_features import test_deep_sleep_awake, swdt
from process_modules import boot_up_data_update
class Typer:
    def __init__(self, keypad, keypad_map):
        self.keypad=keypad
        self.keypad_map=keypad_map

    def start_typing(self):
        try:
            key_inp=self.keypad.keypad_loop()
            text=self.keypad_map.key_out(col=int(key_inp[0]), row=int(key_inp[1]))
            # swdt.feed()
            if text == "off":
                boot_up_data_update.main()
                test_deep_sleep_awake()
            return text
        except:
            swdt.stop()
            print("soft watchdog timer stopped")


