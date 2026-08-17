// Allt som ritas pa skarmen.

#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "eco.h"
#include "logger.h"

// Anvandarens installningar. Sparas i kortets flashminne och overlever
// stromavbrott.
struct AppSettings {
  uint8_t rateIdx;
  uint8_t accelIdx;
  uint8_t gyroIdx;
  uint8_t screenIdx;

  // Ecodrive-granserna. Egen meny, atkomlig fran ecodrive-skarmen, sa att de
  // gar att prova ut medan bilen rullar.
  uint8_t ecoSoftIdx;
  uint8_t ecoHardIdx;
  uint8_t ecoBubbleIdx;
  uint8_t ecoPenaltyIdx;
};

enum Screen {
  SCREEN_MAIN,
  SCREEN_SETTINGS,
  SCREEN_ECO,
  SCREEN_ECO_LIMITS,
};

// En ruta pa skarmen. Anvands bade for att rita knappar och for att avgora
// var anvandaren tryckte.
struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

namespace ui {

// Knappytor pa huvudskarmen.
extern const Rect kBtnStartStop;
extern const Rect kBtnTrack;
extern const Rect kBtnSettings;
extern const Rect kBtnEco;
extern const Rect kBtnScreenOff;

// Knappytor pa ecodrive-skarmen.
extern const Rect kBtnEcoReset;
extern const Rect kBtnEcoLimits;
extern const Rect kBtnEcoBack;

// Knappytor i installningsmenyn. Rad 0-3, minus och plus.
Rect settingsMinus(uint8_t row);
Rect settingsPlus(uint8_t row);
extern const Rect kBtnTare;
extern const Rect kBtnBack;

void begin(Arduino_Canvas *canvas);

void drawMain(const Sample &s, const LoggerStatus &st, uint64_t secondsLeft,
              const String &clock);
void drawSettings(const AppSettings &cfg);
void drawEco(const EcoStatus &e);
void drawEcoLimits(const AppSettings &cfg, const EcoStatus &e);

// Meddelande over hela skarmen, for fel som anvandaren maste atgarda.
void drawMessage(const char *title, const char *line1, const char *line2);

}  // namespace ui
