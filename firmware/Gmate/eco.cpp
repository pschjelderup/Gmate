#include "eco.h"

#include <Preferences.h>
#include <esp_timer.h>
#include <math.h>

#include "config.h"
#include "gnss.h"

namespace {

SemaphoreHandle_t g_mutex = nullptr;
Preferences g_prefs;

EcoStatus g_status = {};

// ---- lodlinjen ------------------------------------------------------------
// Tyngdkraftens riktning, framplockad ur accelerationen. Filtret uppdateras
// inte under manover: annars skulle en lang kurva sakta tolkas som "ned" och
// bubblan sjunka tillbaka mot mitten fast bilen fortfarande ligger i kurvan.
float g_gx = 0, g_gy = 0, g_gz = 0;
bool g_haveGravity = false;
bool g_settled = false;

// ---- framatriktningen -----------------------------------------------------
// Enhetsvektor i vagplanet. Lars in genom att jamfora accelerometerns riktning
// med om GPS-farten okar eller minskar.
float g_fx = 0, g_fy = 0, g_fz = 0;
bool g_haveFwd = false;
float g_fwdQuality = 0;
bool g_fastLearn = false;
bool g_fwdSaved = false;

int64_t g_lastUs = 0;
uint32_t g_startMs = 0;

float g_score = 100.0f;
float g_peak = 0;
float g_prevMagG = 0;

bool g_inEvent = false;

// Taran utfors av avlasningstraden, inte av den som trycker pa knappen. Annars
// skulle lodlinjen skrivas fran tva hall samtidigt, mitt i en filtrering.
// Knapptrycket lamnar bara en begaran har och vantar pa svaret.
bool g_tarePending = false;
bool g_tareDone = false;
bool g_tareOk = false;

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

void normalize3(float &x, float &y, float &z) {
  const float n = sqrtf(x * x + y * y + z * z);
  if (n > 0.0001f) {
    x /= n;
    y /= n;
    z /= n;
  }
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
  if (!g_haveSpeed) {
    g_speedPrev = g_speedNow = f.speedKmh;
    g_speedPrevMs = g_speedNowMs = now;
    g_haveSpeed = true;
    return;
  }
  // Mottagaren lamnar ett nytt varde ungefar en gang i sekunden.
  if (f.speedKmh != g_speedNow || now - g_speedNowMs > 1500) {
    g_speedPrev = g_speedNow;
    g_speedPrevMs = g_speedNowMs;
    g_speedNow = f.speedKmh;
    g_speedNowMs = now;
  }
}

void saveCalibration(bool withForward) {
  g_prefs.begin("ecocal", false);
  g_prefs.putFloat("gx", g_gx);
  g_prefs.putFloat("gy", g_gy);
  g_prefs.putFloat("gz", g_gz);
  g_prefs.putBool("haveG", true);
  if (withForward) {
    g_prefs.putFloat("fx", g_fx);
    g_prefs.putFloat("fy", g_fy);
    g_prefs.putFloat("fz", g_fz);
    g_prefs.putBool("haveF", g_haveFwd);
  }
  g_prefs.end();
}

void loadCalibration() {
  g_prefs.begin("ecocal", true);
  const bool haveG = g_prefs.getBool("haveG", false);
  if (haveG) {
    g_gx = g_prefs.getFloat("gx", 0);
    g_gy = g_prefs.getFloat("gy", 0);
    g_gz = g_prefs.getFloat("gz", 1);
    g_haveGravity = true;
    g_settled = true;
  }
  if (g_prefs.getBool("haveF", false)) {
    g_fx = g_prefs.getFloat("fx", 0);
    g_fy = g_prefs.getFloat("fy", 0);
    g_fz = g_prefs.getFloat("fz", 0);
    g_haveFwd = true;
    // Ett sparat varde far en bra men inte perfekt utgangspunkt, sa att
    // korningen anda korrigerar det om hallaren flyttats.
    g_fwdQuality = 0.8f;
    g_fwdSaved = true;
  }
  g_prefs.end();

  lock();
  g_status.levelStored = haveG;
  unlock();
}

}  // namespace

namespace eco {

void begin() {
  if (g_mutex == nullptr) g_mutex = xSemaphoreCreateMutex();
  g_startMs = millis();
  loadCalibration();
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

bool tare() {
  lock();
  g_tareDone = false;
  g_tareOk = false;
  g_tarePending = true;
  unlock();

  // Vid 1 Hz dröjer det som mest en dryg sekund innan avlasningstraden hinner
  // svara, sa vantetiden ar tilltagen darefter.
  const uint32_t start = millis();
  while (millis() - start < 2500) {
    lock();
    const bool done = g_tareDone;
    const bool ok = g_tareOk;
    unlock();
    if (done) return ok;
    delay(10);
  }

  // Uteblivet svar betyder att avlasningen inte gar - da ar "misslyckades"
  // ratt besked, inte ett tyst ja.
  lock();
  g_tarePending = false;
  unlock();
  return false;
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
  const float gpsLongG = gpsLongitudinalG();

  // ---- lodlinjen ----------------------------------------------------------
  if (!g_haveGravity) {
    g_gx = s.ax;
    g_gy = s.ay;
    g_gz = s.az;
    g_haveGravity = true;
  } else {
    // Under manover star filtret still. Vardet fran forra varvet duger som
    // matt pa om det pagar nagot - det andras inte sa fort.
    const bool manoeuvring = g_prevMagG > ECO_FREEZE_MAG_G ||
                             fabsf(gpsLongG) > ECO_FREEZE_LONG_G;
    if (!manoeuvring) {
      const float tau =
          g_settled ? ECO_GRAVITY_TAU_SLOW_S : ECO_GRAVITY_TAU_FAST_S;
      float k = dt / tau;
      if (k > 1.0f) k = 1.0f;
      g_gx += (s.ax - g_gx) * k;
      g_gy += (s.ay - g_gy) * k;
      g_gz += (s.az - g_gz) * k;

      if (!g_settled && millis() - g_startMs > 5000) g_settled = true;
    }
  }

  const float gMag = sqrtf(g_gx * g_gx + g_gy * g_gy + g_gz * g_gz);

  // Under fritt fall, eller innan filtret hunnit satta sig, finns ingen
  // palitlig lodlinje och da ar det arligare att inte visa nagot alls.
  if (gMag <= 0.5f) {
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
  g_prevMagG = magG;

  // ---- tara, om nagon bett om det ----------------------------------------
  lock();
  const bool wantTare = g_tarePending;
  unlock();
  if (wantTare) {
    // Star kortet stilla ar all kvarvarande vagrat acceleration brus. Ror det
    // sig ar det en manover, och den far inte sparas som lodlinje.
    const bool ok = magG < 0.05f;
    if (ok) {
      normalize3(g_gx, g_gy, g_gz);
      g_settled = true;

      // Framatriktningen gar inte att lara ut nu - stillastaende finns inget
      // framat att observera. Den nollstalls och lars om under korningen som
      // foljer, vilket ar precis vad man vill nar hallaren just flyttats.
      g_haveFwd = false;
      g_fwdQuality = 0;
      g_fastLearn = true;
      g_fwdSaved = false;

      saveCalibration(false);
    }

    lock();
    g_tarePending = false;
    g_tareDone = true;
    g_tareOk = ok;
    if (ok) g_status.levelStored = true;
    unlock();
  }

  // ---- lar in framatriktningen -------------------------------------------
  // Accelerometern ensam kan aldrig veta vilket hall som ar framat. Farten kan:
  // okar den ar den vagrata accelerationen riktad framat, minskar den ar den
  // riktad bakat. Efter nagra gaspadrag och inbromsningar star riktningen.
  if (g_haveSpeed && fabsf(gpsLongG) > ECO_FWD_MIN_LONG_G &&
      magG > ECO_FWD_MIN_MAG_G) {
    const float sgn = (gpsLongG > 0) ? 1.0f : -1.0f;
    const float ux = sgn * hx / magG;
    const float uy = sgn * hy / magG;
    const float uz = sgn * hz / magG;

    if (!g_haveFwd) {
      g_fx = ux;
      g_fy = uy;
      g_fz = uz;
      g_haveFwd = true;
    } else {
      const float k = g_fastLearn ? ECO_FWD_GAIN_FAST : ECO_FWD_GAIN_SLOW;
      g_fx += (ux - g_fx) * k;
      g_fy += (uy - g_fy) * k;
      g_fz += (uz - g_fz) * k;
    }

    // Riktningen maste ligga i vagplanet, annars skulle den sakta tippa med
    // varje backe den lart sig i.
    const float dot = g_fx * dx + g_fy * dy + g_fz * dz;
    g_fx -= dot * dx;
    g_fy -= dot * dy;
    g_fz -= dot * dz;
    normalize3(g_fx, g_fy, g_fz);

    g_fwdQuality += g_fastLearn ? 0.06f : 0.02f;
    if (g_fwdQuality > 1.0f) g_fwdQuality = 1.0f;
    if (g_fwdQuality >= 1.0f) g_fastLearn = false;

    // Spara en gang nar riktningen satt sig, sa att den finns kvar efter en
    // omstart. Fler skrivningar an sa behover flashminnet inte.
    if (!g_fwdSaved && g_fwdQuality >= 1.0f) {
      g_fwdSaved = true;
      saveCalibration(true);
    }
  }

  // ---- tva axlar i vagplanet, for bubblan --------------------------------
  float lonG, latG;
  const bool oriented = g_haveFwd && g_fwdQuality > 0.2f;
  if (oriented) {
    // Hoger = ned x framat. Da hamnar gas uppat och hogerkurva at hoger pa
    // skarmen, precis som i ett vanligt g-diagram.
    const float rx = dy * g_fz - dz * g_fy;
    const float ry = dz * g_fx - dx * g_fz;
    const float rz = dx * g_fy - dy * g_fx;
    lonG = hx * g_fx + hy * g_fy + hz * g_fz;
    latG = hx * rx + hy * ry + hz * rz;
  } else {
    // Innan riktningen ar inlard duger kortets egna axlar. Storleken stammer,
    // riktningen ar godtycklig - och det sags pa skarmen.
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
    normalize3(bx, by, bz);
    const float cx = dy * bz - dz * by;
    const float cy = dz * bx - dx * bz;
    const float cz = dx * by - dy * bx;
    lonG = hx * bx + hy * by + hz * bz;
    latG = hx * cx + hy * cy + hz * cz;
  }

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
      // Farten avgor vad handelsen var. Det kraver ingen kunskap om hur
      // kortet ar vant i bilen, vilket ar hela poangen med att fraga GPS:en
      // i stallet for accelerometern.
      if (gpsLongG > 0.08f) {
        addAccel = 1;
      } else if (gpsLongG < -0.08f) {
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
  g_status.forwardKnown = oriented;
  g_status.forwardQuality = g_fwdQuality;
  g_status.forwardNeedsGnss = !gnss::present();
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
