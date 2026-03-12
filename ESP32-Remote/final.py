from machine import Pin, I2C, ADC
import ssd1306
from mpu6050 import MPU6050
import time
import math
import network
import espnow
import struct


# ===============================
# Button Logic (Invert)
# ===============================
def read_btn(pin):
    return 1 - pin.value()


# ===============================
# Joystick Normalize
# ===============================
def normalize_joystick(value):
    center = 1850
    max_range = 1850

    normalized = (center - value) / max_range

    if normalized > 1:
        normalized = 1
    if normalized < -1:
        normalized = -1

    if abs(normalized) < 0.05:
        normalized = 0

    return normalized


# ===============================
# OLED UI
# ===============================
def draw_ui(oled, angle, throttle, gear, safety, horn):

    t = throttle / 100.0 if throttle > 1 else float(throttle)
    if t < 0: t = 0
    if t > 1: t = 1

    oled.fill(0)

    gear_label = "G:" + gear
    oled.text(gear_label, 2, 2)

    if safety:
        oled.text("[SAFE]", 44, 2)
    else:
        oled.text("[----]", 44, 2)

    angle_str = "{:+.0f}".format(angle) + "~"
    ax = 128 - len(angle_str) * 8 - 2
    oled.text(angle_str, ax, 2)

    if safety:
        oled.fill_rect(0, 13, 128, 2, 1)
    else:
        oled.hline(0, 13, 128, 1)

    oled.text("PWR", 2, 18)

    pct_str = "{}%".format(int(t * 100))
    px = 128 - len(pct_str) * 8 - 2
    oled.text(pct_str, px, 18)

    bar_x = 2
    bar_y = 29
    bar_w = 124
    bar_h = 8
    filled = int(t * bar_w)

    oled.rect(bar_x, bar_y, bar_w, bar_h, 1)

    if filled > 2:
        oled.fill_rect(bar_x + 1, bar_y + 1, filled - 2, bar_h - 2, 1)

    oled.text("TLT", 2, 43)

    tilt_bar_x = 30
    tilt_bar_y = 43
    tilt_bar_w = 96
    tilt_bar_h = 8
    center_x = tilt_bar_x + tilt_bar_w // 2

    oled.rect(tilt_bar_x, tilt_bar_y, tilt_bar_w, tilt_bar_h, 1)
    oled.vline(center_x, tilt_bar_y, tilt_bar_h, 1)

    norm = angle / 90.0

    if norm > 1: norm = 1
    if norm < -1: norm = -1

    half = tilt_bar_w // 2 - 2
    fill_len = int(abs(norm) * half)

    if horn:
        oled.text("HORN!", 50, 55)

    if norm >= 0:
        oled.fill_rect(center_x + 1, tilt_bar_y + 1, fill_len, tilt_bar_h - 2, 1)
    elif fill_len > 0:
        oled.fill_rect(center_x - fill_len, tilt_bar_y + 1, fill_len, tilt_bar_h - 2, 1)

    oled.rect(0, 0, 128, 64, 1)
    oled.show()


# ===============================
# ESP-NOW
# ===============================
sta = network.WLAN(network.STA_IF)
sta.active(True)
sta.disconnect()
sta.config(channel=1)

e = espnow.ESPNow()
e.active(True)

peer = b'\x28\x05\xA5\x34\x87\xE4'

try:
    e.del_peer(peer)
except:
    pass

try:
    e.add_peer(peer)
except:
    pass


# ===============================
# Hardware
# ===============================
led = Pin(2, Pin.OUT)

i2c_oled = I2C(0, scl=Pin(22), sda=Pin(21))
i2c_mpu = I2C(1, scl=Pin(33), sda=Pin(32))

oled = ssd1306.SSD1306_I2C(128, 64, i2c_oled)
mpu = MPU6050(i2c_mpu)


# ===============================
# Inputs
# ===============================
joy_x = ADC(Pin(34))
joy_y = ADC(Pin(35))

joy_x.atten(ADC.ATTN_11DB)

joy_button = Pin(25, Pin.IN, Pin.PULL_UP)
btn_cal = Pin(26, Pin.IN, Pin.PULL_UP)
btn_gear = Pin(27, Pin.IN, Pin.PULL_DOWN)
btn_horn = Pin(5, Pin.IN, Pin.PULL_UP)
btn_safety = Pin(4, Pin.IN, Pin.PULL_UP)

liver = Pin(13, Pin.IN)


# ===============================
# States
# ===============================
gear_main = "N"
drive_mode = "D"
last_sent_gear = "N"

roll_offset = 0

last_cal = 1
last_gear_btn = 1
last_joystick = 1
last_safety = 1
last_horn = 1
last_liver = 1  # FIX: edge detection for liver

safety_mode = False
horn_active = False

roll_filtered = 0
alpha = 0.3

last_send = 0


# ===============================
# LOOP
# ===============================
while True:

    accel = mpu.read_accel_data()
    roll_raw = math.degrees(math.atan2(accel["y"], accel["z"]))

    cal = read_btn(btn_cal)

    if cal == 0 and last_cal == 1:
        roll_offset = roll_raw
        time.sleep_ms(50)

    last_cal = cal

    roll = roll_raw - roll_offset

    if roll > 90: roll = 90
    if roll < -90: roll = -90

    roll_filtered = alpha * roll_filtered + (1 - alpha) * roll


    # Gear Button
    gear_btn = read_btn(btn_gear)
    # print("Gear Btn:", gear_btn, "Last:", last_gear_btn)

    if gear_btn == 0 and last_gear_btn == 1:

        if gear_main == "N":
            gear_main = "DRIVE"
        else:
            gear_main = "N"

        time.sleep_ms(50)

    last_gear_btn = gear_btn


    # Safety Button
    safety_btn = read_btn(btn_safety)

    if safety_btn == 0 and last_safety == 1:

        safety_mode = not safety_mode

        packet = struct.pack("<BB", 1, 1 if safety_mode else 0)

        try:
            e.send(peer, packet)
        except:
            pass

    last_safety = safety_btn


    # Liver + Joystick Button
    joy_btn = read_btn(joy_button)
    liver_val = liver.value()

    if gear_main == "DRIVE":

        led.value(1)

        # FIX: ใช้ edge detection แทนการเช็ค level ตลอดเวลา
        if liver_val == 0 and last_liver == 1:
            drive_mode = "R"

        elif liver_val == 1 and last_liver == 0:
            if drive_mode == "R":
                drive_mode = "D"

        else:
            if joy_btn == 0 and last_joystick == 1:

                if drive_mode == "D":
                    drive_mode = "S"

                elif drive_mode == "S":
                    drive_mode = "D"

                time.sleep_ms(50)

    else:
        led.value(0)

    last_liver = liver_val
    last_joystick = joy_btn


    # Horn
    horn_btn = read_btn(btn_horn)

    if horn_btn == 0 and last_horn == 1:

        horn_active = True

        packet = struct.pack("<BB", 3, 1)

        try:
            e.send(peer, packet)
        except:
            pass

    elif horn_btn == 1 and last_horn == 0:

        horn_active = False

        packet = struct.pack("<BB", 3, 0)

        try:
            e.send(peer, packet)
        except:
            pass

    last_horn = horn_btn


    # Gear Send
    current_gear = "N" if gear_main == "N" else drive_mode

    if current_gear != last_sent_gear:

        packet = struct.pack("<BB", 0, ord(current_gear))

        try:
            e.send(peer, packet)
        except:
            pass

        last_sent_gear = current_gear


    # Throttle
    joy_value = joy_y.read()
    joy_normalized = normalize_joystick(joy_value)

    if gear_main == "N":
        throttle = 0
    else:
        throttle = joy_normalized if joy_normalized > 0 else 0


    # Driving Data
    now = time.ticks_ms()

    if time.ticks_diff(now, last_send) > 10:

        power = int(throttle * 100)

        packet = struct.pack("<Bfi", 2, roll_filtered, power)

        try:
            e.send(peer, packet)
        except:
            pass

        last_send = now


    draw_ui(oled, roll_filtered, int(throttle*100), current_gear, safety_mode, horn_active)