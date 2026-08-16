#include "ui.h"

#include "config.h"

namespace {

Arduino_Canvas *gfx = nullptr;

const uint16_t C_BG = RGB565(8, 12, 20);
const uint16_t C_PANEL = RGB565(22, 30, 44);
const uint16_t C_ACCENT = RGB565(60, 160, 255);
const uint16_t C_GREEN = RGB565(40, 190, 90);
const uint16_t C_RED = RGB565(230, 60, 60);
const uint16_t C_TEXT = RGB565(240, 245, 250);
const uint16_t C_DIM = RGB565(150, 165, 185);
const uint16_t C_WARN = RGB565(250, 190, 60);

// Det inbyggda typsnittet har en glyf per byte enligt teckentabellen CP437.
// De svenska tecknen finns dar, men pa andra platser an i UTF-8, sa texten
// oversatts innan den skrivs ut.
const char *sv(const char *utf8) {
  static char buf[96];
  size_t o = 0;
  for (size_t i = 0; utf8[i] && o < sizeof(buf) - 1;) {
    const uint8_t c = (uint8_t)utf8[i];
    if (c == 0xC3 && utf8[i + 1]) {
      const uint8_t n = (uint8_t)utf8[i + 1];
      char out = '?';
      switch (n) {
        case 0xA5: out = (char)0x86; break;  // a med ring
        case 0xA4: out = (char)0x84; break;  // a med prickar
        case 0xB6: out = (char)0x94; break;  // o med prickar
        case 0x85: out = (char)0x8F; break;  // A med ring
        case 0x84: out = (char)0x8E; break;  // A med prickar
        case 0x96: out = (char)0x99; break;  // O med prickar
        default: out = '?'; break;
      }
      buf[o++] = out;
      i += 2;
    } else if (c == 0xC2 && (uint8_t)utf8[i + 1] == 0xB0) {
      buf[o++] = (char)0xF8;  // gradtecken
      i += 2;
    } else {
      buf[o++] = utf8[i++];
    }
  }
  buf[o] = '\0';
  return buf;
}

int16_t textWidth(const char *s, uint8_t size) {
  return (int16_t)(strlen(s) * 6 * size);
}

void printAt(int16_t x, int16_t y, uint8_t size, uint16_t color,
             const char *text) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x, y);
  gfx->print(sv(text));
}

void printCentered(int16_t cx, int16_t y, uint8_t size, uint16_t color,
                   const char *text) {
  const char *converted = sv(text);
  printAt(cx - textWidth(converted, size) / 2, y, size, color, text);
}

void printRight(int16_t rx, int16_t y, uint8_t size, uint16_t color,
                const char *text) {
  const char *converted = sv(text);
  printAt(rx - textWidth(converted, size), y, size, color, text);
}

void drawPanel(const Rect &r, uint16_t fill) {
  gfx->fillRoundRect(r.x, r.y, r.w, r.h, 14, fill);
}

void drawButton(const Rect &r, uint16_t fill, const char *label, uint8_t size,
                uint16_t textColor) {
  drawPanel(r, fill);
  const uint8_t charH = 8 * size;
  printCentered(r.x + r.w / 2, r.y + (r.h - charH) / 2, size, textColor, label);
}

// "14 dygn 6 tim", "5 tim 20 min", "12 min", "45 s"
void fmtDuration(uint64_t s, char *out, size_t len) {
  if (s == 0) {
    snprintf(out, len, "-");
  } else if (s < 60) {
    snprintf(out, len, "%lu s", (unsigned long)s);
  } else if (s < 3600) {
    snprintf(out, len, "%lu min", (unsigned long)(s / 60));
  } else if (s < 86400) {
    snprintf(out, len, "%lu tim %lu min", (unsigned long)(s / 3600),
             (unsigned long)((s % 3600) / 60));
  } else {
    snprintf(out, len, "%lu dygn %lu tim", (unsigned long)(s / 86400),
             (unsigned long)((s % 86400) / 3600));
  }
}

// Loggningens langd som 00:12:34
void fmtElapsed(uint32_t s, char *out, size_t len) {
  snprintf(out, len, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600),
           (unsigned long)((s % 3600) / 60), (unsigned long)(s % 60));
}

void fmtBytes(uint64_t b, char *out, size_t len) {
  if (b >= 1024ULL * 1024ULL * 1024ULL) {
    snprintf(out, len, "%.1f GB", (double)b / (1024.0 * 1024.0 * 1024.0));
  } else if (b >= 1024ULL * 1024ULL) {
    snprintf(out, len, "%.1f MB", (double)b / (1024.0 * 1024.0));
  } else {
    snprintf(out, len, "%lu kB", (unsigned long)(b / 1024));
  }
}

// Tusentalsavgransare gor stora radantal lasbara.
void fmtCount(uint64_t n, char *out, size_t len) {
  char raw[24];
  snprintf(raw, sizeof(raw), "%llu", (unsigned long long)n);
  const size_t l = strlen(raw);
  size_t o = 0;
  for (size_t i = 0; i < l && o < len - 1; i++) {
    if (i > 0 && ((l - i) % 3) == 0) out[o++] = ' ';
    if (o < len - 1) out[o++] = raw[i];
  }
  out[o] = '\0';
}

}  // namespace

namespace ui {

const Rect kBtnStartStop = {16, 258, 418, 118};
const Rect kBtnSettings = {16, 388, 202, 54};
const Rect kBtnScreenOff = {232, 388, 202, 54};
const Rect kBtnBack = {16, 496, 418, 72};

Rect settingsMinus(uint8_t row) { return Rect{228, (int16_t)(88 + row * 92), 58, 58}; }
Rect settingsPlus(uint8_t row) { return Rect{376, (int16_t)(88 + row * 92), 58, 58}; }

void begin(Arduino_Canvas *canvas) { gfx = canvas; }

void drawMain(const Sample &s, const LoggerStatus &st, uint64_t secondsLeft,
              const String &clock) {
  if (!gfx) return;
  char buf[64];
  char tmp[32];

  gfx->fillScreen(C_BG);

  // ------------------------------------------------------------ rubrik ----
  printAt(16, 14, 3, C_TEXT, "G-LOGGER");
  // Klocksträngen ar "2026-08-16 18:30:05"; vi visar bara tiden.
  if (clock.length() >= 19) {
    printRight(434, 18, 2, C_DIM, clock.c_str() + 11);
  }

  // Liten prick som visar om minneskortet sitter i.
  gfx->fillCircle(434 - 0, 52, 6, st.sdMounted ? C_GREEN : C_RED);
  printRight(414, 46, 2, C_DIM, st.sdMounted ? "kort OK" : "inget kort");

  // ------------------------------------------------- levande matvarden ----
  const Rect live = {16, 74, 418, 172};
  drawPanel(live, C_PANEL);

  snprintf(buf, sizeof(buf), "%.2f g", s.atot);
  printCentered(225, 92, 7, C_TEXT, buf);

  snprintf(buf, sizeof(buf), "X %+.2f   Y %+.2f   Z %+.2f", s.ax, s.ay, s.az);
  printCentered(225, 162, 2, C_ACCENT, buf);

  snprintf(buf, sizeof(buf), "%+.0f  %+.0f  %+.0f grader/s", s.gx, s.gy, s.gz);
  printCentered(225, 190, 2, C_DIM, buf);

  snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", s.temp);
  printCentered(225, 216, 2, C_DIM, buf);

  // ------------------------------------------------ start- och stoppknapp --
  if (st.logging) {
    drawButton(kBtnStartStop, C_RED, "STOPPA", 5, C_TEXT);
  } else if (st.sdMounted) {
    drawButton(kBtnStartStop, C_GREEN, "STARTA", 5, C_TEXT);
  } else {
    drawButton(kBtnStartStop, RGB565(70, 70, 80), "SATT I KORT", 3, C_DIM);
  }

  // ------------------------------------------------------ smaknapparna ----
  drawButton(kBtnSettings, C_PANEL, st.logging ? "MENY (LAST)" : "MENY", 2,
             st.logging ? C_DIM : C_TEXT);
  drawButton(kBtnScreenOff, C_PANEL, "SLACK SKARM", 2, C_TEXT);

  // ------------------------------------------------------------ status ----
  const Rect status = {16, 456, 418, 132};
  drawPanel(status, C_PANEL);

  int16_t y = 470;
  if (st.logging) {
    fmtElapsed(st.elapsedS, tmp, sizeof(tmp));
    snprintf(buf, sizeof(buf), "Loggar  %s", tmp);
    printAt(32, y, 2, C_GREEN, buf);
    y += 26;

    fmtCount(st.rows, tmp, sizeof(tmp));
    char sizeStr[24];
    fmtBytes(st.bytes, sizeStr, sizeof(sizeStr));
    snprintf(buf, sizeof(buf), "%s rader  /  %s", tmp, sizeStr);
    printAt(32, y, 2, C_TEXT, buf);
    y += 26;

    snprintf(buf, sizeof(buf), "Fil: %s", st.fileName);
    printAt(32, y, 2, C_DIM, buf);
    y += 26;
  } else {
    printAt(32, y, 2, C_DIM, "Redo att logga");
    y += 26;

    fmtBytes(st.freeBytes, tmp, sizeof(tmp));
    char total[24];
    fmtBytes(st.cardBytes, total, sizeof(total));
    snprintf(buf, sizeof(buf), "Ledigt: %s av %s", tmp, total);
    printAt(32, y, 2, C_TEXT, buf);
    y += 26;

    snprintf(buf, sizeof(buf), "Frekvens: %u Hz", (unsigned)logger::rate());
    printAt(32, y, 2, C_DIM, buf);
    y += 26;
  }

  fmtDuration(secondsLeft, tmp, sizeof(tmp));
  snprintf(buf, sizeof(buf), "Utrymmet racker: %s", tmp);
  printAt(32, y, 2, secondsLeft < 3600 ? C_WARN : C_ACCENT, buf);

  if (st.writeErrors > 0) {
    snprintf(buf, sizeof(buf), "%lu skrivfel", (unsigned long)st.writeErrors);
    printRight(418, 470, 2, C_WARN, buf);
  }

  gfx->flush();
}

void drawSettings(const AppSettings &cfg) {
  if (!gfx) return;
  char buf[48];

  gfx->fillScreen(C_BG);
  printAt(16, 14, 3, C_TEXT, "INSTÄLLNINGAR");

  const char *labels[4] = {"Loggfrekvens", "Mätområde G", "Mätområde gyro",
                           "Släck skärm"};

  char values[4][24];
  snprintf(values[0], sizeof(values[0]), "%u Hz", (unsigned)kRates[cfg.rateIdx]);
  snprintf(values[1], sizeof(values[1]), "%u g",
           (unsigned)kAccelRanges[cfg.accelIdx]);
  snprintf(values[2], sizeof(values[2]), "%u",
           (unsigned)kGyroRanges[cfg.gyroIdx]);
  if (kScreenTimeouts[cfg.screenIdx] == 0) {
    snprintf(values[3], sizeof(values[3]), "aldrig");
  } else {
    snprintf(values[3], sizeof(values[3]), "%u s",
             (unsigned)kScreenTimeouts[cfg.screenIdx]);
  }

  for (uint8_t row = 0; row < 4; row++) {
    const int16_t y = 88 + row * 92;
    const Rect panel = {16, y, 418, 72};
    drawPanel(panel, C_PANEL);

    printAt(32, y + 10, 2, C_TEXT, labels[row]);
    printAt(32, y + 40, 2, C_DIM, values[row]);

    const Rect minus = settingsMinus(row);
    const Rect plus = settingsPlus(row);
    drawButton(minus, C_ACCENT, "-", 3, C_BG);
    drawButton(plus, C_ACCENT, "+", 3, C_BG);

    // Vardet stort mellan knapparna.
    printCentered(331, y + 26, 3, C_TEXT, values[row]);
  }

  drawButton(kBtnBack, C_GREEN, "KLAR", 4, C_TEXT);
  gfx->flush();
}

void drawMessage(const char *title, const char *line1, const char *line2) {
  if (!gfx) return;
  gfx->fillScreen(C_BG);
  printCentered(225, 230, 4, C_WARN, title);
  if (line1) printCentered(225, 300, 2, C_TEXT, line1);
  if (line2) printCentered(225, 330, 2, C_DIM, line2);
  gfx->flush();
}

}  // namespace ui
