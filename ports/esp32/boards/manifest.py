freeze("$(PORT_DIR)/modules")
# Freeze the live CalSci boot script so flashed firmware runs the current boot path.
freeze("$(MPY_DIR)/../calsci_latest_itr", "boot.py")
include("$(MPY_DIR)/extmod/asyncio")

# Useful networking-related packages.
require("bundle-networking")

# Require some micropython-lib modules.
require("aioespnow")
require("dht")
require("ds18x20")
require("neopixel")
require("onewire")
require("umqtt.robust")
require("umqtt.simple")
require("upysh")
