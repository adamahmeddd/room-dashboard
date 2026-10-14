# 🌙 Room Dashboard

A cozy, touch-controlled room dashboard built on an **Arduino Uno** and a **2.4" TFT touch shield**.
A small Python script on the PC feeds it live data over USB: time, weather, prayer times, PC stats and little messages.

## ✨ Features

**🏠 Home**
- Greeting that changes with the time of day
- Big 12-hour clock with AM/PM, date and a seconds bar
- Live weather: temperature, icon (sun / moon / cloud / rain), humidity and wind
- Rotating kind messages, tap the card for a new one

**🕌 Prayer**
- Next prayer with a live countdown, plus all 5 daily prayer times
- Switches to this page automatically 10 minutes before each prayer
- Shows "It's time" when the prayer begins

**⏳ Focus**
- Pomodoro timer: 25 / 5 / 15 minutes
- Dotted ring that empties as time passes, keeps running on any page
- The PC plays a chime when a session ends

**💻 PC**
- CPU, RAM, disk and battery usage as soft progress bars

**Design:** night-sky gradient with stars and pastel cards, built for upside-down wall mounting (configurable).

## 🧰 Hardware

| Part | Notes |
|---|---|
| Arduino Uno (or clone) | |
| 2.4" TFT LCD touch shield | 8-bit parallel, HX8347G driver (ID `0x7575`) |
| USB cable | Power + data from the PC |

The shield plugs straight on top of the Uno. No wiring needed.

## 🏗️ How it works

```
PC (dashboard.py) ──USB serial──► Arduino Uno + TFT
 - time & date                    ◄── touch commands
 - weather   (Open-Meteo)
 - prayers   (Aladhan API)
 - CPU / RAM / disk / battery
 - messages  (message.txt)
```

The Arduino only draws and handles touch. All the internet work happens in Python.

### Serial protocol (115200 baud, one line per message)

| From PC | Meaning |
|---|---|
| `T:HH:MM:SS` | Current time (24h) |
| `D:Wed, 14 Oct` | Date |
| `W:temp,hum,condition,wind,icon` | Weather (icon: 0 sun, 1 moon, 2 cloud, 3 rain) |
| `Q:HH:MM,HH:MM,HH:MM,HH:MM,HH:MM` | Fajr, Dhuhr, Asr, Maghrib, Isha |
| `P:cpu,ram,disk,battery` | PC stats in % (`-1` = none) |
| `M:text` | Message to show |

| From screen | Meaning |
|---|---|
| `C:NEXTMSG` | Message card tapped |
| `C:TIMER_DONE` | Focus timer finished |

## 🚀 Setup

### 1. Arduino
1. Install from the Library Manager:
   - **MCUFRIEND_kbv** (David Prentice)
   - **Adafruit GFX Library**
   - **Adafruit TouchScreen**
2. **Enable the HX8347G driver** (off by default): open
   `Documents/Arduino/libraries/MCUFRIEND_kbv/MCUFRIEND_kbv.cpp` and change
   `//#define SUPPORT_8347D` to `#define SUPPORT_8347D`
3. Upload `arduino/room_dashboard/room_dashboard.ino`
4. On first boot, tap the 3 crosshairs to calibrate touch (saved to EEPROM).
   To recalibrate, hold a finger on the screen while pressing reset.

> Mounting it the normal way up? Change `#define ROT 2` to `0`.

### 2. PC (Windows)
```bash
cd pc
pip install -r requirements.txt
python dashboard.py
```
Settings at the top of `dashboard.py`:
- `PORT`: Arduino COM port
- `LAT, LON`: your location
- `NAME`: your name
- `PRAYER_METHOD`: [calculation method](https://aladhan.com/calculation-methods), default `5` (Egyptian General Authority of Survey)

Add your own messages to `pc/message.txt`, one per line (max 34 characters).

### 3. Run in the background on startup
1. `Win + R` → `shell:startup`
2. New shortcut:
   `"<path-to>\pythonw.exe" "<path-to>\pc\dashboard.py"`

It starts with Windows, with no window. To stop it (for example before uploading new code), end **pythonw.exe** in Task Manager.

## 📁 Structure
```
room-dashboard/
├── arduino/room_dashboard/room_dashboard.ino
├── pc/
│   ├── dashboard.py
│   ├── message.txt
│   └── requirements.txt
└── README.md
```

## 🙏 Credits
- Weather: [Open-Meteo](https://open-meteo.com/)
- Prayer times: [Aladhan API](https://aladhan.com/prayer-times-api)
- Display driver: [MCUFRIEND_kbv](https://github.com/prenticedavid/MCUFRIEND_kbv)

## 👤 Author
**Adam Ahmed**, Communications & Electronics Engineering student, AASTMT
[github.com/adamahmeddd](https://github.com/adamahmeddd)
