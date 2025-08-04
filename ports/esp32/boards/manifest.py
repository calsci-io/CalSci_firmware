freeze("$(PORT_DIR)/modules")
include("$(MPY_DIR)/extmod/asyncio")

# Useful networking-related packages.
require("bundle-networking")
# freeze("$(MPY_DIR)/python_modules/data_modules")
# freeze("$(MPY_DIR)/python_modules/input_modules")
# freeze("$(MPY_DIR)/python_modules/process_modules")
# Require some micropython-lib modules.
#require("aioespnow")
#require("dht")
#require("ds18x20")
#require("neopixel")
#require("onewire")
#require("umqtt.robust")
#require("umqtt.simple")
#require("upysh")

