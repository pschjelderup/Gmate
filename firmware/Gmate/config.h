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

// ------------------------------------------------------------ ecodrive ----
// Granserna ar satta efter hur det kanns i bilen, inte efter vad som ar
// tekniskt mojligt. 0,15 g ar ungefar sa mycket man kanner utan att tanka pa
// det; 0,30 g ar en inbromsning som far passagerarna att titta upp.
#define ECO_SOFT_G 0.15f
#define ECO_HARD_G 0.30f

// Handelsen raknas som avslutad forst har, sa att ett enda haftigt ryck inte
// raknas som fem handelser nar vardet studsar kring gransen.
#define ECO_CLEAR_G 0.20f

// Poang som dras per g over den mjuka gransen och sekund.
#define ECO_PENALTY_PER_G_S 40.0f

// Tiden poangen speglar. En sammanhangande mjuk stracka sa har lang tar
// poangen fran noll tillbaka till hundra. Det ar det som avgor om siffran
// betyder "sa har har du kort den senaste stunden" eller "sa har har du kort
// sedan du tryckte nollstall" - utan ett fonster minns den allt lika mycket,
// och da slutar den saga nagot om hur det gar just nu.
static const uint16_t kEcoWindowS[] = {60, 120, 300, 600, 1800, 3600};
static const uint8_t kEcoWindowCount = 6;
#define DEFAULT_ECO_WINDOW_INDEX 1  // 2 min

// Vad ytterringen i bubblan motsvarar.
#define ECO_BUBBLE_FULL_G 0.40f

// Vardena ovan ar startvarden. De gar att andra i gransmenyn medan bilen
// rullar, och valet sparas. Listorna nedan ar vad man kan valja mellan.
static const float kEcoSoft[] = {0.08f, 0.10f, 0.12f, 0.15f,
                                 0.18f, 0.20f, 0.25f, 0.30f};
static const uint8_t kEcoSoftCount = 8;
#define DEFAULT_ECO_SOFT_INDEX 3  // 0,15 g

static const float kEcoHard[] = {0.20f, 0.25f, 0.30f, 0.35f,
                                 0.40f, 0.50f, 0.60f};
static const uint8_t kEcoHardCount = 7;
#define DEFAULT_ECO_HARD_INDEX 2  // 0,30 g

static const float kEcoBubble[] = {0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.80f};
static const uint8_t kEcoBubbleCount = 6;
#define DEFAULT_ECO_BUBBLE_INDEX 2  // 0,40 g

// Hur hart poangen straffar. Lag siffra ger en snall matare, hog en strang.
static const float kEcoPenalty[] = {10.0f, 20.0f, 40.0f, 60.0f, 90.0f};
static const uint8_t kEcoPenaltyCount = 5;
#define DEFAULT_ECO_PENALTY_INDEX 2  // 40

// Tidskonstanter for att hitta tyngdkraften. Den snabba anvands nar kortet
// ligger stilla - da ar den uppmatta vektorn tyngdkraften och det finns ingen
// anledning att vara forsiktig. Den langsamma anvands under fard, sa att en
// utdragen kurva inte hinner tolkas som "ned".
#define ECO_GRAVITY_TAU_FAST_S 2.0f
#define ECO_GRAVITY_TAU_SLOW_S 30.0f

// Sa har stilla maste det vara for att raknas som vila. Gyrot ar det som
// avgor: en bil i en jamn kurva har stor sidoacceleration men ocksa en tydlig
// girhastighet, medan ett kort som ligger pa ett bord har ingen alls.
#define ECO_REST_GYRO_DPS 2.5f

// ... och sa lite far accelerationsvektorn andra sig mellan tva avlasningar.
#define ECO_REST_JITTER_G 0.04f

// Under manover uppdateras lodlinjen inte alls. Det ar det som gor att en lang
// avfart eller rondell far behalla sitt varde i stallet for att sjunka undan.
#define ECO_FREEZE_MAG_G 0.12f
#define ECO_FREEZE_LONG_G 0.06f

// Men aldrig langre an sa har. Frysningen avgors av ett varde som raknas fram
// ur lodlinjen, sa en felaktig lodlinje kan halla sig sjalv fryst i all
// evighet om den slapps los. En riktig kurva varar inte tolv sekunder; en
// felaktig lodlinje varar tills stromen bryts.
#define ECO_FREEZE_MAX_S 12.0f

// Sa mycket maste hanna for att en handelse ska duga till att lara ut vilket
// hall som ar framat. Under detta ar bruset for stort for att lita pa.
#define ECO_FWD_MIN_LONG_G 0.08f
#define ECO_FWD_MIN_MAG_G 0.08f

// Inlarningstakt: snabb precis efter en tara, langsam darefter sa att
// riktningen inte vandrar av enstaka konstiga handelser.
#define ECO_FWD_GAIN_FAST 0.20f
#define ECO_FWD_GAIN_SLOW 0.04f

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
