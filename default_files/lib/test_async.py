import uasyncio as asyncio
from machine import Pin

input_pin = Pin(2, Pin.IN, Pin.PULL_UP)
task = None

async def read_gpio():
    try:
        while True:
            print("GPIO Value:", input_pin.value())
            await asyncio.sleep(1)
    except asyncio.CancelledError:
        print("GPIO reader cancelled.")

async def main():
    global task
    task = asyncio.create_task(read_gpio())
    # await asyncio.sleep(10)  # Run for 10 seconds
    # task.cancel()

async def cancel_task():
    global task
    task.cancel()
# asyncio.run(main())