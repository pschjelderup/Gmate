#include "ui.h"

#include <math.h>

#include "config.h"
#include "gnss.h"
#include "track.h"

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
    } else if (c == 0xC2 && utf8[i + 1]) {
      const uint8_t n = (uint8_t)utf8[i + 1];
      char out = '?';
      switch (n) {
        case 0xB0: out = (char)0xF8; break;  // gradtecken
        case 0xB7: out = (char)0xFA; break;  // mittpunkt, som avdelare
        default: out = '?'; break;
      }
      buf[o++] = out;
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

// Stracka: meter sa lange det ar kort, annars kilometer.
void fmtDistance(double meters, char *out, size_t len) {
  if (meters < 1000.0) {
    snprintf(out, len, "%.0f m", meters);
  } else {
    snprintf(out, len, "%.2f km", meters / 1000.0);
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

// Sparknappen far plats genom att de levande matvardena och g-kraftsknappen
// kortats nagot. Ordningen uppifran ar mätvärden, g-kraft, spar, smaknappar,
// status.
const Rect kBtnStartStop = {16, 236, 418, 94};
const Rect kBtnTrack = {16, 338, 418, 64};
// Tre smaknappar i stallet for tva. Eco behover na med ett tryck - den ska
// anvandas medan man kor, och da ar tva tryck ett for mycket.
const Rect kBtnSettings = {16, 410, 134, 48};
const Rect kBtnEco = {158, 410, 134, 48};
const Rect kBtnScreenOff = {300, 410, 134, 48};
const Rect kBtnTare = {16, 434, 418, 48};
const Rect kBtnBack = {16, 512, 418, 72};

const Rect kBtnEcoReset = {16, 528, 202, 56};
const Rect kBtnEcoBack = {232, 528, 202, 56};

Rect settingsMinus(uint8_t row) { return Rect{228, (int16_t)(88 + row * 92), 58, 58}; }
Rect settingsPlus(uint8_t row) { return Rect{376, (int16_t)(88 + row * 92), 58, 58}; }

void begin(Arduino_Canvas *canvas) { gfx = canvas; }

void drawMain(const Sample &s, const LoggerStatus &st, uint64_t secondsLeft,
              const String &clock) {
  if (!gfx) return;
  char buf[64];
  char tmp[32];

  const TrackStatus tr = track::status();
  const bool gpsPresent = gnss::present();
  const GnssFix fix = gnss::fix();

  gfx->fillScreen(C_BG);

  // ------------------------------------------------------------ rubrik ----
  printAt(16, 14, 3, C_TEXT, "G-LOGGER");
  // Klocksträngen ar "2026-08-16 18:30:05"; vi visar bara tiden.
  if (clock.length() >= 19) {
    printRight(434, 18, 2, C_DIM, clock.c_str() + 11);
  }

  // Kortet till hoger, GPS till vanster. Bada visas alltid, aven nar de
  // saknas - annars gar det inte att se skillnad pa "ingen mottagare
  // inkopplad" och "mottagare inkopplad men utan position".
  gfx->fillCircle(434, 52, 6, st.sdMounted ? C_GREEN : C_RED);
  printRight(414, 46, 2, C_DIM, st.sdMounted ? "kort OK" : "inget kort");

  uint16_t gpsDot;
  uint16_t gpsText;
  if (!gpsPresent) {
    gpsDot = RGB565(90, 100, 115);
    gpsText = C_DIM;
    snprintf(buf, sizeof(buf), "ingen GPS");
  } else if (fix.valid) {
    gpsDot = C_GREEN;
    gpsText = C_TEXT;
    snprintf(buf, sizeof(buf), "GPS %u sat", (unsigned)fix.sats);
  } else {
    gpsDot = C_WARN;
    gpsText = C_DIM;
    snprintf(buf, sizeof(buf), "GPS söker (%u sat)", (unsigned)fix.sats);
  }
  gfx->fillCircle(22, 52, 6, gpsDot);
  printAt(36, 46, 2, gpsText, buf);

  // ------------------------------------------------- levande matvarden ----
  const Rect live = {16, 74, 418, 152};
  drawPanel(live, C_PANEL);

  snprintf(buf, sizeof(buf), "%.2f g", s.atot);
  printCentered(225, 84, 7, C_TEXT, buf);

  snprintf(buf, sizeof(buf), "X %+.2f   Y %+.2f   Z %+.2f", s.ax, s.ay, s.az);
  printCentered(225, 148, 2, C_ACCENT, buf);

  snprintf(buf, sizeof(buf), "%+.0f  %+.0f  %+.0f grader/s", s.gx, s.gy, s.gz);
  printCentered(225, 172, 2, C_DIM, buf);

  if (gpsPresent && fix.valid) {
    snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C   ·   %.1f km/h", s.temp,
             fix.speedKmh);
  } else {
    snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", s.temp);
  }
  printCentered(225, 196, 2, C_DIM, buf);

  // ------------------------------------------------ start- och stoppknapp --
  if (st.logging) {
    drawButton(kBtnStartStop, C_RED, "STOPPA", 5, C_TEXT);
  } else if (st.sdMounted) {
    drawButton(kBtnStartStop, C_GREEN, "STARTA", 5, C_TEXT);
  } else {
    drawButton(kBtnStartStop, RGB565(70, 70, 80), "SATT I KORT", 3, C_DIM);
  }

  // ---------------------------------------------------------- sparknapp ----
  if (!gpsPresent) {
    // Utan mottagare finns inget spar att rita, sa knappen ar slack i stallet
    // for att lova nagot den inte kan halla.
    drawButton(kBtnTrack, RGB565(46, 52, 64), "SPÅR: INGEN GPS", 2, C_DIM);
  } else if (tr.logging) {
    drawButton(kBtnTrack, C_RED, "STOPPA SPÅR", 3, C_TEXT);
  } else {
    drawButton(kBtnTrack, C_ACCENT, "STARTA SPÅR", 3, C_BG);
  }

  // ------------------------------------------------------ smaknapparna ----
  drawButton(kBtnSettings, C_PANEL, st.logging ? "LÅST" : "MENY", 2,
             st.logging ? C_DIM : C_TEXT);
  drawButton(kBtnEco, C_PANEL, "ECO", 2, C_TEXT);
  drawButton(kBtnScreenOff, C_PANEL, "SLÄCK", 2, C_TEXT);

  // ------------------------------------------------------------ status ----
  const Rect status = {16, 466, 418, 126};
  drawPanel(status, C_PANEL);

  int16_t y = 478;
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

  } else {
    printAt(32, y, 2, C_DIM, "Redo att logga");
    y += 26;

    fmtBytes(st.freeBytes, tmp, sizeof(tmp));
    char total[24];
    fmtBytes(st.cardBytes, total, sizeof(total));
    snprintf(buf, sizeof(buf), "Ledigt: %s av %s", tmp, total);
    printAt(32, y, 2, C_TEXT, buf);
    y += 26;
  }

  // Tredje raden gar till sparet nar det loggar, eftersom det da ar det man
  // vill folja. Annars star det som stod dar forut.
  if (tr.logging) {
    if (tr.waitingForFix) {
      printAt(32, y, 2, C_WARN, "Spår: väntar på position");
    } else {
      fmtDistance(tr.distanceM, tmp, sizeof(tmp));
      char pts[24];
      fmtCount(tr.points, pts, sizeof(pts));
      snprintf(buf, sizeof(buf), "Spår: %s · %s punkter", tmp, pts);
      printAt(32, y, 2, C_GREEN, buf);
    }
  } else if (st.logging) {
    snprintf(buf, sizeof(buf), "Fil: %s", st.fileName);
    printAt(32, y, 2, C_DIM, buf);
  } else {
    snprintf(buf, sizeof(buf), "Frekvens: %u Hz", (unsigned)logger::rate());
    printAt(32, y, 2, C_DIM, buf);
  }
  y += 26;

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

  // --------------------------------------------------------------- tara ----
  // Taran sparar hur kortet sitter just nu. Lutningen hittas visserligen av
  // sig sjalv efter en halv minut, men med ett sparat lage ar lodlinjen ratt
  // redan vid start - och framatriktningen lars om fran borjan, vilket ar det
  // man vill nar hallaren flyttats.
  const EcoStatus ec = eco::status();
  drawButton(kBtnTare, C_ACCENT, "TARA - STÅ STILL", 2, C_BG);
  if (ec.levelStored) {
    printCentered(225, 488, 2, C_GREEN, "Monteringsläget är sparat");
  } else {
    printCentered(225, 488, 2, C_DIM, "Inget läge sparat ännu");
  }

  drawButton(kBtnBack, C_GREEN, "KLAR", 4, C_TEXT);
  gfx->flush();
}

void drawEco(const EcoStatus &e) {
  if (!gfx) return;
  char buf[64];

  gfx->fillScreen(C_BG);

  // Fargen foljer hur hart du kor just nu och ar samma overallt pa skarmen,
  // sa att man uppfattar den i ogonvran utan att lasa nagot.
  uint16_t zone;
  if (e.magG < ECO_SOFT_G) {
    zone = C_GREEN;
  } else if (e.magG < ECO_HARD_G) {
    zone = C_WARN;
  } else {
    zone = C_RED;
  }

  printAt(16, 14, 3, C_TEXT, "ECODRIVE");

  // ---------------------------------------------------------------- poang --
  const int score = (int)(e.score + 0.5f);
  const char *grade;
  if (score >= 90) {
    grade = "UTMÄRKT";
  } else if (score >= 75) {
    grade = "BRA";
  } else if (score >= 60) {
    grade = "OK";
  } else if (score >= 40) {
    grade = "HACKIGT";
  } else {
    grade = "HÅRT";
  }

  uint16_t scoreColor = C_GREEN;
  if (score < 40) {
    scoreColor = C_RED;
  } else if (score < 75) {
    scoreColor = C_WARN;
  }

  snprintf(buf, sizeof(buf), "%d", score);
  printRight(434, 8, 6, scoreColor, buf);
  printRight(434, 56, 2, C_DIM, grade);

  // -------------------------------------------------------------- bubblan --
  // En vattenpassbubbla, fast tvarto: den ska sta still i mitten. Ju hardare
  // du kor, desto langre ut vandrar den. Ringarna ar 0,1 g var.
  const int16_t cx = 225;
  const int16_t cy = 268;
  const int16_t rOuter = 148;
  const float pxPerG = (float)rOuter / ECO_BUBBLE_FULL_G;

  for (int i = 1; i <= 4; i++) {
    const int16_t r = (int16_t)(rOuter * i / 4);
    // Ringen dar det slutar vara mjuk korning ritas tydligare an de andra.
    const bool isSoftRing = (i == 2);  // 0,2 g
    gfx->drawCircle(cx, cy, r, isSoftRing ? RGB565(90, 110, 135)
                                          : RGB565(45, 55, 72));
  }
  gfx->drawFastHLine(cx - rOuter, cy, rOuter * 2, RGB565(38, 46, 60));
  gfx->drawFastVLine(cx, cy - rOuter, rOuter * 2, RGB565(38, 46, 60));

  // Etiketterna satts ut forst nar kortet vet vilket hall som ar framat.
  // Innan dess vore de en gissning, och en felmarkt axel ar samre an ingen.
  if (e.forwardKnown) {
    printCentered(cx, cy - rOuter - 22, 2, C_DIM, "GAS");
    printCentered(cx, cy + rOuter + 6, 2, C_DIM, "BROMS");
  }

  if (e.levelled) {
    // Toppvardet lamnas kvar som en ring, sa att man ser hur hart det blev
    // aven nar bubblan hunnit tillbaka till mitten.
    if (e.peakG > 0.02f) {
      int16_t rp = (int16_t)(e.peakG * pxPerG);
      if (rp > rOuter) rp = rOuter;
      gfx->drawCircle(cx, cy, rp, RGB565(120, 90, 40));
    }

    float px = e.lonG * pxPerG;
    float py = e.latG * pxPerG;
    const float d = sqrtf(px * px + py * py);
    // Bubblan stannar vid ytterringen i stallet for att forsvinna ut ur
    // rutan - man vill se att det slog i taket, inte tappa den helt.
    if (d > rOuter) {
      px = px * rOuter / d;
      py = py * rOuter / d;
    }

    gfx->fillCircle(cx + (int16_t)py, cy - (int16_t)px, 17, zone);
    gfx->drawCircle(cx + (int16_t)py, cy - (int16_t)px, 17, C_TEXT);

    snprintf(buf, sizeof(buf), "%.2f g", e.magG);
    printCentered(cx, cy - 10, 3, C_TEXT, buf);
  } else {
    printCentered(cx, cy - 10, 2, C_DIM, "hittar lodlinjen ...");
  }

  // ------------------------------------------------------------- raknare --
  const Rect box = {16, 440, 418, 82};
  drawPanel(box, C_PANEL);

  if (e.gpsClassify) {
    snprintf(buf, sizeof(buf), "Hårt: gas %lu  broms %lu  kurva %lu",
             (unsigned long)e.hardAccel, (unsigned long)e.hardBrake,
             (unsigned long)e.hardTurn);
    printCentered(225, 450, 2, C_TEXT, buf);
  } else {
    // Utan GPS gar det inte att veta om ett ryck var gas, broms eller kurva.
    // Da sags det rakt ut i stallet for att gissa.
    snprintf(buf, sizeof(buf), "%lu hårda moment (ingen GPS)",
             (unsigned long)e.hardTotal);
    printCentered(225, 450, 2, C_TEXT, buf);
  }

  snprintf(buf, sizeof(buf), "Topp %.2f g", e.peakG);
  printCentered(225, 472, 2, C_ACCENT, buf);

  // Raden om riktningen ar det enda stallet dar man far veta om bubblan pekar
  // at ratt hall eller bara har ratt storlek. Utan den skulle en godtyckligt
  // vand bubbla se precis lika trovardig ut som en inlard.
  if (e.forwardKnown && e.forwardQuality >= 0.99f) {
    printCentered(225, 494, 2, C_GREEN, "Riktning: inlärd");
  } else if (e.forwardKnown) {
    snprintf(buf, sizeof(buf), "Riktning: lär sig %d%%",
             (int)(e.forwardQuality * 100.0f + 0.5f));
    printCentered(225, 494, 2, C_WARN, buf);
  } else if (e.forwardNeedsGnss) {
    printCentered(225, 494, 2, C_DIM, "Riktning: kräver GPS");
  } else {
    printCentered(225, 494, 2, C_DIM, "Riktning: kör, gasa och bromsa");
  }

  // ------------------------------------------------------------ knappar ---
  drawButton(kBtnEcoReset, C_PANEL, "NOLLSTÄLL", 2, C_TEXT);
  drawButton(kBtnEcoBack, C_ACCENT, "TILLBAKA", 2, C_BG);

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
