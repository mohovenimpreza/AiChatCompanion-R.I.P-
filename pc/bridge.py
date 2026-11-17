import os
import sys
import time

import psutil
import serial
from pynput import keyboard

# Serial port of the ESP32: first argument, or COMPANION_PORT, or the default below (list ports with: ls /dev/cu.*)
PORT = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("COMPANION_PORT", "/dev/cu.usbserial-0001")
esp = serial.Serial(PORT, 115200)
time.sleep(2)

print("AI chat companion - bridge")

# ─── STATE ──────────────────────────────────────────────
current_state = ""
shift_held = False
typing_start = 0
last_keypress = time.time() - 999
last_enter = 0
in_post_enter = False
idle_timeout = 15.0
sleep_timeout = 40.0

# network detection
NOISE_THRESHOLD = 3000  # bytes below this = background noise
STABLE_COUNT = 15  # how many zero reads = answer done
net_stable = 0
answer_started = False


# ─── SEND ───────────────────────────────────────────────
def send(cmd):
    global current_state
    if cmd != current_state:
        esp.write((cmd + "\n").encode())
        print(f"→ {cmd}")
        current_state = cmd


# ─── NETWORK ────────────────────────────────────────────
last_net = psutil.net_io_counters().bytes_recv


def get_net_spike():
    global last_net
    current = psutil.net_io_counters().bytes_recv
    diff = current - last_net
    last_net = current
    return diff


# ─── KEYBOARD ───────────────────────────────────────────
def on_press(key):
    global last_keypress, last_enter, shift_held
    global in_post_enter, typing_start
    global net_stable, answer_started

    now = time.time()

    if key in (keyboard.Key.shift, keyboard.Key.shift_r):
        shift_held = True
        return

    if key == keyboard.Key.enter and shift_held:
        last_keypress = now
        print("  [Shift+Enter → ignored]")
        return
    if key == keyboard.Key.enter and not shift_held:
        print("  [Enter → THINK]")
        last_enter = now
        in_post_enter = True
        typing_start = 0
        net_stable = 0
        answer_started = False
        send("THINK")
        return

    # typing during TALK = cancel and LISTEN
    if in_post_enter:
        in_post_enter = False
        answer_started = False
        typing_start = now
        send("LISTEN")
    else:
        if typing_start == 0:
            typing_start = now
        send("LISTEN")

    last_keypress = now


def on_release(key):
    global shift_held
    if key in (keyboard.Key.shift, keyboard.Key.shift_r):
        shift_held = False


listener = keyboard.Listener(on_press=on_press, on_release=on_release)
listener.start()

print("Ready")
print("    Boot        → SLEEP (until the first key press)")
print("    Type        → LISTEN")
print("    Enter       → THINK")
print("    Network↑    → TALK")
print("    Network=0   → IDLE")
print("    15s no key  → IDLE")
print("    40s no key  → SLEEP")
print()

# ─── MAIN LOOP ──────────────────────────────────────────
while True:
    try:
        now = time.time()
        since_key = now - last_keypress
        net = get_net_spike()

        if in_post_enter:
            since_enter = now - last_enter

            # THINK phase — 2.5 seconds
            if since_enter < 2.5:
                send("THINK")

            # after THINK — watch network
            else:
                if net > NOISE_THRESHOLD:
                    # big spike = the answer is being received
                    answer_started = True
                    net_stable = 0
                    send("TALK")
                    print(f"  [Network spike: {net} → TALK]")

                else:
                    # no big spike
                    if answer_started:
                        net_stable += 1
                        print(f"  [Network quiet x{net_stable}]")

                        if net_stable >= STABLE_COUNT:
                            # answer finished
                            print("  [answer done → IDLE]")
                            in_post_enter = False
                            answer_started = False
                            net_stable = 0
                            send("IDLE")

                    else:
                        # the answer has not started yet
                        # safety fallback after 20 seconds
                        if since_enter > 20:
                            in_post_enter = False
                            send("IDLE")

        else:
            if since_key > sleep_timeout:
                send("SLEEP")
            elif since_key > idle_timeout:
                send("IDLE")
            elif since_key > 2.0:
                send("IDLE")

        time.sleep(0.3)

    except KeyboardInterrupt:
        print("\n👋 Bye!")
        send("SHUTDOWN")
        time.sleep(4)
        esp.close()
        listener.stop()
        break

    except Exception as e:
        print(f"Error: {e}")
        time.sleep(0.5)
