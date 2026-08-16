// Ecodrive: hur mjukt du kor, medan du kor.
//
// Allt vilar pa en enda insikt: sparsam korning och mjuk korning ar samma sak.
// Bransle gar at nar farten andras - vid gaspadrag, inbromsning och kurvtagning
// - och ju haftigare andringen ar, desto mer gar at. Det gar alltsa att mata
// sparsamhet med enbart accelerometern, utan att veta nagot om motorn.
//
// Kortet far sitta hur som helst i bilen. Tyngdkraften visar vilket hall som ar
// ned, och den riktningen raknas bort, sa att det som blir kvar ar den
// vagrata accelerationen - det som faktiskt kanns i sätet.

#pragma once

#include <Arduino.h>

#include "logger.h"

struct EcoStatus {
  float score;      // 0-100, lever medan du kor
  float lonG;       // vagrat, langs kortets ena axel
  float latG;       // vagrat, langs den andra
  float magG;       // total vagrat acceleration
  float peakG;      // hogsta sedan nollstallningen
  bool levelled;    // sant nar tyngdkraften hittats och vardena ar palitliga

  // Handelser sedan nollstallningen. Uppdelningen kraver GPS; utan den
  // rapporteras allt som hardTotal.
  bool gpsClassify;
  uint32_t hardAccel;
  uint32_t hardBrake;
  uint32_t hardTurn;
  uint32_t hardTotal;

  uint32_t elapsedS;
};

namespace eco {

void begin();

// Anropas fran avlasningstraden, en gang per varv.
void tick(const Sample &s);

// Nollstaller poang, toppvarde och raknare. Tyngdkraftsriktningen behalls,
// eftersom den inte har med korningen att gora.
void reset();

EcoStatus status();

}  // namespace eco
