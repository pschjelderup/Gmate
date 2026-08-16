// Gmate - g-kraftslogger for Waveshare ESP32-S3-Touch-AMOLED-2.41
//
// Skarmen ar hela granssnittet: har startas och stoppas loggningen, och har
// visas hur mycket som loggats och hur lange utrymmet racker. Under en lang
// loggning kan skarmen slackas for att spara strom och slippa varme -
// loggningen fortsatter i bakgrunden och skarmen tands igen med ett tryck.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <TouchDrv.hpp>
#include <Wire.h>

#include "config.h"
#include "eco.h"
#include "logger.h"
#include "track.h"
#include "ui.h"

// ------------------------------------------------------------- skarmen ----
Arduino_DataBus *bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_D0,
                                             PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);

Arduino_RM690B0 *panel =
    new Arduino_RM690B0(bus, PIN_LCD_RST, 0 /* rotation */, SCREEN_W, SCREEN_H,
                        LCD_COL_OFFSET, 0, LCD_COL_OFFSET, 0);

// Allt ritas forst i en bildbuffert i psram och skickas sedan till skarmen i
// ett svep. Det ger en bild utan flimmer.
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, panel, 0, 0, 0);

TouchDrvFT6X36 touch;
bool touchOk = false;

Preferences prefs;
AppSettings cfg = {DEFAULT_RATE_INDEX, DEFAULT_ACCEL_RANGE_INDEX,
                   DEFAULT_GYRO_RANGE_INDEX, DEFAULT_SCREEN_TIMEOUT_INDEX};

Screen screen = SCREEN_MAIN;
bool screenOn = true;
uint32_t lastActivityMs = 0;
uint32_t lastDrawMs = 0;
bool wasTouched = false;
bool lastButtonState = HIGH;

// --------------------------------------------------------- installningar --

void loadSettings() {
  prefs.begin("gmate", true);
  cfg.rateIdx = prefs.getUChar("rate", DEFAULT_RATE_INDEX);
  cfg.accelIdx = prefs.getUChar("accel", DEFAULT_ACCEL_RANGE_INDEX);
  cfg.gyroIdx = prefs.getUChar("gyro", DEFAULT_GYRO_RANGE_INDEX);
  cfg.screenIdx = prefs.getUChar("screen", DEFAULT_SCREEN_TIMEOUT_INDEX);
  prefs.end();

  // Ett trasigt eller gammalt sparat varde far inte gora appen obrukbar.
  if (cfg.rateIdx >= kRateCount) cfg.rateIdx = DEFAULT_RATE_INDEX;
  if (cfg.accelIdx >= kAccelRangeCount) cfg.accelIdx = DEFAULT_ACCEL_RANGE_INDEX;
  if (cfg.gyroIdx >= kGyroRangeCount) cfg.gyroIdx = DEFAULT_GYRO_RANGE_INDEX;
  if (cfg.screenIdx >= kScreenTimeoutCount) {
    cfg.screenIdx = DEFAULT_SCREEN_TIMEOUT_INDEX;
  }
}

void saveSettings() {
  prefs.begin("gmate", false);
  prefs.putUChar("rate", cfg.rateIdx);
  prefs.putUChar("accel", cfg.accelIdx);
  prefs.putUChar("gyro", cfg.gyroIdx);
  prefs.putUChar("screen", cfg.screenIdx);
  prefs.end();
}

void applySettings() {
  logger::setRate(kRates[cfg.rateIdx]);
  logger::setRanges(kAccelRanges[cfg.accelIdx], kGyroRanges[cfg.gyroIdx]);
}

// -------------------------------------------------------- skarm av och pa --

void setScreen(bool on) {
  if (on == screenOn) return;
  screenOn = on;
  if (on) {
    panel->displayOn();
    panel->setBrightness(200);
    lastDrawMs = 0;  // tvinga omritning direkt
  } else {
    panel->setBrightness(0);
    panel->displayOff();
  }
  lastActivityMs = millis();
}

// ------------------------------------------------------------- pekskarm ---

// Oversatter fran pekskarmens koordinater till skarmens, ifall panelen ar
// monterad speglad eller vriden.
void mapTouch(int16_t &x, int16_t &y) {
#if TOUCH_SWAP_XY
  const int16_t t = x;
  x = y;
  y = t;
#endif
#if TOUCH_FLIP_X
  x = SCREEN_W - 1 - x;
#endif
#if TOUCH_FLIP_Y
  y = SCREEN_H - 1 - y;
#endif
}

void onPressMain(int16_t x, int16_t y) {
  if (ui::kBtnStartStop.contains(x, y)) {
    if (logger::isLogging()) {
      logger::stop();
    } else if (logger::sdMounted()) {
      logger::start();
    } else {
      logger::remount();
    }
    return;
  }

  if (ui::kBtnTrack.contains(x, y)) {
    // Sparet gar att starta och stoppa oberoende av g-kraftsloggen. De tva
    // svarar pa olika fragor och behover inte folja varandra.
    if (track::isLogging()) {
      track::stop();
    } else {
      track::start();
    }
    return;
  }

  if (ui::kBtnSettings.contains(x, y)) {
    // Under pagaende loggning far installningarna inte andras, annars skulle
    // filen fa olika frekvens i olika delar.
    if (!logger::isLogging()) screen = SCREEN_SETTINGS;
    return;
  }

  if (ui::kBtnEco.contains(x, y)) {
    screen = SCREEN_ECO;
    return;
  }

  if (ui::kBtnScreenOff.contains(x, y)) {
    setScreen(false);
    return;
  }
}

void onPressEco(int16_t x, int16_t y) {
  if (ui::kBtnEcoReset.contains(x, y)) {
    eco::reset();
    return;
  }
  if (ui::kBtnEcoBack.contains(x, y)) screen = SCREEN_MAIN;
}

void onPressSettings(int16_t x, int16_t y) {
  bool changed = false;

  for (uint8_t row = 0; row < 4; row++) {
    const bool minus = ui::settingsMinus(row).contains(x, y);
    const bool plus = ui::settingsPlus(row).contains(x, y);
    if (!minus && !plus) continue;

    uint8_t *value = nullptr;
    uint8_t count = 0;
    switch (row) {
      case 0: value = &cfg.rateIdx; count = kRateCount; break;
      case 1: value = &cfg.accelIdx; count = kAccelRangeCount; break;
      case 2: value = &cfg.gyroIdx; count = kGyroRangeCount; break;
      case 3: value = &cfg.screenIdx; count = kScreenTimeoutCount; break;
    }
    if (!value) continue;

    if (minus && *value > 0) {
      (*value)--;
      changed = true;
    } else if (plus && *value + 1 < count) {
      (*value)++;
      changed = true;
    }
    break;
  }

  if (changed) {
    applySettings();
    saveSettings();
    return;
  }

  if (ui::kBtnBack.contains(x, y)) screen = SCREEN_MAIN;
}

void handleTouch() {
  if (!touchOk) return;

  const TouchPoints &points = touch.getTouchPoints();
  const bool pressed = points.hasPoints();

  if (pressed && !wasTouched) {
    lastActivityMs = millis();

    if (!screenOn) {
      // Forsta trycket nar skarmen ar slackt tander bara skarmen, sa att man
      // inte rakar starta eller stoppa loggningen av misstag.
      setScreen(true);
    } else {
      const TouchPoint &p = points.getPoint(0);
      int16_t x = (int16_t)p.x;
      int16_t y = (int16_t)p.y;
      mapTouch(x, y);
      if (screen == SCREEN_MAIN) {
        onPressMain(x, y);
      } else if (screen == SCREEN_ECO) {
        onPressEco(x, y);
      } else {
        onPressSettings(x, y);
      }
    }
  }
  wasTouched = pressed;
}

void handleButton() {
  const bool state = digitalRead(PIN_BOOT_BUTTON);
  // Knappen drar ingangen till noll nar den trycks ned.
  if (state == LOW && lastButtonState == HIGH) {
    setScreen(!screenOn);
    delay(50);  // enkel studsfiltrering
  }
  lastButtonState = state;
}

// ------------------------------------------------------------------------ --

void setup() {
  Serial.begin(115200);

  // Skarmens matning maste sla pa forst av allt.
  pinMode(PIN_PANEL_POWER, OUTPUT);
  digitalWrite(PIN_PANEL_POWER, HIGH);
  delay(200);

  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  gfx->begin();
  gfx->fillScreen(RGB565(8, 12, 20));
  gfx->flush();
  panel->setBrightness(200);

  ui::begin(gfx);
  ui::drawMessage("GMATE", "startar ...", nullptr);

  loadSettings();

  const bool imuOk = logger::begin();
  applySettings();

  touch.setPins(PIN_TOUCH_RST, TOUCH_IRQ_NOT_CONNECTED);
  touchOk = touch.begin(Wire, FT6X36_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);

  if (!imuOk) {
    ui::drawMessage("SENSORFEL", "Rörelsesensorn svarar inte.",
                    "Starta om kortet.");
    delay(4000);
  }

  lastActivityMs = millis();
}

void loop() {
  handleButton();
  handleTouch();

  // Slack skarmen automatiskt under loggning, men bara om anvandaren valt en
  // tid. Nar ingen loggning pagar star skarmen kvar, sa att den aldrig kanns
  // dod nar man star och pillar med den.
  // Ecodrive-skarmen ar till for att tittas pa medan man kor, sa den slacks
  // aldrig av sig sjalv.
  const uint16_t timeout = kScreenTimeouts[cfg.screenIdx];
  if (screenOn && timeout > 0 && logger::isLogging() && screen != SCREEN_ECO &&
      millis() - lastActivityMs > (uint32_t)timeout * 1000UL) {
    setScreen(false);
  }

  if (screenOn && millis() - lastDrawMs >= 200) {
    lastDrawMs = millis();
    if (screen == SCREEN_MAIN) {
      ui::drawMain(logger::latest(), logger::status(),
                   logger::estimateSecondsLeft(), logger::nowString());
    } else if (screen == SCREEN_ECO) {
      ui::drawEco(eco::status());
    } else {
      ui::drawSettings(cfg);
    }
  }

  delay(10);
}
