// Valfri GPS-mottagare (u-blox, t.ex. SparkFun NEO-M9N) pa i2c-bussen.
//
// Modulen letas upp automatiskt vid start. Sitter ingen GPS inkopplad
// fungerar allt precis som vanligt - loggen far bara farre kolumner.

#pragma once

#include <Arduino.h>

struct GnssFix {
  bool valid;        // sant nar mottagaren har en anvandbar position
  uint8_t fixType;   // 0 = ingen, 2 = tvadimensionell, 3 = tredimensionell
  uint8_t sats;      // antal satelliter som anvands
  double lat;        // grader
  double lon;        // grader
  float altM;        // hojd over havet, meter
  float speedKmh;    // hastighet over marken
};

namespace gnss {

// Letar efter en mottagare pa i2c. Returnerar false om ingen finns, vilket
// inte ar ett fel utan bara betyder att kortet kor utan GPS.
bool begin();

bool present();

// Anropas ofta. Gor av med i2c-trafik hogst en gang per sekund, sa att
// loggtakten inte paverkas.
void poll();

GnssFix fix();

// Sann nar mottagaren har satellittid, vilket ar exakt till sekunden.
bool timeValid();
void utc(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour,
         uint8_t &minute, uint8_t &second);

}  // namespace gnss
