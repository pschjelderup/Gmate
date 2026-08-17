# Hårdvaran

## Kortet

**Waveshare ESP32-S3-Touch-AMOLED-2.41**, Rev 2.0. Varianten `-B` är samma kort
med fodral.

| Del | Krets |
|---|---|
| Skärm | RM690B0 AMOLED, 450×600 stående, QSPI, 16 px kolumnoffset |
| Pekskärm | FT6336U |
| Rörelsesensor | QMI8658, 6DOF + temperatur |
| Klocka | PCF85063 med backup |
| IO-expander | TCA9554 (används inte) |
| Minne | 16 MB flash, 8 MB OPI-PSRAM |
| Kortplats | SDMMC i 1-bitsläge |

## Pinnar

Alla pinnar ligger i `firmware/Gmate/config.h` och är **verifierade mot två
oberoende källor** som anger identiska värden:

1. Waveshares egen Arduino-kortdefinition (`Arduino_GFX_dev_device.h`, blocket
   `WAVESHARE_ESP32_S3_TOUCH_AMOLED_2_41`)
2. CircuitPythons kortdefinition `waveshare_esp32_s3_amoled_241`

Två källor var inte överdrift: kortet identifierades ursprungligen fel två
gånger innan ett foto av förpackningsetiketten avgjorde saken. Ändra inga
pinnar utan att kunna peka på var siffran kommer ifrån.

**GPIO16 matar både skärmen och batterikretsen** och måste dras hög först av
allt i `setup()`. Utan den tänds skärmen aldrig, och felet ser ut som en död
panel.

## i2c-bussen

SDA på 47, SCL på 48. Delad av allt:

| Adress | Enhet |
|---|---|
| `0x20` | IO-expander |
| `0x38` | Pekskärm |
| `0x42` | GPS, om inkopplad |
| `0x51` | Klocka |
| `0x6B` | Rörelsesensor |

GPS-adressen krockar alltså inte med något ombord.

Pekskärmens avbrottssignal går via io-expandern, inte till en riktig GPIO.
Därför pollas pekskärmen i stället för att väckas — `TOUCH_IRQ_NOT_CONNECTED`.

Skissen skriver ut hela avsökningen över serieporten vid start. Saknas en
adress är det kabel eller kontakt, inte mjukvara.

## Kontakterna på undersidan

Tre likadana JST SH 1,0 mm-kontakter sitter bredvid varandra. **En Qwiic-kabel
passar fysiskt i alla tre.**

| Märkning | Stiftordning |
|---|---|
| `I2C` | GND · 3V3 · SDA · SCL |
| `UART` | GND · 3V3 · TXD · RXD |
| `RTC` | – |

I2C-kontaktens ordning är **exakt Qwiic-standardens**, avläst från kortets
silkscreen på Rev 2.0. En Qwiic-kabel går alltså rakt in utan adapter. Porten
matar **3,3 V**, inte 5 V.

## GPS (valfritt)

**SparkFun GPS Breakout NEO-M9N**, u-blox M9, i2c-adress `0x42`, ~31 mA vid
3,3 V. Ansluts med vanlig Qwiic-kabel till `I2C`-kontakten.

### U.FL-varianten behöver en antenn

Varianterna med chipantenn och SMA har en antenn; **u.FL-varianten har ingen
alls** — uttaget är bara ett anslutningsdon. Utan antenn får modulen aldrig fix,
hur länge den än står.

Kortet matar 3,3 V antennspänning på u.FL-uttaget, så en **aktiv** antenn
fungerar direkt. För bil: u.FL→SMA-pigtail plus magnetmonterad aktiv
GNSS-antenn, så att antennen kan sitta på taket i stället för inne bland
elektroniken.

Två fällor vid inköp:

- **Kön.** Magnetantenner har SMA hane, så pigtailen behöver SMA hona.
- **Generation.** Kortet har vanlig u.FL (IPEX1/MHF1), den största. MHF4 är
  mindre och passar inte.

Hur man ser skillnad på "ingen antenn" och "ingen sikt" över serieporten står i
[`gps-felsok`-skillen](../.claude/skills/gps-felsok/SKILL.md).

## Ström

| Post | Uppskattning |
|---|---|
| Kortet, skärm släckt, 5 Hz | 40–80 mA |
| GPS-modulen | ~31 mA |
| Skärmen tänd | Klart mest, och den enda nämnvärda värmekällan |

Det inbyggda batteriet räcker inte till fjorton dygn — det kräver fast USB-ström
eller en powerbank som inte stänger av sig vid låg last. Siffrorna är
uppskattningar; mät med USB-mätare innan du litar på dem.

## Sensorns skala

Enheten i bruk visar **0,89 g i stillhet** i stället för 1,00. Det är en verklig
förskjutning i sensorn, inte ett mätområdesfel — ett sådant hade gett faktor 2
eller 0,5.

Loggen skriver råvärdena oförändrade. Ecodrive räknar däremot allt som **andel
av den uppmätta tyngdkraften**, så gränserna hamnar rätt ändå. Se
[ecodrive.md](ecodrive.md).
