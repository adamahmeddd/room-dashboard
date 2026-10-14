import os, time, datetime, winsound
import serial, requests, psutil

PORT = "COM6"
BAUD = 115200
LAT, LON = 30.04, 31.24      # Cairo
NAME = "Adam"
PRAYER_METHOD = 5            # 5 = Egyptian General Authority of Survey

HERE = os.path.dirname(os.path.abspath(__file__))
MSG_FILE = os.path.join(HERE, "message.txt")

CODES = {0: "Clear sky", 1: "Mostly clear", 2: "Partly cloudy", 3: "Overcast",
         45: "Foggy", 48: "Foggy", 51: "Drizzle", 53: "Drizzle", 55: "Drizzle",
         61: "Light rain", 63: "Rain", 65: "Heavy rain", 71: "Snow",
         80: "Showers", 81: "Showers", 82: "Heavy showers", 95: "Thunderstorm"}

DEFAULT_MSGS = [f"You're doing great, {NAME}", "Take a deep breath",
                "Small steps every day", "Drink some water",
                "Rest is productive too", "Proud of you, keep going",
                "One thing at a time"]


def get_weather():
    url = ("https://api.open-meteo.com/v1/forecast"
           f"?latitude={LAT}&longitude={LON}"
           "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,is_day")
    c = requests.get(url, timeout=10).json()["current"]
    code = c["weather_code"]
    if code in (0, 1):
        icon = 0 if c["is_day"] else 1
    elif code in (2, 3, 45, 48):
        icon = 2
    else:
        icon = 3
    cond = CODES.get(code, "Unknown")[:15]
    return (f"W:{c['temperature_2m']:.1f},{c['relative_humidity_2m']},"
            f"{cond},{round(c['wind_speed_10m'])},{icon}")


def get_prayers():
    today = datetime.date.today().strftime("%d-%m-%Y")
    url = (f"https://api.aladhan.com/v1/timings/{today}"
           f"?latitude={LAT}&longitude={LON}&method={PRAYER_METHOD}")
    t = requests.get(url, timeout=10).json()["data"]["timings"]
    return "Q:" + ",".join(t[k][:5] for k in ("Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"))


def get_messages():
    msgs = list(DEFAULT_MSGS)
    if os.path.exists(MSG_FILE):
        with open(MSG_FILE, encoding="utf-8") as f:
            msgs += [line.strip() for line in f if line.strip()]
    return [m[:34] for m in msgs]


def send(ser, line):
    ser.write((line + "\n").encode("ascii", "ignore"))


def run(ser):
    time.sleep(2.5)
    psutil.cpu_percent(None)
    disk_root = os.path.abspath(os.sep)
    last_sec = None
    last_pc = last_fetch = last_wsend = last_msg = last_qsend = 0
    weather = prayers = None
    prayer_day = None
    last_ptry = 0
    mi = 0

    while True:
        now = datetime.datetime.now()

        if now.second != last_sec:
            last_sec = now.second
            send(ser, now.strftime("T:%H:%M:%S"))
            send(ser, "D:" + now.strftime("%a, %d %b"))

        if time.time() - last_pc >= 2:
            last_pc = time.time()
            bat = psutil.sensors_battery()
            send(ser, f"P:{round(psutil.cpu_percent(None))},"
                      f"{round(psutil.virtual_memory().percent)},"
                      f"{round(psutil.disk_usage(disk_root).percent)},"
                      f"{round(bat.percent) if bat else -1}")

        if time.time() - last_fetch >= 600:
            try:
                weather = get_weather()
                last_fetch = time.time()
                print("Weather:", weather)
            except Exception as e:
                print("Weather error:", e)
                last_fetch = time.time() - 540

        if weather and time.time() - last_wsend >= 30:
            last_wsend = time.time()
            send(ser, weather)

        if prayer_day != now.date() and time.time() - last_ptry >= 60:
            last_ptry = time.time()
            try:
                prayers = get_prayers()
                prayer_day = now.date()
                last_qsend = 0
                print("Prayers:", prayers)
            except Exception as e:
                print("Prayer error:", e)

        if prayers and time.time() - last_qsend >= 30:
            last_qsend = time.time()
            send(ser, prayers)

        if time.time() - last_msg >= 20:
            last_msg = time.time()
            msgs = get_messages()
            send(ser, "M:" + msgs[mi % len(msgs)])
            mi += 1

        while ser.in_waiting:
            line = ser.readline().decode(errors="ignore").strip()
            if not line.startswith("C:"):
                continue
            cmd = line[2:]
            print("Screen:", cmd)
            if cmd == "NEXTMSG":
                last_msg = 0
            elif cmd == "TIMER_DONE":
                winsound.PlaySound("SystemExclamation",
                                   winsound.SND_ALIAS | winsound.SND_ASYNC)

        time.sleep(0.03)


while True:
    try:
        with serial.Serial(PORT, BAUD, timeout=0.1) as ser:
            print("Connected to", PORT)
            run(ser)
    except serial.SerialException as e:
        print("Waiting for screen...", e)
        time.sleep(3)
