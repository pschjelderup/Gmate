# Gmate uppkopplad: WiFi-uppladdning, Cloudflare-backend och Claude-analys

> **Status: plan, inte byggd.** Skriven under en planeringsomgång och flyttad
> hit så att den överlever sessionen. Ingenting av den finns i firmware. Siffror
> och gratisnivåer är från augusti 2026 och bör kontrolleras om innan de används.

## Bakgrund

Gmate loggar idag till microSD och visar allt på skärmen. Kortet sitter fysiskt
oåtkomligt under mätperioden, så data går bara att hämta genom att plocka ur
minneskortet. Det gör löpande uppföljning omöjlig och gör att ett fel upptäcks
först i efterhand.

Målet är att kortet skickar mätdata till en egen server medan det loggar, och
att Claude analyserar datan varje timme så att avvikelser syns när de händer i
stället för veckor senare.

Förutsättningarna, från dina svar:

| Fråga | Svar | Följd |
|---|---|---|
| Uppkoppling | WiFi finns på plats | Inget 4G-modem behövs — radion sitter redan i kortet |
| Data | Allt rådata, löpande | ~40 MB/dygn, oproblematiskt över WiFi |
| Färskhet | Batchar var 10:e minut | Radion får sova emellan — nästan gratis strömmässigt |
| Årstid | Mars–oktober | Solceller blir rakt fram att dimensionera |
| Hosting | Cloudflare | R2 för filerna, Workers för mottagning och analys |

**Cloudflare, inte Vercel.** Vercels gratisnivå kör schemalagda jobb högst en
gång per dygn, vilket omöjliggör timvis analys, och saknar billig objektlagring.
Cloudflare klarar båda på gratisnivån: R2 ger 10 GB lagring och fri utgående
trafik, Workers ger 100 000 anrop/dygn och schemalagda körningar.

---

## Arkitektur

```
Kortet                    Cloudflare                     Du
──────                    ──────────                     ──
SD-kort (sanning)
  │
  ├─ rådata.csv  ──POST──▶ Worker /ingest ──▶ R2  (arkiv, allt rådata)
  └─ summering.json ─────▶       │            │
                                 └──────▶ D1  (nyckeltal per minut)
                                             │
                            Cron 1 ggr/tim ──┤
                                             ├──▶ Claude API ──▶ D1 (analys)
                                             │
                                        Worker / ──────────────▶ webbsida
```

**Minneskortet förblir sanningen.** Uppladdning är en kopia, inte en flytt.
Ligger nätet nere fortsätter loggningen och filerna köar på kortet tills
förbindelsen kommer tillbaka. Ingen mätning går förlorad för att WiFi:t strular.

### Två dataströmmar, med flit

Att skicka 40 MB rådata till Claude varje timme vore både dyrt och meningslöst —
det är ~500 000 tokens i timmen av siffror som modellen ändå inte kan överblicka.
Kortet räknar därför ut nyckeltal per minut (min, max, medel, RMS per axel,
högsta total-g, antal sampel) och laddar upp dem separat. Det är några kilobyte
i timmen.

Det ger tre fördelar: analysen blir vass eftersom Claude får siffror den kan
resonera om i stället för en vägg av rådata; Workers gratisnivå räcker eftersom
den bara läser 60 små rader i stället för att tugga 1,7 MB CSV; och rådatan
finns ändå kvar i R2 för den dag du vill gräva i detaljerna.

---

## Del 1 — Firmware

### Nya filer

**`firmware/Gmate/netlog.h` / `netlog.cpp`**
WiFi-hantering och uppladdningskö.

- `netlog::begin()` — läser sparade WiFi-uppgifter, kopplar inte upp än
- `netlog::poll()` — anropas från loop; var tionde minut: koppla upp, ladda upp
  alla ostängda filer, koppla ner
- `netlog::status()` — antal filer i kö, byte i kö, senaste lyckade uppladdning,
  senaste fel — för skärmen
- `netlog::startPortal()` — startar konfigurationsläget

WiFi-uppgifter matas in via **WiFiManager** (bibliotek `tzapu/WiFiManager`):
kortet startar ett eget nätverk, du ansluter med mobilen och fyller i lösenordet
i webbläsaren. Ingen kod, ingen omflashning. Uppgifterna sparas i kortets
flashminne.

### Ändringar i befintliga filer

**`logger.cpp` — tidsbaserad filväxling**
Idag växlar loggen fil vid 64 MB. Det fungerar inte med uppladdning: uppladdaren
skulle behöva läsa samma fil som loggningen skriver till, vilket är precis den
sortens kapplöpning som ger svårfunna fel.

Lösningen är att växla fil **var femte minut** när uppladdning är påslagen.
Uppladdaren rör då bara **stängda** filer och kan aldrig krocka med skrivningen.
Det är också varför femminutersfilerna passar tiominutersbatcharna — varje batch
har alltid ett par färdiga filer att skicka.

Samtidigt läggs summeringen till: en löpande ackumulator per minut som skrivs som
`S0001_042.JSON` bredvid CSV-filen.

**`ui.cpp` — status på skärmen**
Rad i huvudvyn: WiFi-symbol, kö (`3 filer · 4,2 MB väntar`), tid sedan senaste
lyckade uppladdning. Rött när kön växer — då vet du att nätet strular utan att
behöva logga in någonstans.

**`config.h`** — serveradress, enhets-ID, delad hemlighet, uppladdningsintervall,
filväxlingstid.

**`Gmate.ino`** — koppla in `netlog`, lägg till menyval för WiFi-inställning.

### Uppladdningsprotokoll

```
POST https://<worker>.workers.dev/ingest
X-Device-Id: gmate-01
X-Api-Key:   <delad hemlighet>
X-Filename:  L0001_042.CSV
Content-Type: text/csv

<filens innehåll>
```

Svar `200` → filen byter namn till `.SNT` på kortet och räknas som skickad.
Allt annat → ligger kvar i kön och försöker igen nästa gång.

Den delade hemligheten är inte valfri. Utan den kan vem som helst som hittar
adressen fylla din lagring med skräp.

---

## Del 2 — Cloudflare

### Resurser

| Resurs | Namn | Gratisnivå räcker till |
|---|---|---|
| R2-bucket | `gmate-raw` | 10 GB → ~8 månader vid 5 Hz |
| D1-databas | `gmate` | 5 GB |
| Worker | `gmate` | 100 000 anrop/dygn (vi använder ~300) |

Filerna hamnar i R2 som `raw/<enhet>/<datum>/<filnamn>.csv`. Vid 5 Hz blir det
~1,2 GB/månad; över gratisnivåns 10 GB kostar det $0,015/GB/månad, alltså
ören. En regel som flyttar filer äldre än 90 dagar till billigare lagring läggs
in från början.

### Worker-rutter

**`POST /ingest`** — kontrollerar nyckeln, strömmar kroppen rakt till R2 utan att
läsa in den i minnet, och skriver summeringsraderna till D1 om filen är en
summering. Streaming är poängen: en Worker på gratisnivån har 10 ms CPU, och att
strömma en fil till R2 kostar nästan ingen CPU alls eftersom väntan på nätverket
inte räknas.

**`GET /`** — instrumentpanel: senaste analysen, graf över högsta g per timme
senaste dygnet, larm, uppladdningsstatus per enhet. Skyddad med samma nyckel.

**Cron `0 * * * *`** — timanalysen:
1. Läs senaste timmens 60 summeringsrader från D1
2. Räkna ut timsammanfattning (topp, medel, spridning, antal överskridanden)
3. Skicka till Claude tillsammans med föregående timmes analys som kontext
4. Spara svaret i D1; sätt larmflagga om Claude flaggar något

### Claude-anropet

Modell: **`claude-opus-5`**. Payloaden är liten (~3 000 tokens in, ~800 ut), så
kostnaden landar på ungefär **$25/månad** vid ett anrop i timmen dygnet runt.

Vill du ha det billigare finns `claude-haiku-4-5` som gör samma jobb för
ungefär **$5/månad** — sämre på att resonera kring mönster över tid, men fullt
kapabel att flagga tröskelöverskridanden. Det är ditt val; jag föreslår att
börja på Opus och byta ned om kostnaden stör.

Nyckeln lagras som Worker-secret (`wrangler secret put ANTHROPIC_API_KEY`),
aldrig i koden.

Prompten får timmens siffror plus föregående analys och ombeds svara strukturerat:
sammanfattning i en mening, avvikelser med tidpunkt, trend jämfört med tidigare
timmar, och en larmnivå. Strukturerat svar (`output_config.format`) gör att
instrumentpanelen kan färgkoda utan att gissa sig fram i fritext.

### Filer

```
cloud/
├─ wrangler.toml          # bindningar till R2, D1, cron
├─ src/index.ts           # rutter + cron-hanterare
├─ src/analyze.ts         # Claude-anropet
├─ src/dashboard.ts       # HTML-vyn
└─ schema.sql             # D1-tabeller
```

---

## Del 3 — Ström och solceller

Solcellsdriften dimensioneras för **mars till oktober**. November till februari
ligger utanför — se sista avsnittet.

Mätvärden först, dimensionering sen — men här är utgångspunkten:

| Post | Uppskattning |
|---|---|
| Kortet, skärm släckt, 5 Hz | 40–80 mA @ 3,7 V |
| WiFi i batchar var 10:e min | +3–8 mA i snitt |
| **Summa** | **~70 mA ≈ 0,26 W ≈ 6,2 Wh/dygn** |
| Med omvandlingsförluster (×1,5) | **~9,3 Wh/dygn** |

Sämsta månaden i mars–oktober-fönstret är mars, med ~1,5 soltimmar/dygn i
Sverige. Panel = 9,3 / (1,5 × 0,75) ≈ **8 W teoretiskt**.

**Rekommendation: 20 W panel.** Överdimensioneringen betalar för moln, snedställd
panel, smuts och snö i mars. Solceller dimensionerade mot medelvärdet står stilla
varje gång vädret inte samarbetar.

Batteri för 3–5 dygns mörker: 28–47 Wh. Två vägar:

1. **12 V-uppsättning (robustast):** 20 W panel → MPPT-laddregulator → 12 V
   LiFePO4 7–12 Ah (84–144 Wh) → 12 V-till-USB-omvandlare. Allt är standardvaror
   och laddregulatorn sköter batteriet ordentligt.
2. **Powerbank:** 20 W USB-panel → powerbank med genomladdning. Enklare, men
   många powerbanker klarar inte att laddas och urladdas samtidigt, och stänger
   av sig vid låg last. Kontrollera innan du köper.

Jag rekommenderar väg 1.

Siffrorna ovan är uppskattningar. **Mät med USB-mätare på din uppsättning innan
du litar på dem**, och provkör ett dygn innan du lämnar den.

---

## Verifiering

**Firmware:** kompileras i CI som idag. Uppladdningen testas mot en lokal
mottagare (`python3 -m http.server` räcker för att se att anropet går ut) innan
Cloudflare pekas in.

**Cloudflare:** `wrangler dev` kör Workern lokalt mot riktig R2 och D1.
Cron-hanteraren triggas manuellt med `curl "http://localhost:8787/__scheduled"`
så analysen går att testa utan att vänta en timme.

**Hela kedjan, i ordning:**
1. Kortet ansluter till WiFi och visar det på skärmen
2. En femminutersfil dyker upp i R2 inom tio minuter efter start
3. Summeringsrader syns i D1 (`wrangler d1 execute gmate --command "select * from minute_stats limit 5"`)
4. Cron körs manuellt → analys hamnar i D1 och syns på instrumentpanelen
5. **Dra ur WiFi:t i en timme.** Loggningen ska fortsätta, kön ska växa på
   skärmen, och allt ska laddas upp när nätet kommer tillbaka. Det här är det
   viktigaste testet — det är hela poängen med att kortet är sanningen.

---

## Vad planen inte omfattar

- **4G.** Inte aktuellt eftersom WiFi finns. Om platsen ändras är SIM7080G
  tillagd som UART-modul ett avgränsat tillägg.
- **Vinterdrift på solceller.** Mars–oktober var ditt svar. December i Sverige
  ger ~1 soltimme/dygn plus snö på panelen och kräver en helt annan
  dimensionering — troligen 50 W+ och sänkt arbetscykel på loggningen.
- **Flera enheter.** Protokollet har enhets-ID från början, så det skalar, men
  instrumentpanelen byggs för en enhet tills du behöver fler.
