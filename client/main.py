import asyncio
import threading
from bleak import BleakClient
from evdev import InputDevice, ecodes

queue = asyncio.Queue()

def map_key(code):
    if code == 305 or code == 307:
        return 15
    if code == 304 or code == 308:
        return 14
    if code == 310:
        return 5
    if code == 311:
        return 4
    if code == 312 or code == 313:
        return 13
    if code == 314 or code == 315:
        return 12
    return -1

def scale(value):
    shifted = (value >> 8) & 0xFF
    if shifted >= 128:
        shifted -= 128
    return shifted

async def bleak_main():
    # TODO: Don't hard code this
    address = "DC:DA:0C:61:DE:12"
    btn_uuid = "00002f61-712a-44b5-d241-a54659450e7b"
    axs_uuid = "4a06a311-24ca-a1a1-7646-031ee289a877"
    async with BleakClient(address) as client:
        while True:
            event = await queue.get()
            if event.type == ecodes.EV_KEY:
                print(f"key {event.code} {event.value}")
                key = map_key(event.code)
                if key != -1:
                    await client.write_gatt_char(btn_uuid, [key, event.value], response=False)
            elif event.type == ecodes.EV_ABS:
                print(f"abs {event.code} {event.value}")
                if event.code == 16:
                    if event.value == -1:
                        await client.write_gatt_char(btn_uuid, [9, 1], response=False)
                    elif event.value == 1:
                        await client.write_gatt_char(btn_uuid, [8, 1], response=False)
                    else:
                        await client.write_gatt_char(btn_uuid, [9, 0], response=False)
                        await client.write_gatt_char(btn_uuid, [8, 0], response=False)
                elif event.code == 17:
                    if event.value == -1:
                        await client.write_gatt_char(btn_uuid, [11, 1], response=False)
                    elif event.value == 1:
                        await client.write_gatt_char(btn_uuid, [10, 1], response=False)
                    else:
                        await client.write_gatt_char(btn_uuid, [11, 0], response=False)
                        await client.write_gatt_char(btn_uuid, [10, 0], response=False)
                elif event.code == 3:
                    if event.value < -8192:
                        await client.write_gatt_char(btn_uuid, [1, 1], response=False)
                    elif event.value > 8192:
                        await client.write_gatt_char(btn_uuid, [0, 1], response=False)
                    else:
                        await client.write_gatt_char(btn_uuid, [1, 0], response=False)
                        await client.write_gatt_char(btn_uuid, [0, 0], response=False)
                elif event.code == 4:
                    if event.value < -8192:
                        await client.write_gatt_char(btn_uuid, [3, 1], response=False)
                    elif event.value > 8192:
                        await client.write_gatt_char(btn_uuid, [2, 1], response=False)
                    else:
                        await client.write_gatt_char(btn_uuid, [3, 0], response=False)
                        await client.write_gatt_char(btn_uuid, [2, 0], response=False)
                elif event.code == 0:
                    scaled = scale(event.value)
                    await client.write_gatt_char(axs_uuid, [0, scaled], response=False)
                elif event.code == 1:
                    scaled = scale(event.value)
                    await client.write_gatt_char(axs_uuid, [1, scaled], response=False)

def evdev_main():
    try:
        # TODO: Don't hard code this
        gamepad = InputDevice('/dev/input/event22')

        for event in gamepad.read_loop():
            if event.type != ecodes.EV_SYN:
                asyncio.run_coroutine_threadsafe(queue.put(event), loop)
    except:
        print("Problem in controller loop")

async def main():
    global loop
    loop = asyncio.get_running_loop()
    threading.Thread(target=evdev_main, daemon=True).start()
    await bleak_main()

print("Hello, NinXT")
asyncio.run(main())