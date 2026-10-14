#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <EEPROM.h>

MCUFRIEND_kbv tft;
const int XP=9,XM=A3,YP=A2,YM=8;
TouchScreen ts(XP, YP, XM, YM, 300);
#define MINPRESSURE 100
#define MAXPRESSURE 1000

#define ROT 2      // 2 = hung upside down, 0 = normal

// ---------- cozy palette ----------
#define RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
#define CARD    RGB(48,40,88)
#define DIMRING RGB(78,66,120)
#define LAV     RGB(200,180,255)
#define PEACH   RGB(255,190,150)
#define PINK    RGB(255,160,205)
#define MINT    RGB(150,230,200)
#define CREAM   RGB(255,244,225)
#define SKY     RGB(150,200,255)
#define MUTED   RGB(160,145,200)
#define INK     RGB(50,28,70)

// ================= TOUCH =================
struct Cal { uint16_t magic; uint8_t swapXY; int16_t xa, xb, ya, yb; };
Cal cal;
#define CAL_MAGIC 0xCA1B

bool readRaw(int &rx, int &ry) {
  TSPoint p = ts.getPoint();
  pinMode(YP, OUTPUT); pinMode(XM, OUTPUT);
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) { rx = p.x; ry = p.y; return true; }
  return false;
}
void waitRelease() {
  int r = 0, x, y;
  while (r < 8) { if (readRaw(x, y)) r = 0; else r++; delay(10); }
}
void waitTapRaw(int &rx, int &ry) {
  long sx = 0, sy = 0; int n = 0, x, y;
  while (n < 15) { if (readRaw(x, y)) { sx += x; sy += y; n++; } delay(10); }
  rx = sx / n; ry = sy / n; waitRelease();
}
void crosshair(int x, int y, uint16_t c) {
  tft.drawFastHLine(x - 10, y, 21, c); tft.drawFastVLine(x, y - 10, 21, c); tft.drawCircle(x, y, 6, c);
}
void calibrate() {
  int ax1, ay1, ax2, ay2, ax3, ay3;
  tft.setRotation(0);
  do {
    tft.fillScreen(INK); tft.setTextColor(CREAM); tft.setTextSize(2);
    tft.setCursor(20, 140); tft.print(F("Tap each + firmly"));
    crosshair(20, 20, PINK);  waitTapRaw(ax1, ay1); crosshair(20, 20, INK);
    crosshair(220, 20, PINK); waitTapRaw(ax2, ay2); crosshair(220, 20, INK);
    crosshair(20, 300, PINK); waitTapRaw(ax3, ay3); crosshair(20, 300, INK);
    cal.swapXY = abs(ay2 - ay1) > abs(ax2 - ax1);
    if (cal.swapXY) { cal.xa = ay1; cal.xb = ay2; cal.ya = ax1; cal.yb = ax3; }
    else            { cal.xa = ax1; cal.xb = ax2; cal.ya = ay1; cal.yb = ay3; }
  } while (abs(cal.xb - cal.xa) < 20 || abs(cal.yb - cal.ya) < 20);
  cal.magic = CAL_MAGIC; EEPROM.put(0, cal);
  tft.setRotation(ROT);
}
bool getTouch(int &x, int &y) {
  int rx, ry;
  if (!readRaw(rx, ry)) return false;
  long ux = cal.swapXY ? ry : rx, uy = cal.swapXY ? rx : ry;
  x = constrain(20 + (ux - cal.xa) * 200L / (cal.xb - cal.xa), 0, 239);
  y = constrain(20 + (uy - cal.ya) * 280L / (cal.yb - cal.ya), 0, 319);
#if ROT == 2
  x = 239 - x; y = 319 - y;
#endif
  return true;
}

// ================= DATA =================
int hh = -1, mm = -1, ss = -1;
char dateStr[16] = "";
float wTemp = NAN;
int wHum = -1, wWind = -1;
uint8_t wIcon = 2;
char wCond[16] = "";
char msg[40] = "Waiting for your PC...";
int pcv[4] = {-1, -1, -1, -1};     // cpu, ram, disk, battery
bool online = false;
unsigned long lastData = 0;
char buf[80]; uint8_t bi = 0;
uint8_t page = 0;                  // 0 home, 1 prayer, 2 focus, 3 pc
int8_t lastGreet = -1;

// prayer
const char* PN[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
int pt[5] = {-1, -1, -1, -1, -1};  // minutes since midnight
int8_t autoFor = -1, shownNext = -1;

// focus timer
unsigned long tDur = 25UL * 60000UL, tRemain = 25UL * 60000UL, tLast = 0;
bool tRun = false;
uint8_t tMode = 0;
int litDrawn = -1;
unsigned long shownSec = 0;

void sendCmd(const __FlashStringHelper* c) { Serial.print(F("C:")); Serial.println(c); }

// ================= BACKGROUND =================
uint16_t gradAt(int y) { return RGB(20 + 38L * y / 319, 18 + 12L * y / 319, 48 + 24L * y / 319); }

void drawStars(int y0, int y1) {
  uint16_t s = 1234;
  for (uint8_t i = 0; i < 34; i++) {
    s = s * 25173 + 13849; int x = s % 240;
    s = s * 25173 + 13849; int y = s % 320;
    if (y < y0 || y > y1) continue;
    if (i % 5 == 0) { tft.drawFastHLine(x - 2, y, 5, LAV); tft.drawFastVLine(x, y - 2, 5, LAV); }
    else tft.drawPixel(x, y, CREAM);
  }
}

void bgRegion(int y0, int y1) {
  for (int y = y0; y <= y1; y++) tft.drawFastHLine(0, y, 240, gradAt(y));
  drawStars(y0, y1);
}

void title(const __FlashStringHelper* t) {
  tft.setTextSize(3); tft.setTextColor(LAV);
  tft.setCursor(14, 10); tft.print(t);
}

void pill(int x, int y, int w, int h, uint16_t c, uint16_t tc, const __FlashStringHelper* label) {
  tft.fillRoundRect(x, y, w, h, h / 2, c);
  tft.setTextSize(2); tft.setTextColor(tc);
  tft.setCursor(x + (w - (int)strlen_P((PGM_P)label) * 12) / 2, y + (h - 16) / 2);
  tft.print(label);
}

// ================= NAV BAR =================
const __FlashStringHelper* navLabel(uint8_t i) {
  switch (i) { case 0: return F("Home"); case 1: return F("Prayer"); case 2: return F("Focus"); default: return F("PC"); }
}

void drawNav() {
  tft.fillRoundRect(8, 282, 224, 34, 17, CARD);
  for (uint8_t i = 0; i < 4; i++) {
    int x = 8 + i * 56;
    if (i == page) tft.fillRoundRect(x + 4, 286, 48, 26, 13, PINK);
    tft.setTextSize(1); tft.setTextColor(i == page ? INK : LAV);
    tft.setCursor(x + (56 - (int)strlen_P((PGM_P)navLabel(i)) * 6) / 2, 295);
    tft.print(navLabel(i));
  }
}

// ================= HOME =================
int8_t greetIndex() {
  if (hh < 0) return 0;
  if (hh < 5) return 4;
  if (hh < 12) return 1;
  if (hh < 18) return 2;
  if (hh < 22) return 3;
  return 4;
}

void drawGreeting() {
  bgRegion(0, 33);
  const __FlashStringHelper* g;
  switch (greetIndex()) {
    case 1:  g = F("Good morning, Adam"); break;
    case 2:  g = F("Good afternoon Adam"); break;
    case 3:  g = F("Good evening, Adam"); break;
    case 4:  g = F("Good night, Adam"); break;
    default: g = F("Hello, Adam");
  }
  tft.setTextSize(2); tft.setTextColor(PINK);
  tft.setCursor((240 - (int)strlen_P((PGM_P)g) * 12) / 2, 11);
  tft.print(g);
  lastGreet = greetIndex();
}

void drawClockCard() {
  tft.fillRoundRect(8, 36, 224, 92, 14, CARD);
  char t[6];
  if (hh < 0) strcpy(t, "--:--");
  else { int h12 = hh % 12; if (!h12) h12 = 12; sprintf(t, "%d:%02d", h12, mm); }
  int len = strlen(t);
  int w = len * 36 + (hh < 0 ? 0 : 30);
  int x0 = 8 + (224 - w) / 2;
  tft.setTextSize(6); tft.setTextColor(CREAM);
  tft.setCursor(x0, 44); tft.print(t);
  if (hh >= 0) {
    tft.setTextSize(2); tft.setTextColor(PEACH);
    tft.setCursor(x0 + len * 36 + 6, 46);
    tft.print(hh < 12 ? F("AM") : F("PM"));
  }
  tft.setTextSize(2); tft.setTextColor(LAV);
  tft.setCursor(8 + (224 - (int)strlen(dateStr) * 12) / 2, 98);
  tft.print(dateStr);
  for (int i = 0; i < 60; i++) tft.fillRect(30 + i * 3, 120, 2, 3, (ss >= 0 && i <= ss) ? PINK : DIMRING);
}

void secStep(int prev) {
  if (prev < 0 || ss < prev) {
    for (int i = 0; i < 60; i++) tft.fillRect(30 + i * 3, 120, 2, 3, i <= ss ? PINK : DIMRING);
  } else {
    for (int i = prev + 1; i <= ss; i++) tft.fillRect(30 + i * 3, 120, 2, 3, PINK);
  }
}

void iconSun(int cx, int cy) {
  tft.fillCircle(cx, cy, 10, PEACH);
  for (int k = 0; k < 8; k++) {
    float a = k * 0.7854;
    tft.drawLine(cx + cos(a) * 14, cy + sin(a) * 14, cx + cos(a) * 19, cy + sin(a) * 19, PEACH);
  }
}
void iconMoon(int cx, int cy, uint16_t bg) {
  tft.fillCircle(cx, cy, 13, CREAM);
  tft.fillCircle(cx + 7, cy - 5, 12, bg);
  tft.fillCircle(cx + 12, cy + 10, 1, LAV);
  tft.fillCircle(cx + 15, cy - 14, 1, LAV);
}
void iconCloud(int cx, int cy) {
  tft.fillCircle(cx - 9, cy + 3, 8, LAV);
  tft.fillCircle(cx + 1, cy - 4, 11, LAV);
  tft.fillCircle(cx + 11, cy + 4, 7, LAV);
  tft.fillRect(cx - 9, cy + 4, 20, 8, LAV);
}
void iconRain(int cx, int cy) {
  iconCloud(cx, cy - 5);
  for (int k = 0; k < 3; k++) tft.drawLine(cx - 8 + k * 8, cy + 12, cx - 11 + k * 8, cy + 19, SKY);
}

void drawWeather() {
  tft.fillRoundRect(8, 136, 224, 70, 14, CARD);
  int cx = 38, cy = 170;
  switch (wIcon) {
    case 0: iconSun(cx, cy); break;
    case 1: iconMoon(cx, cy, CARD); break;
    case 3: iconRain(cx, cy); break;
    default: iconCloud(cx, cy);
  }
  tft.setTextSize(3); tft.setTextColor(CREAM);
  tft.setCursor(72, 148);
  if (isnan(wTemp)) tft.print(F("--"));
  else { tft.print((int)round(wTemp)); tft.write(247); }
  tft.setTextSize(1); tft.setTextColor(LAV);
  tft.setCursor(72, 180); tft.print(wCond);

  tft.setTextColor(MUTED);
  tft.setCursor(166, 146); tft.print(F("Humidity"));
  tft.setTextSize(2); tft.setTextColor(SKY);
  tft.setCursor(166, 158);
  if (wHum < 0) tft.print(F("--")); else { tft.print(wHum); tft.print('%'); }
  tft.setTextSize(1); tft.setTextColor(MUTED);
  tft.setCursor(166, 182);
  if (wWind >= 0) { tft.print(wWind); tft.print(F(" km/h")); }
}

void drawMessage() {
  tft.fillRoundRect(8, 214, 224, 58, 14, CARD);
  tft.setTextSize(2); tft.setTextColor(PINK);
  char line[18];
  const char* p = msg;
  for (int row = 0; row < 2 && *p; row++) {
    int len = strlen(p);
    int take = len <= 17 ? len : 17;
    if (len > 17) { int k = take; while (k > 0 && p[k] != ' ') k--; if (k > 0) take = k; }
    memcpy(line, p, take); line[take] = 0;
    tft.setCursor(8 + (224 - take * 12) / 2, 224 + row * 20);
    tft.print(line);
    p += take; while (*p == ' ') p++;
  }
}

void drawHome() { drawGreeting(); drawClockCard(); drawWeather(); drawMessage(); }

// ================= PRAYER =================
int nowMin() { return hh * 60 + mm; }

int8_t nextPrayer() {
  int n = nowMin();
  for (int8_t i = 0; i < 5; i++) if (pt[i] + 5 > n) return i;
  return 0;
}

bool isTimeNow(int8_t i) { int n = nowMin(); return n >= pt[i] && n < pt[i] + 5; }

long secsLeft(int8_t i) {
  long target = pt[i] * 60L, now = nowMin() * 60L + ss;
  if (target < now) target += 86400L;
  return target - now;
}

void fmt12(int minutes, char* out) {
  int h = minutes / 60, m = minutes % 60;
  int h12 = h % 12; if (!h12) h12 = 12;
  sprintf(out, "%d:%02d %s", h12, m, h < 12 ? "AM" : "PM");
}

bool prayersReady() { return pt[0] >= 0 && hh >= 0; }

void drawCountdown() {
  int8_t n = shownNext;
  bool now = isTimeNow(n);
  bool soon = now || secsLeft(n) <= 600;
  uint16_t border = soon ? PINK : CARD;
  tft.drawRoundRect(8, 46, 224, 96, 14, border);
  tft.drawRoundRect(9, 47, 222, 94, 13, border);

  tft.fillRect(110, 102, 112, 16, CARD);
  char t[12];
  if (now) {
    strcpy(t, "It's time");
    tft.setTextColor(PINK);
  } else {
    long s = secsLeft(n);
    sprintf(t, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
    tft.setTextColor(soon ? PINK : MINT);
  }
  tft.setTextSize(2);
  tft.setCursor(220 - (int)strlen(t) * 12, 102);
  tft.print(t);
}

void drawPrayer() {
  title(F("Prayer"));
  tft.fillCircle(206, 22, 11, CREAM);
  tft.fillCircle(212, 18, 10, gradAt(18));

  if (!prayersReady()) {
    tft.fillRoundRect(8, 46, 224, 96, 14, CARD);
    tft.setTextSize(2); tft.setTextColor(LAV);
    tft.setCursor(26, 86); tft.print(F("Getting times..."));
    return;
  }

  int8_t n = nextPrayer();
  shownNext = n;
  char t[10];

  tft.fillRoundRect(8, 46, 224, 96, 14, CARD);
  tft.setTextSize(1); tft.setTextColor(MUTED);
  tft.setCursor(20, 56);  tft.print(F("NEXT PRAYER"));
  tft.setCursor(166, 56); tft.print(F("TIME LEFT"));
  tft.setTextSize(3); tft.setTextColor(CREAM);
  tft.setCursor(20, 70); tft.print(PN[n]);
  fmt12(pt[n], t);
  tft.setTextSize(2); tft.setTextColor(PEACH);
  tft.setCursor(20, 102); tft.print(t);
  drawCountdown();

  tft.fillRoundRect(8, 150, 224, 124, 14, CARD);
  for (int8_t i = 0; i < 5; i++) {
    int y = 156 + i * 23;
    if (i == n) tft.fillRoundRect(14, y - 3, 212, 22, 11, PINK);
    tft.setTextSize(2);
    tft.setTextColor(i == n ? INK : LAV);
    tft.setCursor(24, y); tft.print(PN[i]);
    fmt12(pt[i], t);
    tft.setTextColor(i == n ? INK : CREAM);
    tft.setCursor(216 - (int)strlen(t) * 12, y); tft.print(t);
  }
}

void drawPage();

void prayerTick() {
  if (!prayersReady()) return;
  int8_t n = nextPrayer();

  if ((isTimeNow(n) || secsLeft(n) <= 600) && autoFor != n) {
    autoFor = n;
    if (page != 1) { page = 1; drawPage(); return; }
  }

  if (page == 1) {
    if (n != shownNext) drawPage();
    else drawCountdown();
  }
}

// ================= FOCUS =================
#define RCX 120
#define RCY 125
#define RR 68
#define NDOTS 40

int litCount() { return (int)((tRemain * NDOTS + tDur - 1) / tDur); }

void ringDot(int i, uint16_t c) {
  float a = -1.5708 + i * 6.2832 / NDOTS;
  tft.fillCircle(RCX + cos(a) * RR, RCY + sin(a) * RR, 3, c);
}

void drawRing(bool full) {
  int lit = litCount();
  if (full) { for (int i = 0; i < NDOTS; i++) ringDot(i, i < lit ? PINK : DIMRING); }
  else if (lit != litDrawn) {
    int a = min(lit, litDrawn), b = max(lit, litDrawn);
    for (int i = a; i < b && i < NDOTS; i++) ringDot(i, i < lit ? PINK : DIMRING);
  }
  litDrawn = lit;
}

void drawFocusTime() {
  unsigned long s = (tRemain + 999) / 1000;
  shownSec = s;
  char t[6]; sprintf(t, "%02lu:%02lu", s / 60, s % 60);
  tft.setTextSize(4); tft.setTextColor(CREAM, CARD);
  tft.setCursor(RCX - 60, RCY - 18); tft.print(t);
}

void drawFocusLabel() {
  tft.fillRect(RCX - 50, RCY + 22, 100, 10, CARD);
  const __FlashStringHelper* l;
  if (tRemain == 0) l = F("well done, Adam!");
  else if (tMode == 0) l = F("focus time");
  else if (tMode == 1) l = F("short break");
  else l = F("long break");
  tft.setTextSize(1); tft.setTextColor(tRemain == 0 ? MINT : LAV);
  tft.setCursor(RCX - (int)strlen_P((PGM_P)l) * 3, RCY + 23);
  tft.print(l);
}

void drawFocusControls() {
  pill(8, 205, 68, 32,   tMode == 0 ? PINK : CARD, tMode == 0 ? INK : LAV, F("25m"));
  pill(86, 205, 68, 32,  tMode == 1 ? PINK : CARD, tMode == 1 ? INK : LAV, F("5m"));
  pill(164, 205, 68, 32, tMode == 2 ? PINK : CARD, tMode == 2 ? INK : LAV, F("15m"));
  const __FlashStringHelper* s = tRun ? F("Pause") : (tRemain < tDur && tRemain > 0 ? F("Resume") : F("Start"));
  pill(8, 245, 140, 30, PEACH, INK, s);
  pill(156, 245, 76, 30, CARD, LAV, F("Reset"));
}

void drawFocus() {
  title(F("Focus"));
  tft.fillCircle(RCX, RCY, 56, CARD);
  drawRing(true); drawFocusTime(); drawFocusLabel(); drawFocusControls();
}

void focusTouch(int x, int y) {
  if (y >= 205 && y < 237) {
    tMode = x < 80 ? 0 : (x < 158 ? 1 : 2);
    tDur = (tMode == 0 ? 25UL : (tMode == 1 ? 5UL : 15UL)) * 60000UL;
    tRemain = tDur; tRun = false;
  } else if (y >= 245 && y < 275) {
    if (x < 150) {
      if (tRemain == 0) tRemain = tDur;
      tRun = !tRun; tLast = millis();
    } else { tRun = false; tRemain = tDur; }
  } else return;
  drawRing(true); drawFocusTime(); drawFocusLabel(); drawFocusControls();
}

// ================= PC =================
const __FlashStringHelper* pcLabel(uint8_t i) {
  switch (i) { case 0: return F("CPU"); case 1: return F("RAM"); case 2: return F("Disk"); default: return F("Battery"); }
}

void pcRowValue(uint8_t i) {
  int y = 48 + i * 56, v = pcv[i];
  char s[6];
  if (v < 0) strcpy(s, "  --"); else sprintf(s, "%3d%%", v);
  tft.setTextSize(2); tft.setTextColor(CREAM, CARD);
  tft.setCursor(170, y + 8); tft.print(s);
  uint16_t col = (i == 3) ? (v < 20 ? PINK : MINT) : (v < 60 ? MINT : (v < 85 ? PEACH : PINK));
  tft.fillRoundRect(20, y + 32, 200, 8, 4, DIMRING);
  if (v > 0) tft.fillRoundRect(20, y + 32, max(8, v * 2), 8, 4, col);
}

void drawPCPage() {
  title(F("Your PC"));
  for (uint8_t i = 0; i < 4; i++) {
    int y = 48 + i * 56;
    tft.fillRoundRect(8, y, 224, 48, 12, CARD);
    tft.setTextSize(2); tft.setTextColor(LAV);
    tft.setCursor(20, y + 8); tft.print(pcLabel(i));
    pcRowValue(i);
  }
}

// ================= PAGES =================
void drawPage() {
  bgRegion(0, 319);
  switch (page) {
    case 0: drawHome(); break;
    case 1: drawPrayer(); break;
    case 2: drawFocus(); break;
    case 3: drawPCPage(); break;
  }
  drawNav();
}

void handleTouch(int x, int y) {
  if (y >= 280) {
    if (x >= 8) {
      uint8_t t = (x - 8) / 56;
      if (t < 4 && t != page) { page = t; drawPage(); }
    }
    return;
  }
  if (page == 0 && y >= 214 && y < 272) {
    tft.drawRoundRect(8, 214, 224, 58, 14, PINK);
    sendCmd(F("NEXTMSG"));
  }
  else if (page == 2) focusTouch(x, y);
}

// ================= SERIAL FROM PC =================
void handleLine(char* line) {
  if (line[1] != ':') return;
  char* d = line + 2;
  lastData = millis();
  online = true;

  switch (line[0]) {
    case 'T': {
      int h = atoi(d), m = atoi(d + 3), s = atoi(d + 6);
      int prevS = ss; ss = s;
      if (h != hh || m != mm) {
        hh = h; mm = m;
        if (page == 0) { drawClockCard(); if (greetIndex() != lastGreet) drawGreeting(); }
      } else if (page == 0 && s != prevS) secStep(prevS);
      if (s != prevS) prayerTick();
      break;
    }
    case 'D':
      if (strcmp(dateStr, d) != 0) {
        strncpy(dateStr, d, 15); dateStr[15] = 0;
        if (page == 0) drawClockCard();
      }
      break;
    case 'W': {
      char* t = strtok(d, ",");
      char* h = strtok(NULL, ",");
      char* c = strtok(NULL, ",");
      char* w = strtok(NULL, ",");
      char* ic = strtok(NULL, ",");
      if (t) wTemp = atof(t);
      if (h) wHum = atoi(h);
      if (c) { strncpy(wCond, c, 15); wCond[15] = 0; }
      if (w) wWind = atoi(w);
      if (ic) wIcon = atoi(ic);
      if (page == 0) drawWeather();
      break;
    }
    case 'P': {
      char* v = strtok(d, ",");
      for (uint8_t i = 0; i < 4 && v; i++) { pcv[i] = atoi(v); v = strtok(NULL, ","); }
      if (page == 3) for (uint8_t i = 0; i < 4; i++) pcRowValue(i);
      break;
    }
    case 'Q': {
      bool changed = false;
      char* v = strtok(d, ",");
      for (uint8_t i = 0; i < 5 && v; i++) {
        int m = atoi(v) * 60 + atoi(v + 3);
        if (m != pt[i]) { pt[i] = m; changed = true; }
        v = strtok(NULL, ",");
      }
      if (changed && page == 1) drawPage();
      break;
    }
    case 'M':
      strncpy(msg, d, 39); msg[39] = 0;
      if (page == 0) drawMessage();
      break;
  }
}

void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { buf[bi] = 0; handleLine(buf); bi = 0; }
    else if (c != '\r' && bi < 79) buf[bi++] = c;
  }
}

// ================= MAIN =================
void setup() {
  Serial.begin(115200);
  tft.begin(0x7575);
  tft.setRotation(ROT);

  EEPROM.get(0, cal);
  int hits = 0, x, y;
  for (int i = 0; i < 5; i++) { if (readRaw(x, y)) hits++; delay(20); }
  if (cal.magic != CAL_MAGIC || hits >= 3) calibrate();

  drawPage();
}

void loop() {
  readSerial();

  if (tRun) {
    unsigned long now = millis(), dt = now - tLast;
    tLast = now;
    if (dt >= tRemain) {
      tRemain = 0; tRun = false;
      sendCmd(F("TIMER_DONE"));
      page = 2; drawPage();
    } else {
      tRemain -= dt;
      if (page == 2 && (tRemain + 999) / 1000 != shownSec) { drawFocusTime(); drawRing(false); }
    }
  }

  if (online && millis() - lastData > 5000) {
    online = false;
    strcpy(msg, "Waiting for your PC...");
    if (page == 0) drawMessage();
  }

  int x, y;
  if (getTouch(x, y)) { handleTouch(x, y); waitRelease(); }
}
