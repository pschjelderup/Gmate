// Gmate - g-kraftslogger for Waveshare ESP32-S3-Touch-AMOLED-2.41
//
// Pinnarna nedan ar verifierade mot tva oberoende kallor:
//  1. Waveshare egen Arduino-kortdefinition (Arduino_GFX_dev_device.h,
//     blocket WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_41)
//  2. CircuitPythons kortdefinition waveshare_esp32_s3_amoled_241
// Bada anger identiska pinnar.

#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------- skarm ----
// RM690B0 AMOLED via QSPI. Panelen ar 450x600 (staende) och har 16 pixlars
// kolumnoffset.
#define PIN_LCD_CS 9
#define PIN_LCD_SCK 10
#define PIN_LCD_D0 11
#define PIN_LCD_D1 12
#define PIN_LCD_D2 13
#define PIN_LCD_D3 14
#define PIN_LCD_RST 21

// GPIO16 matar bade skarmen och batterikretsen. Maste dras hog forst av allt,
// annars tander skarmen aldrig.
#define PIN_PANEL_POWER 16

#define SCREEN_W 450
#define SCREEN_H 600
#define LCD_COL_OFFSET 16

// ------------------------------------------------------------------ i2c ----
// Delad buss: QMI8658 (rorelsesensor), FT6336U (pekskarm), PCF85063 (klocka),
// TCA9554 (io-expander, anvands inte har).
#define PIN_I2C_SDA 47
#define PIN_I2C_SCL 48
#define PIN_TOUCH_RST 3

// Pekskarmens avbrottssignal gar via io-expandern, inte till en riktig GPIO,
// sa vi lasar av pekskarmen med pollning i stallet.
#define TOUCH_IRQ_NOT_CONNECTED -1

// Valfri u-blox GPS pa samma i2c-buss. Adressen krockar inte med nagot
// ombord: rorelsesensorn ligger pa 0x6B, pekskarmen 0x38, klockan 0x51 och
// io-expandern 0x20.
#define GNSS_I2C_ADDR 0x42

// GPS ger tiden i UTC. Noll betyder att loggen skrivs i UTC, vilket ar
// entydigt aret om. Vill du hellre ha svensk tid: 60 pa vintern, 120 pa
// sommaren. Sommartiden byts inte om automatiskt.
#define GNSS_UTC_OFFSET_MINUTES 0

// Om pekningarna hamnar fel: satt dessa till 1 for att spegla respektive axel.
#define TOUCH_FLIP_X 0
#define TOUCH_FLIP_Y 0
#define TOUCH_SWAP_XY 0

// ------------------------------------------------------------- minneskort --
// Kortplatsen sitter pa SDMMC i 1-bitslage.
#define PIN_SD_CLK 4
#define PIN_SD_CMD 5
#define PIN_SD_D0 6

// ---------------------------------------------------------------- knappar --
// BOOT-knappen. Anvands som skarm av/pa under loggning.
#define PIN_BOOT_BUTTON 0

// ------------------------------------------------------------ sparlogg ----
#define TRACK_DIR "/GMATE/SPAR"

// Tatast mojliga avstand mellan tva sparpunkter, i sekunder. Halls aven nar
// baten gar fort.
#define TRACK_MIN_INTERVAL_S 2

// En punkt sparas forst nar farkosten flyttat sig sa har manga meter. Utan den
// regeln fylls sparet med tusentals identiska punkter sa fort man ligger still,
// och gps-bruset ritar ett garnnystan dar baten faktiskt lag stilla.
#define TRACK_MIN_MOVE_M 5.0

// ... men en punkt sparas anda sa har ofta, aven vid stillaliggande. Det ar sa
// man i efterhand ser att man lag kvar i viken i tre timmar, i stallet for att
// sparet ser ut att hoppa direkt vidare.
#define TRACK_MAX_INTERVAL_S 60

// ------------------------------------------------------------- loggning ----
#define LOG_DIR "/GMATE"

// Loggen delas i flera filer sa att en enskild fil inte blir ohanterlig och
// sa att ett stromavbrott bara kan skada den fil som just skrivs.
#define LOG_ROTATE_BYTES (64UL * 1024UL * 1024UL)

// Skrivbuffert. Rader samlas har och skrivs till kortet i klump, vilket ar
// bade snabbare och skonsammare mot kortet an en skrivning per rad.
#define LOG_WRITE_BUFFER 4096

// Hur ofta bufferten toms till kortet aven om den inte ar full. Bestammer hur
// mycket data som kan ga forlorad vid ett stromavbrott.
#define LOG_FLUSH_INTERVAL_MS 5000

// Valbara loggfrekvenser i menyn.
static const uint16_t kRates[] = {1, 2, 5, 10, 20, 50, 100, 200};
static const uint8_t kRateCount = sizeof(kRates) / sizeof(kRates[0]);
#define DEFAULT_RATE_INDEX 2  // 5 Hz

// Valbara matomraden for accelerometern (+/- g).
static const uint8_t kAccelRanges[] = {2, 4, 8, 16};
static const uint8_t kAccelRangeCount = 4;
#define DEFAULT_ACCEL_RANGE_INDEX 2  // +/-8 g

// Valbara matomraden for gyrot (+/- grader per sekund).
static const uint16_t kGyroRanges[] = {64, 128, 256, 512, 1024};
static const uint8_t kGyroRangeCount = 5;
#define DEFAULT_GYRO_RANGE_INDEX 3  // +/-512 dps

// Skarmen slacks efter sa har manga sekunders orordhet under loggning.
// 0 = slacks aldrig.
static const uint16_t kScreenTimeouts[] = {0, 15, 30, 60, 300};
static const uint8_t kScreenTimeoutCount = 5;
#define DEFAULT_SCREEN_TIMEOUT_INDEX 2  // 30 s
