import gc
import vfs
from flashbdev import bdev
import st7565 as display
import builtins
display.init(9, 11, 10, 13, 12)
builtins.display=display
# display.write_instruction(0x81)
# display.write_instruction(9)
try:
    if bdev:
        vfs.mount(bdev, "/")
except OSError:
    import inisetup

    inisetup.setup()

gc.collect()
