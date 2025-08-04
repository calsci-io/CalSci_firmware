class Nav:
    def __init__(self, elements={"keypad":"D", "wifi":"WN", "bluetooth":"B", "capslock":"C", "app_name":"app", "charging":"NCH"}):
        self.elements=elements
        self.keypad=self.elements["keypad"] # keypad values can be "D", "A", "B"
        self.wifi=self.elements["wifi"] # wifi values can be "  ", "WC", "WN"
        self.bluetooth=self.elements["bluetooth"] # bluetooth values can be "  ", "BC", "BN"
        self.capslock=self.elements["capslock"] # capslock values can be " ", "C"
        self.app_name=self.elements["app_name"] # app_name value can only be a string containing the name of the app
        self.charging=self.elements["charging"] # charging values can be "CH ", "NCH"
        self.changed_element=""
        
    def update_buffer(self, element): # element will be like ["keypad","A"]
        self.elements[element[0]]=element[1]
        self.changed_element=element[0]
        self.keypad=self.elements["keypad"] # keypad values can be "D", "A", "B"
        self.wifi=self.elements["wifi"] # wifi values can be "  ", "WC", "WN"
        self.bluetooth=self.elements["bluetooth"] # bluetooth values can be "  ", "BC", "BN"
        self.capslock=self.elements["capslock"] # capslock values can be " ", "C"
        self.app_name=self.elements["app_name"] # app_name value can only be a string containing the name of the app
        self.charging=self.elements["charging"] # charging values can be "CH ", "NCH"

    def buffer(self):
        # el=self.elements
        buf=[self.keypad, self.wifi, self.bluetooth, self.capslock, self.app_name, self.charging]
        nav_str=""
        for i in range(len(buf)):
            nav_str += (buf[i] + "-" if i != len(buf)-1 else buf[i])
        return nav_str
    
    def refresh_element(self):
        return self.changed_element
    
    def update(self):
        pass
