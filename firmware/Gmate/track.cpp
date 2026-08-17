#include "track.h"

#include <SD_MMC.h>
#include <math.h>

#include "config.h"
#include "gnss.h"

namespace {

// Avslutningen pa gpx-filen. Den skrivs efter varje tomning och skrivs sedan
// over av nasta punkt, sa att filen pa kortet alltid ar komplett och gar att
// oppna aven om stommen tar slut mitt i ett spar.
const char *kGpxFooter = "</trkseg></trk></gpx>\n";

// Ovanstaende knep vilar pa att en sparpunkt alltid ar langre an avslutningen.
// Den kortaste punkt vi skriver ar drygt 60 tecken, avslutningen 22, sa nasta
// punkt tacker alltid over den helt och lamnar ingen svans efter sig.

File g_gpx;
File g_csv;

SemaphoreHandle_t g_mutex = nullptr;

volatile bool g_want = false;
bool g_active = false;

TrackStatus g_status = {};

uint32_t g_sessionIndex = 0;
uint32_t g_startMs = 0;
uint32_t g_lastPointMs = 0;
uint32_t g_lastFlushMs = 0;
uint32_t g_trkPos = 0;

bool g_haveLast = false;
double g_lastLat = 0, g_lastLon = 0;
double g_distanceM = 0;

void lock() {
  if (g_mutex) xSemaphoreTake(g_mutex, portMAX_DELAY);
}
void unlock() {
  if (g_mutex) xSemaphoreGive(g_mutex);
}

// Avstand mellan tva positioner pa jordytan, i meter.
double haversineM(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371008.8;  // jordens medelradie
  const double rad = M_PI / 180.0;
  const double dLat = (lat2 - lat1) * rad;
  const double dLon = (lon2 - lon1) * rad;
  const double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1 * rad) * cos(lat2 * rad) * sin(dLon / 2) *
                       sin(dLon / 2);
  return 2.0 * R * asin(fmin(1.0, sqrt(a)));
}

// "2026-08-16T18:30:05Z" - tiden kommer fran satellit och ar exakt.
bool utcString(char *out, size_t len) {
  if (!gnss::timeValid()) return false;
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  gnss::utc(year, month, day, hour, minute, second);
  snprintf(out, len, "%04u-%02u-%02uT%02u:%02u:%02uZ", (unsigned)year,
           (unsigned)month, (unsigned)day, (unsigned)hour, (unsigned)minute,
           (unsigned)second);
  return true;
}

uint32_t nextSessionIndex() {
  uint32_t index = 1;
  char path[64];
  while (index < 10000) {
    snprintf(path, sizeof(path), TRACK_DIR "/T%04lu.GPX", (unsigned long)index);
    if (!SD_MMC.exists(path)) return index;
    index++;
  }
  return index;
}

// Skriver avslutningen och backar filpekaren dit den stod, sa att nasta punkt
// hamnar pa ratt plats.
void sealGpx() {
  if (!g_gpx) return;
  g_trkPos = g_gpx.position();
  g_gpx.write((const uint8_t *)kGpxFooter, strlen(kGpxFooter));
  g_gpx.flush();
  g_gpx.seek(g_trkPos);
}

bool openFiles() {
  if (!SD_MMC.exists("/GMATE")) SD_MMC.mkdir("/GMATE");
  if (!SD_MMC.exists(TRACK_DIR)) SD_MMC.mkdir(TRACK_DIR);

  g_sessionIndex = nextSessionIndex();

  char gpxPath[64];
  char csvPath[64];
  snprintf(gpxPath, sizeof(gpxPath), TRACK_DIR "/T%04lu.GPX",
           (unsigned long)g_sessionIndex);
  snprintf(csvPath, sizeof(csvPath), TRACK_DIR "/T%04lu.CSV",
           (unsigned long)g_sessionIndex);

  g_gpx = SD_MMC.open(gpxPath, FILE_WRITE);
  if (!g_gpx) return false;
  g_csv = SD_MMC.open(csvPath, FILE_WRITE);
  if (!g_csv) {
    g_gpx.close();
    return false;
  }

  char header[256];
  int n = snprintf(header, sizeof(header),
                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                   "<gpx version=\"1.1\" creator=\"Gmate\" "
                   "xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
                   "<trk><name>Gmate spar %lu</name><trkseg>\n",
                   (unsigned long)g_sessionIndex);
  g_gpx.write((const uint8_t *)header, n);

  const char *csvHeader =
      "klocka_utc,lat,lon,alt_m,hast_kmh,kurs_grader,satelliter,fix,"
      "delstracka_m,total_m\n";
  g_csv.write((const uint8_t *)csvHeader, strlen(csvHeader));

  sealGpx();

  lock();
  strncpy(g_status.fileName, gpxPath, sizeof(g_status.fileName) - 1);
  g_status.fileName[sizeof(g_status.fileName) - 1] = '\0';
  unlock();
  return true;
}

void writePoint(const GnssFix &f, double stepM) {
  char when[32] = "";
  const bool haveTime = utcString(when, sizeof(when));

  // ---- gpx: bara element som ingar i standarden, sa att filen oppnas utan
  // knot i sjokortsappar och Google Earth. Fart och kurs finns i csv-filen.
  char pt[256];
  int n = snprintf(pt, sizeof(pt), "<trkpt lat=\"%.7f\" lon=\"%.7f\">", f.lat,
                   f.lon);
  if (f.fixType >= 3) {
    n += snprintf(pt + n, sizeof(pt) - n, "<ele>%.1f</ele>", f.altM);
  }
  if (haveTime) {
    n += snprintf(pt + n, sizeof(pt) - n, "<time>%s</time>", when);
  }
  n += snprintf(pt + n, sizeof(pt) - n, "<sat>%u</sat></trkpt>\n",
                (unsigned)f.sats);
  g_gpx.write((const uint8_t *)pt, n);

  // ---- csv: hela sanningen, for den som vill rakna sjalv
  char row[192];
  const int len =
      snprintf(row, sizeof(row), "%s,%.7f,%.7f,%.1f,%.2f,%.1f,%u,%u,%.1f,%.1f\n",
               when, f.lat, f.lon, f.altM, f.speedKmh, f.courseDeg,
               (unsigned)f.sats, (unsigned)f.fixType, stepM, g_distanceM);
  g_csv.write((const uint8_t *)row, len);
}

void startTrack() {
  if (!openFiles()) {
    g_want = false;
    return;
  }

  g_startMs = millis();
  g_lastPointMs = 0;
  g_lastFlushMs = millis();
  g_haveLast = false;
  g_distanceM = 0;

  lock();
  g_status.logging = true;
  g_status.waitingForFix = true;
  g_status.points = 0;
  g_status.elapsedS = 0;
  g_status.distanceM = 0;
  g_status.speedKmh = 0;
  unlock();

  g_active = true;
}

void stopTrack() {
  if (g_gpx) {
    // Avslutningen skrivs en sista gang, och den har gangen backar vi inte.
    g_gpx.write((const uint8_t *)kGpxFooter, strlen(kGpxFooter));
    g_gpx.close();
  }
  if (g_csv) {
    g_csv.flush();
    g_csv.close();
  }
  g_active = false;

  lock();
  g_status.logging = false;
  g_status.waitingForFix = false;
  unlock();
}

}  // namespace

namespace track {

void begin() {
  if (g_mutex == nullptr) g_mutex = xSemaphoreCreateMutex();
}

bool start() {
  if (!gnss::present()) return false;
  g_want = true;
  for (int i = 0; i < 200 && !g_active; i++) {
    if (!g_want) return false;  // traden gav upp, t.ex. fullt kort
    delay(10);
  }
  return g_active;
}

void stop() {
  g_want = false;
  for (int i = 0; i < 200 && g_active; i++) delay(10);
}

bool isLogging() { return g_active; }

void closeFiles() {
  g_want = false;
  if (g_active) stopTrack();
}

void tick() {
  if (g_want && !g_active) {
    startTrack();
  } else if (!g_want && g_active) {
    stopTrack();
  }
  if (!g_active) return;

  const uint32_t now = millis();
  const GnssFix f = gnss::fix();

  lock();
  g_status.elapsedS = (now - g_startMs) / 1000;
  g_status.waitingForFix = !f.valid;
  unlock();

  if (!f.valid) return;

  // Tatare an sa har behover inga punkter sparas, hur fort det an gar.
  if (g_lastPointMs != 0 &&
      now - g_lastPointMs < (uint32_t)TRACK_MIN_INTERVAL_S * 1000) {
    return;
  }

  double stepM = 0;
  if (g_haveLast) {
    stepM = haversineM(g_lastLat, g_lastLon, f.lat, f.lon);

    const bool movedEnough = stepM >= TRACK_MIN_MOVE_M;
    const bool waitedLongEnough =
        now - g_lastPointMs >= (uint32_t)TRACK_MAX_INTERVAL_S * 1000;

    // Star farkosten stilla sparas anda en punkt da och da, sa att uppehallet
    // syns i sparet i stallet for att forsvinna.
    if (!movedEnough && !waitedLongEnough) return;

    if (movedEnough) g_distanceM += stepM;
  }

  writePoint(f, stepM);

  g_lastLat = f.lat;
  g_lastLon = f.lon;
  g_haveLast = true;
  g_lastPointMs = now;

  lock();
  g_status.points++;
  g_status.distanceM = g_distanceM;
  g_status.speedKmh = f.speedKmh;
  unlock();

  // Tomningen kostar tid, sa den sker sallan. Mellan tomningarna riskerar man
  // som mest en halv minut av sparet vid ett stromavbrott.
  if (now - g_lastFlushMs >= 30000) {
    g_lastFlushMs = now;
    g_csv.flush();
    sealGpx();
  }
}

TrackStatus status() {
  lock();
  TrackStatus s = g_status;
  unlock();
  return s;
}

}  // namespace track
