// Sparloggning: positionen over tid, som en plotter pa sjon.
//
// Sparet ar en egen loggning vid sidan av g-kraftsloggen. De gar att starta och
// stoppa var for sig, och de kan mycket val vara igang samtidigt - g-krafterna
// beskriver vad som hande, sparet var det hande.
//
// Filerna skrivs i tva format: GPX som kan slappas rakt in i en sjokortsapp
// eller Google Earth, och CSV med samma punkter for den som vill rakna sjalv.

#pragma once

#include <Arduino.h>

struct TrackStatus {
  bool logging;
  bool waitingForFix;   // loggar, men mottagaren har ingen position an
  uint32_t points;      // antal sparade punkter
  uint32_t elapsedS;    // sparets langd i sekunder
  double distanceM;     // tillryggalagd stracka
  float speedKmh;       // senast sparade hastighet
  char fileName[40];    // gpx-filen som skrivs
};

namespace track {

// Anropas en gang vid start, innan avlasningstraden drar igang.
void begin();

// Startar och stoppar sparloggningen. Sjalva filoppningen sker i
// avlasningstraden, sa att minneskortet bara ror sig fran ett hall.
bool start();
void stop();
bool isLogging();

// Anropas fran avlasningstraden, en gang per varv. Bestammer sjalv hur ofta en
// punkt faktiskt behover sparas.
void tick();

// Stanger filen ordentligt, t.ex. innan kortet plockas ut.
void closeFiles();

TrackStatus status();

}  // namespace track
