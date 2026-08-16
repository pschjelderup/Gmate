// Loggermotorn: laser av rorelsesensorn och skriver rader till minneskortet.
//
// All avlasning sker i en egen trad sa att loggningen haller jamn takt aven
// nar skarmen ritas om. Traden kor aven nar ingen loggning pagar, for da
// behover skarmen fortfarande fardska varden att visa.

#pragma once

#include <Arduino.h>

// Ett avlast varde fran sensorn.
struct Sample {
  float ax, ay, az;  // acceleration per axel, i g
  float atot;        // total acceleration, i g
  float gx, gy, gz;  // vridhastighet per axel, i grader/sekund
  float temp;        // sensorns temperatur, i grader Celsius
};

// Ogonblicksbild av loggerns tillstand, for skarmen att visa.
struct LoggerStatus {
  bool sdMounted;
  bool logging;
  uint64_t rows;         // antal skrivna rader denna loggning
  uint64_t bytes;        // antal skrivna byte denna loggning
  uint32_t elapsedS;     // loggningens langd i sekunder
  uint64_t freeBytes;    // ledigt pa kortet
  uint64_t cardBytes;    // kortets totala storlek
  uint32_t bytesPerRow;  // uppmatt radlangd, for tidsuppskattningen
  uint32_t writeErrors;  // antal misslyckade skrivningar
  char fileName[40];     // filen som skrivs just nu
};

namespace logger {

// Startar i2c, rorelsesensorn, klockan och minneskortet samt avlasningstraden.
// Returnerar false bara om rorelsesensorn inte svarar, eftersom allt annat gar
// att aterstalla i eftterhand.
bool begin();

bool imuOk();
bool sdMounted();

// Forsoker montera minneskortet igen, t.ex. efter att anvandaren stoppat i ett.
bool remount();

// Loggfrekvens i hertz. Gar bara att andra nar ingen loggning pagar.
void setRate(uint16_t hz);
uint16_t rate();

// Matomraden. Gar bara att andra nar ingen loggning pagar.
void setRanges(uint8_t accelG, uint16_t gyroDps);

bool start();
void stop();
bool isLogging();

Sample latest();
LoggerStatus status();

// Uppskattad aterstaende loggtid i sekunder, utifran ledigt utrymme, uppmatt
// radlangd och vald frekvens.
uint64_t estimateSecondsLeft();

// Aktuell tid fran kortets klocka, som "2026-08-16 18:30:05".
String nowString();

}  // namespace logger
