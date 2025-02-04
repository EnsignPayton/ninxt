import asyncio
import threading
import struct
from dataclasses import dataclass
from bleak import BleakClient
from evdev import InputDevice, ecodes, categorize

NINXT_ADDR = "DC:DA:0C:61:DE:12"
CHR_UUID = "00002f61-712a-44b5-d241-a54659450e7b"
CON_PATH = "/dev/input/event22"

queue = asyncio.Queue()

@dataclass
class ControllerState:
    buttons: int
    x_axis: int
    y_axis: int

    def to_bytes(self) -> bytes:
        return struct.pack("<Hbb", self.buttons, self.x_axis, self.y_axis)

state = ControllerState(buttons=0, x_axis=0, y_axis=0)

# Button mapping go here
def update_state(event):
    if event.type == ecodes.EV_KEY:
        # A,X -> A
        if event.code == 305 or event.code == 307:
            if event.value:
                state.buttons |= 0x8000
            else:
                state.buttons &= ~(0x8000)
        # B,Y -> B
        if event.code == 304 or event.code == 308:
            if event.value:
                state.buttons |= 0x4000
            else:
                state.buttons &= ~(0x4000)
        # ZL,ZR -> Z
        if event.code == 312 or event.code == 313:
            if event.value:
                state.buttons |= 0x2000
            else:
                state.buttons &= ~(0x2000)
        # +,- -> Start
        if event.code == 315 or event.code == 314:
            if event.value:
                state.buttons |= 0x1000
            else:
                state.buttons &= ~(0x1000)
        # L -> L
        if event.code == 310:
            if event.value:
                state.buttons |= 0x0020
            else:
                state.buttons &= ~(0x0020)
        # R -> R
        if event.code == 311:
            if event.value:
                state.buttons |= 0x0010
            else:
                state.buttons &= ~(0x0010)
    elif event.type == ecodes.EV_ABS:
        # Left X -> X
        if event.code == 0:
            state.x_axis = scale(event.value)
        # Left Y -> Y
        if event.code == 1:
            state.y_axis = scale(event.value)
        # Right X -> CX
        if event.code == 3:
            if event.value < -8192:
                state.buttons |= 0x0002
                state.buttons &= ~(0x0001)
            elif event.value > 8192:
                state.buttons &= ~(0x0002)
                state.buttons |= 0x0001
            else:
                state.buttons &= ~(0x0002)
                state.buttons &= ~(0x0001)
        # Right Y -> CY
        if event.code == 4:
            if event.value < -8192:
                state.buttons |= 0x0008
                state.buttons &= ~(0x0004)
            elif event.value > 8192:
                state.buttons &= ~(0x0008)
                state.buttons |= 0x0004
            else:
                state.buttons &= ~(0x0008)
                state.buttons &= ~(0x0004)
        # DX -> DX
        if event.code == 16:
            if event.value < 0:
                state.buttons |= 0x0200
                state.buttons &= ~(0x0100)
            elif event.value > 0:
                state.buttons &= ~(0x0200)
                state.buttons |= 0x0100
            else:
                state.buttons &= ~(0x0200)
                state.buttons &= ~(0x0100)
        # DY -> DY
        if event.code == 17:
            if event.value < 0:
                state.buttons |= 0x0800
                state.buttons &= ~(0x0400)
            elif event.value > 0:
                state.buttons &= ~(0x0800)
                state.buttons |= 0x0400
            else:
                state.buttons &= ~(0x0800)
                state.buttons &= ~(0x0400)

def scale(value):
    shifted = (value >> 8) & 0xFF
    if shifted >= 128:
        shifted -= 128
    return shifted

async def bleak_main():
    async with BleakClient(NINXT_ADDR) as client:
        while True:
            event = await queue.get()
            update_state(event)
            data = state.to_bytes()
            await client.write_gatt_char(CHR_UUID, data, response=False)

def evdev_main():
    try:
        gamepad = InputDevice(CON_PATH)

        for event in gamepad.read_loop():
            if event.type != ecodes.EV_SYN:
                print(categorize(event))
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