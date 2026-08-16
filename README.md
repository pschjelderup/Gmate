# Gmate

G-kraftslogger för **Waveshare ESP32-S3-Touch-AMOLED-2.41**. Loggar allt
rörelsesensorn QMI8658 kan mäta – acceleration i tre riktningar, vridning i tre
riktningar och temperatur – till ett vanligt microSD-kort, i ett format som går
att öppna direkt i Excel.

Skärmen är hela gränssnittet: en stor **STARTA**-knapp, en **STOPPA**-knapp, och
löpande besked om hur mycket som loggats och hur länge utrymmet räcker.

---

## Vad du behöver

| Sak | Kommentar |
|---|---|
| Waveshare ESP32-S3-Touch-AMOLED-2.41 | Med eller utan fodral (`-B` betyder bara att fodral ingår – samma kort) |
| microSD-kort | 8–128 GB. Formaterat som **FAT32** eller **exFAT** |
| USB-C-kabel | Måste klara **data**. Många kablar är rena laddkablar och fungerar inte |
| Strömkälla | Bara för långa loggningar – se [Ström och värme](#ström-och-värme) |

---

## Installera

1. Öppna **[flashsidan](https://pschjelderup.github.io/Gmate/)** i Chrome eller
   Edge på en dator. (Safari och Firefox kan inte prata med kort över USB.)
2. Sätt i USB-kabeln.
3. Klicka på **Installera Gmate på kortet** och välj kortets port i listan.
4. Vänta tills det står att det är klart.

Hittar datorn ingen port: håll inne **BOOT**, tryck och släpp **RESET**, släpp
sedan BOOT. Då lägger sig kortet i ett läge där det alltid går att flasha.

> Flashsidan blir tillgänglig först när GitHub Pages är påslaget för det här
> förrådet: **Settings → Pages → Source: GitHub Actions**. Fram tills dess går
> filen att hämta manuellt under fliken **Actions**, i senaste körningen under
> *Artifacts*.

---

## Använda

### Huvudskärmen

Överst visas de levande mätvärdena – total g-kraft med stora siffror, sedan
värdena per axel, vridhastigheten och temperaturen.

Mitt på skärmen sitter den stora knappen:

- **STARTA** – börjar logga. Knappen blir röd och byter till **STOPPA**.
- **STOPPA** – avslutar och stänger filen ordentligt.
- **SÄTT I KORT** – grå knapp som betyder att inget minneskort hittades. Sätt i
  ett kort och tryck på knappen, så letar den igen.

Längst ned står hur det går:

- När den står stilla: hur mycket ledigt utrymme kortet har, vald frekvens och
  hur länge utrymmet räcker.
- När den loggar: hur länge den hållit på, antal rader, hur många megabyte som
  skrivits, vilken fil som skrivs, och hur länge utrymmet räcker till.

### Menyn

Knappen **MENY** öppnar inställningarna. Under en pågående loggning är den låst,
så att en och samma fil inte får olika inställningar i olika delar. Stoppa
först.

| Inställning | Val | Standard |
|---|---|---|
| Loggfrekvens | 1, 2, 5, 10, 20, 50, 100, 200 Hz | **5 Hz** |
| Mätområde G | ±2, ±4, ±8, ±16 g | ±8 g |
| Mätområde gyro | ±64 … ±1024 grader/s | ±512 |
| Släck skärm | aldrig, 15, 30, 60, 300 s | 30 s |

Inställningarna sparas i kortet och överlever strömavbrott.

**Mätområde** betyder hur kraftiga rörelser sensorn klarar innan värdet slår i
taket. Ett mindre område ger finare upplösning men klipper vid hårda smällar.
±8 g räcker till det mesta på väg; ±16 g om det handlar om stötar och slag.

### Skärmen under långa loggningar

Skärmen är den överlägset största strömförbrukaren och den enda nämnvärda
värmekällan. Därför slocknar den automatiskt efter den tid du valt – men
**bara medan den loggar**, aldrig när du står och pillar med den.

- Loggningen fortsätter opåverkad när skärmen är släckt.
- Tänd igen med **sidoknappen (BOOT)** eller genom att trycka på skärmen.
- Första trycket tänder bara skärmen, det startar eller stoppar aldrig
  loggningen av misstag.
- Vill du att den aldrig ska slockna: ställ **Släck skärm** på *aldrig*.

---

## Filerna på kortet

Filerna hamnar i mappen `GMATE` och heter `L0001_01.CSV`, `L0002_01.CSV` och så
vidare – ett nytt nummer för varje loggning. Blir en fil större än 64 MB börjar
den på `_02`, `_03` … Det är för att en enskild fil inte ska bli ohanterlig, och
för att ett strömavbrott bara ska kunna skada den fil som just skrevs.

Varje fil börjar med en rubrikrad och innehåller sedan en rad per mätning:

| Kolumn | Betydelse |
|---|---|
| `tid_s` | Sekunder sedan loggningen startade |
| `klocka` | Datum och tid från kortets klocka |
| `ax_g`, `ay_g`, `az_g` | Acceleration per axel, i g |
| `a_tot_g` | Total acceleration, i g |
| `gx_dps`, `gy_dps`, `gz_dps` | Vridhastighet per axel, i grader per sekund |
| `temp_c` | Sensorns temperatur, i grader Celsius |

I stillhet visar `a_tot_g` ungefär **1,00** – det är jordens dragningskraft, och
den är alltid med.

Data skrivs till kortet var femte sekund. Vid ett strömavbrott förlorar du
alltså som mest de senaste fem sekunderna, aldrig hela filen.

### Om klockan

Kortet har en egen klocka med backup, men den vet inte vad den är för tid när
den är ny. Första gången firmware startar ställs den till den tidpunkt då
firmware byggdes, vilket ligger nära nog för att loggarna ska gå att sortera och
hitta i efterhand. Vill du ha sekundexakt tid går det att koppla in en GPS – se
[Idéer att bygga vidare på](#idéer-att-bygga-vidare-på).

---

## Hur länge räcker kortet?

Skärmen räknar ut det åt dig utifrån hur långa raderna faktiskt blir och hur
mycket ledigt utrymme kortet har. Grovt sett, med en rad på ~92 tecken:

| Frekvens | Per dygn | 14 dygn | Räcker på 32 GB |
|---|---|---|---|
| 5 Hz | ~38 MB | ~0,5 GB | ca 2,3 år |
| 20 Hz | ~150 MB | ~2,1 GB | ca 7 månader |
| 100 Hz | ~760 MB | ~10 GB | ca 6 veckor |
| 200 Hz | ~1,5 GB | ~21 GB | ca 3 veckor |

Vid 5 Hz är utrymmet aldrig problemet – ett vanligt 32 GB-kort räcker i flera
år.

---

## Ström och värme

**Det lilla batteriet räcker inte till 14 dygn.** Kortet drar i storleksordningen
40–80 mA med skärmen släckt, och betydligt mer med den tänd. Över 14 dygn blir
det ungefär **15–25 Ah**, alltså långt mer än ett litet litiumbatteri rymmer.

För en loggning som ska gå två veckor i sträck behöver kortet:

- **fast USB-ström**, eller
- **en powerbank på minst 20 000 mAh** – och en som inte stänger av sig själv
  vid låg strömförbrukning, vilket många gör. Sådana som marknadsförs för
  övervakningskameror eller "trickle charge" brukar fungera.

Det som firmware gör för att hjälpa till:

- Skärmen slocknar helt under loggning – inte bara nedtonad, utan avstängd.
- WiFi och Bluetooth startas aldrig.
- Data buffras och skrivs i klump, så kortet väcks sällan.

Siffrorna ovan är uppskattningar utifrån vad kretsarna brukar dra. Mät gärna
med en USB-mätare på din egen uppsättning innan du litar på en tvåveckorskörning
– och gör ett dygns provkörning först.

---

## Om något strular

| Symptom | Vad det brukar vara |
|---|---|
| Skärmen är svart efter flashning | Fel USB-kabel, eller kortet sitter kvar i flashläge. Tryck **RESET** |
| Det står **SÄTT I KORT** | Kortet sitter inte i ordentligt, eller är formaterat som NTFS. Formatera om till FAT32 eller exFAT |
| **SENSORFEL** vid start | Rörelsesensorn svarar inte. Starta om kortet; kvarstår det är det ett maskinvarufel |
| Knapptryck hamnar fel på skärmen | Öppna `firmware/Gmate/config.h` och sätt `TOUCH_FLIP_X` eller `TOUCH_FLIP_Y` till `1` |
| Datorn hittar ingen port | Håll **BOOT**, tryck **RESET**, släpp BOOT. Prova en annan kabel |
| Loggningen stannade av sig själv | Kortet blev fullt. Den stoppar med flit i stället för att skriva sönder filen |

---

## Idéer att bygga vidare på

- **GPS för exakt tid och hastighet.** En u-blox NEO-M9N över I2C ger sekundexakt
  tid från satellit, plus position och hastighet – vilket gör g-krafterna
  betydligt mer meningsfulla, eftersom man ser vad som hände.
- **Nollställning.** En knapp som drar bort den rådande lutningen, så att
  loggen visar avvikelse i stället för absolut riktning.
- **Toppvärden.** Högsta uppmätta g sedan start, kvar på skärmen.

---

## För den som vill bygga själv

Firmware ligger i `firmware/Gmate` och är en vanlig Arduino-skiss. Den byggs
automatiskt av GitHub Actions vid varje ändring, och resultatet blir både en
nedladdningsbar fil och flashsidan.

Bygga lokalt:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino" "SensorLib"
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,FlashMode=qio,PartitionScheme=huge_app,CDCOnBoot=cdc \
  firmware/Gmate
```

Pinnarna i `config.h` är verifierade mot två oberoende källor: Waveshares egen
Arduino-kortdefinition och CircuitPythons kortdefinition för samma kort. Båda
anger identiska pinnar.
