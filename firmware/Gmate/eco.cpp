#include "eco.h"

#include <esp_timer.h>
#include <math.h>

#include "config.h"
#include "gnss.h"

namespace {

SemaphoreHandle_t g_mutex = nullptr;

EcoStatus g_status = {};

// Tyngdkraftens riktning, framplockad ur accelerationen med ett langsamt
// filter. Korrorelser hinner inte paverka den; en langvarig backe gor det,
// vilket ar precis ratt - i en backe ar det backen som ar "ned".
float g_gx = 0, g_gy = 0, g_gz = 0;
bool g_haveGravity = false;

int64_t g_lastUs = 0;
uint32_t g_startMs = 0;

float g_score = 100.0f;
float g_peak = 0;

bool g_inEvent = false;

// GPS-fart, for att kunna skilja gaspadrag fran inbromsning. Bara tva varden
// behovs: mottagaren lamnar en ny fart en gang i sekunden.
float g_speedNow = 0, g_speedPrev = 0;
uint32_t g_speedNowMs = 0, g_speedPrevMs = 0;
bool g_haveSpeed = false;

void lock() {
  if (g_mutex) xSemaphoreTake(g_mutex, portMAX_DELAY);
}
void unlock() {
  if (g_mutex) xSemaphoreGive(g_mutex);
}

// Langsgaende acceleration ur GPS-farten, i g. Noll nar underlaget saknas.
// Tecknet ar det intressanta: plus ar gaspadrag, minus ar inbromsning.
float gpsLongitudinalG() {
  if (!g_haveSpeed || g_speedNowMs == g_speedPrevMs) return 0;
  const float dtS = (float)(g_speedNowMs - g_speedPrevMs) / 1000.0f;
  if (dtS < 0.2f || dtS > 5.0f) return 0;
  const float dvMs = (g_speedNow - g_speedPrev) / 3.6f;
  return (dvMs / dtS) / 9.80665f;
}

void updateSpeed() {
  if (!gnss::present()) {
    g_haveSpeed = false;
    return;
  }
  const GnssFix f = gnss::fix();
  if (!f.valid) {
    g_haveSpeed = false;
    return;
  }

  const uint32_t now = millis();
  // Mottagaren lamnar ett nytt varde ungefar en gang i sekunden. Vi flyttar
  // fram fonstret nar vardet faktiskt andrats, eller om det stat still lange.
  if (!g_haveSpeed) {
    g_speedPrev = g_speedNow = f.speedKmh;
    g_speedPrevMs = g_speedNowMs = now;
    g_haveSpeed = true;
    return;
  }
  if (f.speedKmh != g_speedNow || now - g_speedNowMs > 1500) {
    g_speedPrev = g_speedNow;
    g_speedPrevMs = g_speedNowMs;
    g_speedNow = f.speedKmh;
    g_speedNowMs = now;
  }
}

}  // namespace

namespace eco {

void begin() {
  if (g_mutex == nullptr) g_mutex = xSemaphoreCreateMutex();
  g_startMs = millis();
  lock();
  g_status.score = 100.0f;
  unlock();
}

void reset() {
  lock();
  g_score = 100.0f;
  g_peak = 0;
  g_inEvent = false;
  g_startMs = millis();
  g_status.score = 100.0f;
  g_status.peakG = 0;
  g_status.hardAccel = 0;
  g_status.hardBrake = 0;
  g_status.hardTurn = 0;
  g_status.hardTotal = 0;
  g_status.elapsedS = 0;
  unlock();
}

void tick(const Sample &s) {
  const int64_t nowUs = esp_timer_get_time();
  if (g_lastUs == 0) {
    g_lastUs = nowUs;
    return;
  }
  float dt = (float)(nowUs - g_lastUs) / 1000000.0f;
  g_lastUs = nowUs;
  // Ett hopp i tiden, t.ex. efter en lang skrivning, far inte dra ivag poangen.
  if (dt <= 0 || dt > 0.5f) dt = 0.5f;

  updateSpeed();

  // ---- hitta tyngdkraften -------------------------------------------------
  if (!g_haveGravity) {
    g_gx = s.ax;
    g_gy = s.ay;
    g_gz = s.az;
    g_haveGravity = true;
  } else {
    const float a = dt / ECO_GRAVITY_TAU_S;
    const float k = (a > 1.0f) ? 1.0f : a;
    g_gx += (s.ax - g_gx) * k;
    g_gy += (s.ay - g_gy) * k;
    g_gz += (s.az - g_gz) * k;
  }

  const float gMag = sqrtf(g_gx * g_gx + g_gy * g_gy + g_gz * g_gz);

  // Under fritt fall, eller innan filtret hunnit satta sig, finns ingen
  // palitlig lodlinje och da ar det arligare att inte visa nagot alls.
  const bool levelled = gMag > 0.5f;
  if (!levelled) {
    lock();
    g_status.levelled = false;
    unlock();
    return;
  }

  // ---- rakna bort tyngdkraften -------------------------------------------
  const float dx = g_gx / gMag, dy = g_gy / gMag, dz = g_gz / gMag;
  const float along = s.ax * dx + s.ay * dy + s.az * dz;
  const float hx = s.ax - along * dx;
  const float hy = s.ay - along * dy;
  const float hz = s.az - along * dz;
  const float magG = sqrtf(hx * hx + hy * hy + hz * hz);

  // ---- tva axlar i vagplanet, for bubblan --------------------------------
  // Kortets x-axel projicerad ned i vagplanet blir den ena riktningen. Ligger
  // kortet sa att x pekar rakt ned duger inte den axeln, och da tas y i
  // stallet - annars skulle basen kollapsa.
  float bx, by, bz;
  if (fabsf(dx) < 0.9f) {
    bx = 1.0f - dx * dx;
    by = -dx * dy;
    bz = -dx * dz;
  } else {
    bx = -dy * dx;
    by = 1.0f - dy * dy;
    bz = -dy * dz;
  }
  const float bLen = sqrtf(bx * bx + by * by + bz * bz);
  if (bLen > 0.0001f) {
    bx /= bLen;
    by /= bLen;
    bz /= bLen;
  }
  // Den andra riktningen ar vinkelrat mot bade lodlinjen och den forsta.
  const float cx = dy * bz - dz * by;
  const float cy = dz * bx - dx * bz;
  const float cz = dx * by - dy * bx;

  const float lonG = hx * bx + hy * by + hz * bz;
  const float latG = hx * cx + hy * cy + hz * cz;

  // ---- poang --------------------------------------------------------------
  if (magG > ECO_SOFT_G) {
    g_score -= (magG - ECO_SOFT_G) * ECO_PENALTY_PER_G_S * dt;
  } else {
    g_score += ECO_RECOVERY_PER_S * dt;
  }
  if (g_score < 0) g_score = 0;
  if (g_score > 100.0f) g_score = 100.0f;

  if (magG > g_peak) g_peak = magG;

  // ---- handelser ----------------------------------------------------------
  uint32_t addAccel = 0, addBrake = 0, addTurn = 0, addTotal = 0;
  if (!g_inEvent && magG >= ECO_HARD_G) {
    g_inEvent = true;
    if (g_haveSpeed) {
      const float lg = gpsLongitudinalG();
      // Farten avgor vad handelsen var. Det kraver ingen kunskap om hur
      // kortet ar vant i bilen, vilket ar hela poangen med att fraga GPS:en
      // i stallet for accelerometern.
      if (lg > 0.08f) {
        addAccel = 1;
      } else if (lg < -0.08f) {
        addBrake = 1;
      } else {
        addTurn = 1;
      }
    } else {
      addTotal = 1;
    }
  } else if (g_inEvent && magG < ECO_CLEAR_G) {
    g_inEvent = false;
  }

  lock();
  g_status.score = g_score;
  g_status.lonG = lonG;
  g_status.latG = latG;
  g_status.magG = magG;
  g_status.peakG = g_peak;
  g_status.levelled = true;
  g_status.gpsClassify = g_haveSpeed;
  g_status.hardAccel += addAccel;
  g_status.hardBrake += addBrake;
  g_status.hardTurn += addTurn;
  g_status.hardTotal += addTotal;
  g_status.elapsedS = (millis() - g_startMs) / 1000;
  unlock();
}

EcoStatus status() {
  lock();
  EcoStatus s = g_status;
  unlock();
  return s;
}

}  // namespace eco
