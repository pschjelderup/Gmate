#include "logger.h"

#include <SD_MMC.h>
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <Wire.h>
#include <esp_timer.h>
#include <time.h>

#include "config.h"
#include "gnss.h"

namespace {

SensorQMI8658 imu;
SensorPCF85063 rtc;

bool g_imuOk = false;
bool g_rtcOk = false;
bool g_sdOk = false;

// Skyddar all delad data mellan avlasningstraden och skarmen.
SemaphoreHandle_t g_mutex = nullptr;

Sample g_latest = {};
LoggerStatus g_status = {};

uint16_t g_rate = kRates[DEFAULT_RATE_INDEX];
uint8_t g_accelRange = kAccelRanges[DEFAULT_ACCEL_RANGE_INDEX];
uint16_t g_gyroRange = kGyroRanges[DEFAULT_GYRO_RANGE_INDEX];

volatile bool g_wantLogging = false;
bool g_isLogging = false;

// Skrivbuffert och den fil som just nu skrivs.
File g_file;
char g_buf[LOG_WRITE_BUFFER];
size_t g_bufLen = 0;
uint32_t g_sessionIndex = 0;
uint16_t g_partIndex = 0;
uint64_t g_bytesInFile = 0;
int64_t g_startUs = 0;
int64_t g_lastFlushMs = 0;
uint32_t g_lastClockSyncMs = 0;
bool g_clockSynced = false;

// Innan nagon rad skrivits har vi ingen uppmatt radlangd. Det har ar en
// rimlig gissning for formatet nedan och ersatts av det verkliga vardet sa
// snart forsta raderna skrivits.
const uint32_t kAssumedBytesPerRow = 92;

void lock() {
  if (g_mutex) xSemaphoreTake(g_mutex, portMAX_DELAY);
}

void unlock() {
  if (g_mutex) xSemaphoreGive(g_mutex);
}

SensorQMI8658::AccelRange accelRangeEnum(uint8_t g) {
  switch (g) {
    case 2: return SensorQMI8658::ACC_RANGE_2G;
    case 4: return SensorQMI8658::ACC_RANGE_4G;
    case 16: return SensorQMI8658::ACC_RANGE_16G;
    default: return SensorQMI8658::ACC_RANGE_8G;
  }
}

SensorQMI8658::GyroRange gyroRangeEnum(uint16_t dps) {
  switch (dps) {
    case 64: return SensorQMI8658::GYR_RANGE_64DPS;
    case 128: return SensorQMI8658::GYR_RANGE_128DPS;
    case 256: return SensorQMI8658::GYR_RANGE_256DPS;
    case 1024: return SensorQMI8658::GYR_RANGE_1024DPS;
    default: return SensorQMI8658::GYR_RANGE_512DPS;
  }
}

// Sensorn far ga snabbare an vi loggar. Det ger ett slags medelvarde i
// sensorns eget filter i stallet for att vi rakar prickar en enstaka
// mattidpunkt, och gor mjukare varden vid laga loggfrekvenser.
void applySensorConfig() {
  const bool fast = g_rate > 100;
  imu.configAccelerometer(accelRangeEnum(g_accelRange),
                          fast ? SensorQMI8658::ACC_ODR_1000Hz
                               : SensorQMI8658::ACC_ODR_250Hz,
                          SensorQMI8658::LPF_MODE_0);
  imu.configGyroscope(gyroRangeEnum(g_gyroRange),
                      fast ? SensorQMI8658::GYR_ODR_896_8Hz
                           : SensorQMI8658::GYR_ODR_224_2Hz,
                      SensorQMI8658::LPF_MODE_0);
  imu.enableAccelerometer();
  imu.enableGyroscope();
}

void refreshFreeSpace() {
  if (!g_sdOk) {
    g_status.freeBytes = 0;
    g_status.cardBytes = 0;
    return;
  }
  const uint64_t total = SD_MMC.totalBytes();
  const uint64_t used = SD_MMC.usedBytes();
  g_status.cardBytes = total;
  g_status.freeBytes = (total > used) ? (total - used) : 0;
}

// Letar upp nasta lediga loggnummer sa att en ny loggning aldrig skriver over
// en gammal.
uint32_t nextSessionIndex() {
  uint32_t index = 1;
  char path[48];
  while (index < 10000) {
    snprintf(path, sizeof(path), LOG_DIR "/L%04lu_01.CSV", (unsigned long)index);
    if (!SD_MMC.exists(path)) return index;
    index++;
  }
  return index;
}

void buildFileName(char *out, size_t len) {
  snprintf(out, len, LOG_DIR "/L%04lu_%02u.CSV", (unsigned long)g_sessionIndex,
           (unsigned)g_partIndex);
}

bool flushBuffer() {
  if (g_bufLen == 0) return true;
  if (!g_file) return false;
  const size_t written = g_file.write((const uint8_t *)g_buf, g_bufLen);
  const bool ok = (written == g_bufLen);
  if (!ok) g_status.writeErrors++;
  g_bufLen = 0;
  return ok;
}

bool openNextPart() {
  if (g_file) {
    flushBuffer();
    g_file.close();
  }
  g_partIndex++;
  char path[48];
  buildFileName(path, sizeof(path));

  g_file = SD_MMC.open(path, FILE_WRITE);
  if (!g_file) return false;

  g_bytesInFile = 0;

  // Rubrikrad, sa att filen gar att oppna direkt i Excel eller liknande.
  // Sitter en GPS inkopplad far filen fyra extra kolumner.
  const char *header =
      gnss::present()
          ? "tid_s,klocka,ax_g,ay_g,az_g,a_tot_g,gx_dps,gy_dps,gz_dps,temp_c,"
            "lat,lon,alt_m,hast_kmh,satelliter,fix\n"
          : "tid_s,klocka,ax_g,ay_g,az_g,a_tot_g,gx_dps,gy_dps,gz_dps,temp_c\n";
  g_file.write((const uint8_t *)header, strlen(header));
  g_bytesInFile += strlen(header);

  lock();
  strncpy(g_status.fileName, path, sizeof(g_status.fileName) - 1);
  g_status.fileName[sizeof(g_status.fileName) - 1] = '\0';
  unlock();
  return true;
}

bool startLogging() {
  if (!g_sdOk && !logger::remount()) return false;

  if (!SD_MMC.exists(LOG_DIR)) SD_MMC.mkdir(LOG_DIR);

  g_sessionIndex = nextSessionIndex();
  g_partIndex = 0;
  g_bufLen = 0;
  if (!openNextPart()) return false;

  g_startUs = esp_timer_get_time();
  g_lastFlushMs = millis();

  lock();
  g_status.logging = true;
  g_status.rows = 0;
  g_status.bytes = 0;
  g_status.elapsedS = 0;
  g_status.writeErrors = 0;
  g_status.bytesPerRow = kAssumedBytesPerRow;
  unlock();

  g_isLogging = true;
  return true;
}

void stopLogging() {
  if (g_file) {
    flushBuffer();
    g_file.close();
  }
  g_isLogging = false;
  lock();
  g_status.logging = false;
  unlock();
  refreshFreeSpace();
}

void writeRow(const Sample &s) {
  // Tid sedan loggningen startade, med millisekundupplosning.
  const int64_t us = esp_timer_get_time() - g_startUs;
  const uint32_t sec = (uint32_t)(us / 1000000);
  const uint32_t ms = (uint32_t)((us % 1000000) / 1000);

  char clock[24] = "";
  if (g_rtcOk) {
    RTC_DateTime dt = rtc.getDateTime();
    snprintf(clock, sizeof(clock), "%04u-%02u-%02u %02u:%02u:%02u",
             (unsigned)dt.getYear(), (unsigned)dt.getMonth(), (unsigned)dt.getDay(),
             (unsigned)dt.getHour(), (unsigned)dt.getMinute(), (unsigned)dt.getSecond());
  }

  char gpsPart[80] = "";
  if (gnss::present()) {
    const GnssFix f = gnss::fix();
    if (f.valid) {
      snprintf(gpsPart, sizeof(gpsPart), ",%.7f,%.7f,%.1f,%.2f,%u,%u", f.lat,
               f.lon, f.altM, f.speedKmh, (unsigned)f.sats,
               (unsigned)f.fixType);
    } else {
      // Utan position lamnas kolumnerna tomma. Att skriva nollor hade sett ut
      // som en giltig position i Atlanten utanfor Ghana.
      snprintf(gpsPart, sizeof(gpsPart), ",,,,,%u,%u", (unsigned)f.sats,
               (unsigned)f.fixType);
    }
  }

  char row[256];
  const int len = snprintf(
      row, sizeof(row),
      "%lu.%03lu,%s,%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.1f%s\n",
      (unsigned long)sec, (unsigned long)ms, clock, s.ax, s.ay, s.az, s.atot,
      s.gx, s.gy, s.gz, s.temp, gpsPart);
  if (len <= 0) return;

  if (g_bufLen + (size_t)len > sizeof(g_buf)) flushBuffer();
  memcpy(g_buf + g_bufLen, row, (size_t)len);
  g_bufLen += (size_t)len;

  g_bytesInFile += (size_t)len;

  lock();
  g_status.rows++;
  g_status.bytes += (uint64_t)len;
  g_status.elapsedS = sec;
  // Uppmatt genomsnittlig radlangd ger en arlig tidsuppskattning.
  if (g_status.rows > 0) {
    g_status.bytesPerRow = (uint32_t)(g_status.bytes / g_status.rows);
    if (g_status.bytesPerRow == 0) g_status.bytesPerRow = kAssumedBytesPerRow;
  }
  unlock();
}

// Satellittid ar exakt till sekunden. Nar GPS:en vet vad klockan ar stalls
// kortets egen klocka efter den - forst sa fort ett fix finns, sedan en gang
// i timmen for att motverka drift under langa loggningar.
void syncClockFromGnss() {
  if (!g_rtcOk || !gnss::present() || !gnss::timeValid()) return;

  const uint32_t now = millis();
  if (g_clockSynced && (now - g_lastClockSyncMs) < 3600000UL) return;

  uint16_t year;
  uint8_t month, day, hour, minute, second;
  gnss::utc(year, month, day, hour, minute, second);
  if (year < 2024) return;

  // GPS levererar UTC. Vill man ha lokal tid i loggen andras offseten i
  // config.h; omrakningen gar via unix-tid sa att dygns- och manadsbyten
  // hanteras ratt.
  struct tm t = {};
  t.tm_year = (int)year - 1900;
  t.tm_mon = (int)month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  time_t stamp = mktime(&t);
  if (stamp == (time_t)-1) return;
  stamp += (time_t)GNSS_UTC_OFFSET_MINUTES * 60;

  struct tm out;
  if (gmtime_r(&stamp, &out) == nullptr) return;

  rtc.setDateTime(RTC_DateTime((uint16_t)(out.tm_year + 1900),
                               (uint8_t)(out.tm_mon + 1), (uint8_t)out.tm_mday,
                               (uint8_t)out.tm_hour, (uint8_t)out.tm_min,
                               (uint8_t)out.tm_sec));
  g_lastClockSyncMs = now;
  g_clockSynced = true;
}

void samplerTask(void *) {
  TickType_t last = xTaskGetTickCount();
  uint32_t sinceSpaceCheck = 0;

  for (;;) {
    // Nar ingen loggning pagar racker det med nagra avlasningar per sekund
    // for att skarmen ska kannas levande.
    const uint16_t hz = g_isLogging ? g_rate : 10;
    TickType_t period = pdMS_TO_TICKS(1000 / hz);
    if (period == 0) period = 1;

    // Start och stopp sker har, i traden, sa att filen aldrig ror vid tva
    // tradar samtidigt.
    if (g_wantLogging && !g_isLogging) {
      if (!startLogging()) g_wantLogging = false;
      last = xTaskGetTickCount();
    } else if (!g_wantLogging && g_isLogging) {
      stopLogging();
    }

    gnss::poll();
    syncClockFromGnss();

    Sample s = {};
    if (g_imuOk) {
      imu.getAccelerometer(s.ax, s.ay, s.az);
      imu.getGyroscope(s.gx, s.gy, s.gz);
      s.temp = imu.getTemperature_C();
      s.atot = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
    }

    lock();
    g_latest = s;
    unlock();

    if (g_isLogging) {
      writeRow(s);

      // Tom bufferten med jamna mellanrum sa att ett stromavbrott bara kan
      // kosta de senaste sekunderna.
      if ((int64_t)millis() - g_lastFlushMs >= LOG_FLUSH_INTERVAL_MS) {
        flushBuffer();
        g_file.flush();
        g_lastFlushMs = millis();
      }

      if (g_bytesInFile >= LOG_ROTATE_BYTES) openNextPart();

      // Ledigt utrymme ar dyrt att rakna ut, sa det gors sallan.
      if (++sinceSpaceCheck >= (uint32_t)hz * 30) {
        sinceSpaceCheck = 0;
        const uint64_t before = g_status.freeBytes;
        refreshFreeSpace();
        // Slut pa plats: stoppa hellre snyggt an att skriva sonder filen.
        if (g_status.freeBytes < 1024UL * 1024UL && before != 0) {
          g_wantLogging = false;
        }
      }
    }

    vTaskDelayUntil(&last, period);
  }
}

}  // namespace

namespace logger {

bool begin() {
  g_mutex = xSemaphoreCreateMutex();
  memset(&g_status, 0, sizeof(g_status));
  g_status.bytesPerRow = kAssumedBytesPerRow;

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  // Sensorn sitter pa 0x6B pa det har kortet, men vissa exemplar svarar pa
  // 0x6A. Prova bada.
  g_imuOk = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (!g_imuOk) {
    g_imuOk = imu.begin(Wire, QMI8658_H_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  }
  if (g_imuOk) applySensorConfig();

  // Valfri GPS. Finns ingen ar det inte ett fel - loggen far bara farre
  // kolumner.
  gnss::begin();

  g_rtcOk = rtc.begin(Wire, PIN_I2C_SDA, PIN_I2C_SCL);
  if (g_rtcOk) {
    // Ett kort som aldrig fatt tiden stalld svarar med ett orimligt artal.
    // Da satter vi klockan till tidpunkten firmware byggdes, vilket ligger
    // nara nog for att loggarna ska ga att sortera.
    RTC_DateTime now = rtc.getDateTime();
    if (now.getYear() < 2024 || now.getYear() > 2099) {
      rtc.setDateTime(RTC_DateTime(__DATE__, __TIME__));
    }
  }

  remount();

  // Riklig stack: raderna formateras med flyttal, vilket kraver mer utrymme
  // an man forst tror.
  xTaskCreatePinnedToCore(samplerTask, "sampler", 8192, nullptr, 5, nullptr, 0);
  return g_imuOk;
}

bool imuOk() { return g_imuOk; }
bool sdMounted() { return g_sdOk; }

bool remount() {
  if (g_sdOk) SD_MMC.end();
  g_sdOk = false;

  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  // true = enbitslage, vilket ar sa kortplatsen ar kopplad pa det har kortet.
  if (SD_MMC.begin("/sdcard", true, false)) {
    g_sdOk = (SD_MMC.cardType() != CARD_NONE);
  }

  lock();
  g_status.sdMounted = g_sdOk;
  unlock();
  refreshFreeSpace();
  return g_sdOk;
}

void setRate(uint16_t hz) {
  if (g_isLogging) return;
  g_rate = hz;
  if (g_imuOk) applySensorConfig();
}

uint16_t rate() { return g_rate; }

void setRanges(uint8_t accelG, uint16_t gyroDps) {
  if (g_isLogging) return;
  g_accelRange = accelG;
  g_gyroRange = gyroDps;
  if (g_imuOk) applySensorConfig();
}

bool start() {
  if (!g_sdOk && !remount()) return false;
  g_wantLogging = true;
  // Traden hinner starta loggningen inom en avlasningsperiod.
  for (int i = 0; i < 100 && !g_isLogging; i++) {
    if (!g_wantLogging) return false;  // traden gav upp
    delay(10);
  }
  return g_isLogging;
}

void stop() {
  g_wantLogging = false;
  for (int i = 0; i < 200 && g_isLogging; i++) delay(10);
}

bool isLogging() { return g_isLogging; }

Sample latest() {
  lock();
  Sample s = g_latest;
  unlock();
  return s;
}

LoggerStatus status() {
  lock();
  LoggerStatus s = g_status;
  unlock();
  return s;
}

uint64_t estimateSecondsLeft() {
  LoggerStatus s = status();
  if (!s.sdMounted || g_rate == 0) return 0;
  uint32_t perRow = s.bytesPerRow ? s.bytesPerRow : kAssumedBytesPerRow;
  const uint64_t perSecond = (uint64_t)perRow * g_rate;
  if (perSecond == 0) return 0;
  return s.freeBytes / perSecond;
}

String nowString() {
  if (!g_rtcOk) return String("");
  RTC_DateTime dt = rtc.getDateTime();
  char buf[24];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)dt.getYear(),
           (unsigned)dt.getMonth(), (unsigned)dt.getDay(), (unsigned)dt.getHour(),
           (unsigned)dt.getMinute(), (unsigned)dt.getSecond());
  return String(buf);
}

}  // namespace logger
