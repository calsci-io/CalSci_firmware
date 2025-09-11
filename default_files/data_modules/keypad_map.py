class Keypad_5X8:
    def __init__(self, state="d"):
        # keypad_5X8_layout_default=[
        #     ["alpha", "beta", "nav_u", "home", "on"],
        #     ["back", "nav_l", "ok", "nav_r", "tab"],
        #     ["\"", "'", "nav_d", "(", ")"],
        #     ["pow(", "sin(", "cos(", "tan(", "log("],
        #     ["7", "8", "9", "nav_b", "AC"],
        #     ["4", "5", "6", "*", "/"],
        #     ["1", "2", "3", "+", "-"],
        #     [".", "0", "*pow(10,", "ans", "exe"],
        # ]
        # keypad_5X8_layout_alpha=[
        #     ["alpha", "beta", "nav_u", "opt", "on"],
        #     ["alpha_on", "nav_l", "ok", "nav_r", "<"],
        #     ["y", "z", "nav_d", "[", "]"],
        #     ["t", "u", "v", "w", "x"],
        #     ["o", "p", "q", "r", "s"],
        #     ["j", "k", "l", "m", "n"],
        #     ["e", "f", "g", "h", "i"],
        #     ["a", "b", "c", "d", " "],
        # ]
        # keypad_5X8_layout_beta=[
        #     ["alpha", "beta", "nav_u", "backlight", "on"],
        #     ["beta_on", "nav_l", "ok", "nav_r", ">"],
        #     ["off", "caps", "nav_d", "{", "}"],
        #     ["pi", "asin(", "acos(", "atan(", ""],
        #     ["&", "~", "\\", "", ""],
        #     ["$", "%", "^", "|", ""],
        #     ["!", "@", "#", "=", "_"],
        #     [",", "?", ":", ";", ""],
        # ]
        keypad_5X8_layout_default=[
            ["on", "alpha", "beta", "home", "wifi"],
            ["backlight", "back", "toolbox", "diff(", "ln"],
            ["nav_l", "nav_d", "nav_r", "ok", "nav_u"],
            ["module", "bluetooth", "sin(", "cos", "tan"],
            ["ingn(", "pi", "e", "summation", "fraction"],
            ["log", "pow(", "pow( ,0.5)", "pow( ,2)", "S_D"],
            ["7", "8", "9", "nav_b", "AC"],
            ["4", "5", "6", "*", "/"],
            ["1", "2", "3", "+", "-"],
            [".", "0", ",", "ans", "exe"]
        ]
        keypad_5X8_layout_alpha=[
            ["on", "alpha", "beta", "home", "wifi"],
            ["backlight", "back", "caps", "f", "l"],
            ["nav_l", "nav_d", "nav_r", "ok", "nav_u"],
            ["a", "b", "c", "d", "e"],
            ["g", "h", "i", "j", "k"],
            ["m", "n", "o", "p", "q"],
            ["r", "s", "t", "nav_b", "AC"],
            ["u", "v", "w", "*", "/"],
            ["x", "y", "z", "+", "-"],
            [" ", "off", "tab", "ans", "exe"]
        ]
        keypad_5X8_layout_beta=[
            ["on", "alpha", "beta", "home", "wifi"],
            ["backlight", "back", "undo", "=", "$"],
            ["nav_l", "nav_d", "nav_r", "ok", "nav_u"],
            ["copy", "paste", "asin(", "acos(", "atan("],
            ["&", "`", '"', "'", "shot"],
            ["^", "~", "!", "<", ">"],
            ["[", "]", "%", "nav_b", "AC"],
            ["{", "}", ":", "*", "/"],
            ["(", ")", ";", "+", "-"],
            ["@", "?", "\"", "ans", "exe"]
        ]
        
        self.state=state
        self.states={"d":keypad_5X8_layout_default, "a":keypad_5X8_layout_alpha, "b": keypad_5X8_layout_beta}
    def key_out(self, col, row):
        return self.states[self.state][row][col]
    def key_change(self, state):
        self.state=state
