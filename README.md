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
| u-blox-GPS | **Valfritt.** Ger position, hastighet och exakt klocka – se [GPS](#gps-valfritt) |
| Qwiic-kabel | Bara om du kopplar in GPS. Passar rakt in, ingen adapter behövs |

---

## Installera

1. Öppna **[flashsidan](https://pschjelderup.github.io/Gmate/)** i Chrome eller
   Edge på en dator. (Safari och Firefox kan inte prata med kort över USB.)
2. Sätt i USB-kabeln.
3. Klicka på **Installera Gmate på kortet** och välj kortets port i listan.
4. Vänta tills det står att det är klart.

Det finns **bara en mjukvara** – du väljer ingenting. Sidan installerar alltid
det senaste som byggts, och under knappen står vilken version det är och när den
byggdes. Vilken version som sitter på kortet ser du i **MENY**, uppe till höger.
Stämmer de två överens gick flashningen igenom.

Hittar datorn ingen port: håll inne **BOOT**, tryck och släpp **RESET**, släpp
sedan BOOT. Då lägger sig kortet i ett läge där det alltid går att flasha.

### Behöver jag kryssa i *Erase device*?

**Nej.** Firmwaren skrivs i två delar med ett hål emellan, precis där kortet
sparar sina inställningar. Följande överlever varje omflashning:

- Loggfrekvens, mätområden och skärmtimeout
- Ecodrive-gränserna och poängfönstret
- Tarat monteringsläge
- Inlärd framåtriktning

Kryssa i den bara när du **vill** börja om från noll – då raderas hela kortet
och allt står på fabriksinställning efter omstart.

### Engångsinställning innan flashsidan fungerar

Flashsidan ligger på GitHub Pages, och det behöver slås på en gång:

1. **Settings → General**, längst ned: **Change repository visibility → Public**.
   (Pages är gratis för publika förråd men kostar för privata. Koden är en
   g-kraftslogger, det finns inget känsligt i den.)
2. **Settings → Pages → Source: GitHub Actions**.
3. Slå ihop grenen till `main`. Bygget publicerar då sidan automatiskt.

Därefter ligger flashsidan kvar och uppdateras av sig själv vid varje ändring.

> Vill du hellre behålla förrådet privat går det att hämta filen manuellt under
> fliken **Actions** → senaste körningen → *Artifacts*, och flasha den med
> [Espressifs webbflashare](https://espressif.github.io/esptool-js/). Filen ska
> då skrivas till adress `0x0`. Det kräver att kontots artefaktlagring inte är
> full.

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

Under den sitter **spårknappen** – se [Spårloggning](#spårloggning) – och under
den tre småknappar: **MENY**, **ECO** (se
[ECODRIVE-skärmen](#ecodrive-skärmen)) och **SLÄCK**.

Längst ned står hur det går:

- När den står stilla: hur mycket ledigt utrymme kortet har, vald frekvens och
  hur länge utrymmet räcker.
- När den loggar: hur länge den hållit på, antal rader, hur många megabyte som
  skrivits, vilken fil som skrivs, och hur länge utrymmet räcker till.
- När ett spår loggar tar spåret över tredje raden och visar sträcka och antal
  punkter, eftersom det är det man vill följa då.

Uppe i hörnen sitter två lägesprickar: **minneskortet** till höger och **GPS**
till vänster.

| GPS-prick | Betyder |
|---|---|
| Grå, "ingen GPS" | Ingen mottagare inkopplad |
| Gul, "GPS söker" | Mottagaren är inkopplad men har ännu ingen position |
| Grön, "GPS 9 sat" | Position låst, antal satelliter visas |

Skillnaden mellan grå och gul är den viktiga: grå betyder att kabeln eller
modulen inte fungerar, gul att allt är rätt inkopplat och att det bara är att
vänta.

### Spårloggning

Spåret är din färdväg över tid – samma sak som spårloggen i en plotter. Det är
en **egen loggning vid sidan av g-kraftsloggen**: de startas och stoppas var för
sig och kan mycket väl köra samtidigt. G-krafterna beskriver vad som hände,
spåret var det hände.

- **STARTA SPÅR** – börjar spara positionen. Knappen blir röd och byter till
  **STOPPA SPÅR**.
- **SPÅR: INGEN GPS** – grå och avstängd. Utan mottagare finns ingen position
  att spara, så knappen lovar inget den inte kan hålla.

Spåret sparas i två filer i mappen `GMATE/SPAR`:

| Fil | Vad du gör med den |
|---|---|
| `T0001.GPX` | Dra in den i Google Earth, OpenCPN, Garmin BaseCamp eller vilken sjökortsapp som helst – färdvägen ritas ut på kartan |
| `T0001.CSV` | Samma punkter som siffror: tid, position, höjd, fart, kurs, satelliter och sträcka. Öppnas i Excel |

**Hur ofta sparas en punkt?** Inte blint varje sekund – det ger tusentals
identiska punkter så fort du ligger stilla, och GPS-bruset ritar ett garnnystan
där båten faktiskt låg still. I stället sparas en punkt när du flyttat dig minst
**5 meter**, dock aldrig tätare än var **2:a sekund**. Ligger du stilla sparas
ändå en punkt varje **minut**, så att uppehållet syns i spåret i stället för att
försvinna. Siffrorna går att ändra överst i `firmware/Gmate/config.h`.

GPX-filen skrivs så att den **alltid går att öppna**, även om strömmen försvinner
mitt i ett spår. Filen avslutas korrekt var trettionde sekund och nästa punkt
skriver över avslutningen. Vid ett strömavbrott förlorar du alltså som mest en
halv minut av spåret, aldrig hela filen.

### ECODRIVE-skärmen

Knappen **ECO** öppnar en skärm som visar hur mjukt du kör, medan du kör. Den är
gjord för att sitta i bilen och gå att uppfatta i ögonvrån.

Tanken bakom den är enkel: **sparsam körning och mjuk körning är samma sak.**
Bränsle går åt när farten ändras – vid gaspådrag, inbromsning och kurvtagning –
och ju häftigare ändringen är, desto mer går åt. Därför går sparsamhet att mäta
med enbart rörelsesensorn, utan att veta någonting om motorn.

**Bubblan** är ett vattenpass baklänges: den ska stå still i mitten. Ju hårdare
du kör, desto längre ut vandrar den. Två av ringarna är dina gränser – den
gröna är den mjuka, den orange den hårda – och de flyttar sig när du ändrar dem
i [GRÄNSER](#gränser--ställ-in-medan-du-kör). Färgen följer med:

| Färg | Betyder |
|---|---|
| Grön | Under mjuka gränsen (0,15 g) – så mycket man knappt tänker på |
| Gul | Mellan gränserna – märks i sätet |
| Röd | Över hårda gränsen (0,30 g) – passagerarna tittar upp |

Ringarna i bubblan betyder tre olika saker:

| Ring | Betyder |
|---|---|
| Grön | Mjuka gränsen – här börjar det kosta poäng |
| Röd | Hårda gränsen – här räknas det som ett hårt moment |
| Lila | Högsta värdet sedan nollställning |

Den lila ligger kvar så att du ser hur hårt det blev även när bubblan hunnit
tillbaka till mitten.

**Poängen** uppe till höger är 0–100 och lever hela tiden. Den sjunker när du
går över mjuka gränsen och klättrar tillbaka när du kör mjukt. **Poängfönstret**
i [GRÄNSER](#gränser--ställ-in-medan-du-kör) bestämmer hur snabbt: en
sammanhängande mjuk sträcka lika lång som fönstret tar poängen från noll
tillbaka till hundra. Kort fönster ger en siffra som säger *hur du kör just nu*;
långt fönster en som säger *hur hela resan gått*.
Under siffran står ett omdöme: UTMÄRKT, BRA, OK, HACKIGT eller HÅRT.

**Räknarna** längst ned visar antal hårda moment sedan du nollställde, uppdelat
i gas, broms och kurva. Under dem står toppvärdet och **hur långt kortet kommit
med att lära sig vilket håll som är framåt** – se
[Var får den sitta?](#var-får-den-sitta) nedan.

**NOLLSTÄLL** nollar poäng, toppvärde och räknare – lämpligt vid varje ny resa.
**GRÄNSER** öppnar inställningarna, se nedan. **TILLBAKA** går till
huvudskärmen. Skärmen släcks aldrig av sig själv i
eco-läget; den är till för att tittas på.

Poängen räknas **hela tiden**, även när skärmen visar något annat eller är
släckt. Annars skulle den börja om varje gång du tittade på den.

#### Var får den sitta?

**Var som helst. Den behöver inte ligga plant.** Tyngdkraften plockas fram ur
mätvärdena och räknas bort, så det som blir kvar är den vågräta accelerationen –
det som faktiskt känns i sätet. En snett monterad enhet visar alltså inte
konstant utslag, och siffrorna (0,23 g, poängen, räknarna) stämmer oavsett hur
den är vänd. Vid start står det kort "hittar lodlinjen" medan den ställer in sig.

**Flyttar du hållaren rättar den till sig själv.** När kortet ligger stilla –
vilket avgörs av gyrot – lär den om lodlinjen inom ett par sekunder. Du behöver
alltså inte tara bara för att du flyttat den; taran är till för att slippa vänta
de sekunderna och för att starta om riktningsinlärningen.

**Sensorns egen skala spelar ingen roll.** Visar din enhet 0,89 g i stillhet i
stället för 1,00 är det en förskjutning i själva sensorn. Ecodrive räknar allt
som *andel* av den uppmätta tyngdkraften, så gränserna hamnar rätt ändå. Loggen
skriver däremot alltid råvärdena, oförändrade – det är rådata och ska förbli
det.

Det som *inte* går att räkna ut av sig själv är **vilket håll som är framåt**.
En accelerometer som ligger stilla kan omöjligt veta det – det finns ingen
körriktning att observera när bilen står. Det märks bara på ett ställe: bubblans
riktning. Innan kortet vet vilket håll som är framåt är bubblans **storlek**
rätt men **riktningen godtycklig** – en inbromsning kan lika gärna kasta bubblan
åt sidan som nedåt.

Därför lär kortet sig riktningen medan du kör. Det jämför GPS-farten med
accelerationen: ökar farten pekar accelerationen framåt, minskar den pekar den
bakåt. Efter några gaspådrag och inbromsningar står riktningen, och då dyker
etiketterna **GAS** och **BROMS** upp kring bubblan. Nedersta raden på
ECODRIVE-skärmen berättar var den ligger:

| Raden säger | Betyder |
|---|---|
| `Riktning: kräver GPS` | Ingen mottagare inkopplad – riktningen går inte att lära sig |
| `Riktning: väntar på GPS-fix` | Mottagaren svarar men har ingen position ännu. Att köra hjälper inte förrän den fått fix |
| `Riktning: kör, gasa och bromsa` | GPS finns, men inlärningen har inte börjat |
| `Riktning: lär sig 40%` | Pågår. Kör normalt, den blir bättre för varje inbromsning |
| `Riktning: inlärd` | Klar. Broms = bubblan nedåt, högerkurva = bubblan åt vänster |

Riktningen sparas i kortets flashminne när den satt sig, så nästa gång du
startar är den redan inlärd.

#### TARA – när du flyttat hållaren

I **MENY** sitter knappen **TARA – STÅ STILL**. Den sparar hur kortet sitter
just nu, och startar om inlärningen av framåtriktningen.

Du behöver den inte för att komma igång – lutningen hittas ändå av sig själv
inom en halv minut. Den är till för två saker:

1. **Lodlinjen är rätt direkt vid start** i stället för efter en halv minut, så
   att de första sekunderna av en resa också duger.
2. **Riktningen lärs om från början** när du flyttat hållaren. Utan tara sitter
   den gamla riktningen kvar och korrigeras bara långsamt.

Tryck på knappen **medan bilen står stilla**. Rör sig kortet svarar den
**STÅ STILL** och sparar ingenting – då är det inte tyngdkraften den skulle
spara utan en manöver, och en felsparad lodlinje sitter kvar tills du tarar om.
Under knappen står om ett läge finns sparat.

#### Gas, broms och kurva avgörs av GPS-farten

Räknarna delar upp de hårda momenten i gas, broms och kurva. Den uppdelningen
görs på farten, inte på accelerometern, just för att den ska fungera oavsett hur
kortet är vänt. Utan GPS räknas därför alla hårda moment ihop till en enda
siffra, och skärmen säger rakt ut att uppdelningen inte går att göra – hellre
det än att visa "hård inbromsning" när du faktiskt svängde.

#### GRÄNSER – ställ in medan du kör

Knappen **GRÄNSER** på ECODRIVE-skärmen öppnar en egen meny med fyra rattar.
Den går att öppna **även under pågående loggning**, till skillnad från
huvudmenyn – gränserna påverkar bara hur skärmen bedömer körningen, aldrig vad
som hamnar i loggfilen, så det finns ingen fil som kan bli inkonsekvent.

| Ratt | Vad den gör | Standard |
|---|---|---|
| Mjuk gräns | Här börjar det kosta poäng | 0,15 g |
| Hård gräns | Här räknas det som ett hårt moment | 0,30 g |
| Ytterring | Vad bubblans ytterkant motsvarar | 0,40 g |
| Stränghet | Poäng som dras per g och sekund över mjuka gränsen | 40 |
| Poängfönster | Hur långt tillbaka poängen speglar | 2 min |

Uppe till höger står det levande värdet medan du skruvar, så du ser vad du
faktiskt kör med i stället för att gissa. Valet sparas och överlever
strömavbrott.

Sätt **ytterringen** efter hur du kör: 0,4 g fyller bubblan vid ganska rask
körning, 0,8 g behövs om du vill se topparna på en kurvig väg utan att bubblan
slår i kanten hela tiden.

Startvärdena ligger kvar överst i `firmware/Gmate/config.h`, tillsammans med
listorna över vad menyn erbjuder.

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

Under listan sitter **TARA – STÅ STILL**, som sparar hur kortet är monterat –
se [TARA](#tara--när-du-flyttat-hållaren).

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

Sitter en GPS inkopplad tillkommer sex kolumner: `lat`, `lon`, `alt_m`,
`hast_kmh`, `satelliter` och `fix`. Se [GPS](#gps-valfritt).

I stillhet visar `a_tot_g` ungefär **1,00** – det är jordens dragningskraft, och
den är alltid med.

Data skrivs till kortet var femte sekund. Vid ett strömavbrott förlorar du
alltså som mest de senaste fem sekunderna, aldrig hela filen.

### Om klockan

Kortet har en egen klocka med backup, men den vet inte vad den är för tid när
den är ny. Första gången firmware startar ställs den till den tidpunkt då
firmware byggdes, vilket ligger nära nog för att loggarna ska gå att sortera och
hitta i efterhand. Vill du ha sekundexakt tid: koppla in en GPS, så ställs
klockan efter satellittid automatiskt – se [GPS](#gps-valfritt).

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
| ECODRIVE visar hårt fast enheten ligger still | Skulle inte kunna hända längre. Händer det ändå: tryck **TARA** i menyn medan den står stilla, och hör av dig – det är ett fel |
| Kortet beter sig som den gamla versionen efter flashning | Jämför versionsraden på flashsidan med den i **MENY**. Visar sidan fel version har webbläsaren sparat en gammal kopia av sidan – ladda om med `Ctrl`+`F5` (`Cmd`+`Shift`+`R` på Mac) |

---

## GPS (valfritt)

Kopplar du in en u-blox-GPS – till exempel SparkFun NEO-M9N – på I2C-kontakten
hittas den automatiskt vid start. Sitter ingen GPS inkopplad fungerar allt
precis som vanligt, loggen får bara färre kolumner. Du behöver alltså inte byta
firmware fram och tillbaka.

Med GPS inkopplad:

- Loggen får **position, höjd och hastighet** på varje rad. Det är det som gör
  g-krafterna tolkbara i efterhand – man ser vad som faktiskt hände.
- **Klockan blir sekundexakt.** Kortets egen klocka ställs efter satellittid så
  fort mottagaren fått fix, och sedan en gång i timmen för att motverka drift.
- Skärmen visar antal satelliter och aktuell hastighet.

Innan mottagaren har fått fix lämnas positionskolumnerna **tomma**. De fylls
inte med nollor, eftersom 0,0 är en giltig position i Atlanten och hade sett ut
som riktiga mätvärden.

### Kabeln passar rakt in

Kontakten märkt **I2C** på kortets undersida har stiftordningen **GND · 3V3 ·
SDA · SCL**, tryckt i klartext bredvid kontakten (Rev 2.0). Det är exakt
Qwiic-standardens ordning, och kontakten är samma fysiska typ – JST SH 1,0 mm,
4-polig.

**En vanlig Qwiic-kabel går alltså direkt mellan korten.** Ingen adapter, ingen
omkastning, ingen lödning.

| | Gmate-kortet | NEO-M9N |
|---|---|---|
| Kontakt | JST SH 1,0 mm 4-pol | Samma |
| Stiftordning | GND · 3V3 · SDA · SCL | Samma |
| Spänning | **3,3 V** | 3,3 V |
| I2C-adress | – | 0x42, som firmware letar efter |
| Ström | – | ~31 mA |

Porten matar 3,3 V, inte 5 V – vilket är tur, eftersom NEO-M9N-kortet saknar
regulator och inte tål 5 V.

> Kontakten sitter till vänster om USB-C-uttaget. Bredvid den sitter en likadan
> märkt **UART** (GND · 3V3 · TXD · RXD) och en märkt **RTC**. Det är bara den
> som står **I2C** som ska användas – en Qwiic-kabel passar fysiskt i alla tre,
> men UART-porten lägger sändardata där GPS:en väntar sig klocksignal.

Två saker att veta:

- **Tiden loggas i UTC**, vilket är entydigt året om. Vill du hellre ha svensk
  tid: ändra `GNSS_UTC_OFFSET_MINUTES` i `firmware/Gmate/config.h` till `60`
  (vintertid) eller `120` (sommartid). Sommartiden byts inte om automatiskt.
- **GPS:en drar ström** – ungefär 30 mA kontinuerligt, vilket är i samma
  storleksordning som resten av kortet med skärmen släckt. Räkna med att
  ungefär fördubbla strömbudgeten för en tvåveckorskörning.

GPS:en läses av en gång i sekunden oavsett loggfrekvens, eftersom mottagaren
inte uppdaterar oftare än så. Vid högre loggfrekvenser upprepas alltså samma
position på flera rader, medan g-krafterna är färska på varje rad.

---

## Idéer att bygga vidare på

- **Uppkoppling.** WiFi-uppladdning till en egen server, så att data går att
  följa medan mätningen pågår i stället för i efterhand.
- **Sammanfattning per resa.** En rad per körning med sträcka, topp-g och poäng,
  så att man ser utvecklingen över tid utan att öppna rådatan.

---

## För den som vill bygga själv

Firmware ligger i `firmware/Gmate` och är en vanlig Arduino-skiss. Den byggs
automatiskt av GitHub Actions vid varje ändring, och resultatet blir både en
nedladdningsbar fil och flashsidan.

Bygga lokalt:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino" "SensorLib" "SparkFun u-blox GNSS v3"
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,FlashMode=qio,PartitionScheme=huge_app,CDCOnBoot=cdc \
  firmware/Gmate
```

Pinnarna i `config.h` är verifierade mot två oberoende källor: Waveshares egen
Arduino-kortdefinition och CircuitPythons kortdefinition för samma kort. Båda
anger identiska pinnar.
