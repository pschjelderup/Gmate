---
name: gps-felsok
description: Ställ diagnos på GPS-mottagaren i Gmate utifrån serieloggen. Använd när användaren rapporterar att GPS:en söker satelliter utan att få fix, klistrar in en logg från webbflasharens konsol, undrar varför spårloggning eller ecodrive-riktningen inte fungerar, eller när ingen modul hittas på i2c-bussen.
---

# Felsök GPS:en

Skissen skriver en rad var femte sekund över serieporten. Den är byggd för att
en klistrad logg ska räcka för att ställa diagnos — utvecklingsmiljön har ingen
hårdvara, så siffrorna är hela underlaget.

```
GPS: avlasningar 374  paket 363  fixtyp 0  satelliter 0
```

| Fält | Betyder |
|---|---|
| `avlasningar` | Försök att läsa av modulen, ett per sekund |
| `paket` | Positionspaket som faktiskt kom |
| `fixtyp` | 0 = ingen, 2 = 2D, 3 = 3D |
| `satelliter` | Satelliter modulen **spårar** — inte bara de som ger fix |

Vid start skrivs också en avsökning av i2c-bussen med kända adresser i klartext.

## Beslutstabellen

| Observation | Diagnos | Åtgärd |
|---|---|---|
| `0x42` saknas i avsökningen | Modulen svarar inte alls | Kabel eller kontakt. Kontrollera att den sitter i porten märkt **I2C**, inte UART eller RTC — en Qwiic-kabel passar i alla tre |
| `avlasningar` stiger, `paket` står still | Svarar på adressen men levererar inget | Ovanligt. Misstänk konfiguration eller trasig modul |
| `paket` följer `avlasningar`, `satelliter` **0** över flera minuter | **Modulen mår bra men ser ingenting** | Antenn eller sikt — se nedan |
| `satelliter` > 0, `fixtyp` 0 | Den ser satelliter men har inte låst | Vänta. Kallstart tar 25–30 s, längre utan almanacka |
| `fixtyp` 3 | Allt fungerar | – |

Att `paket` ligger några procent under `avlasningar` är normalt: ibland finns
inget nytt sedan förra avläsningen.

## Noll satelliter är starkare än låga satelliter

`satelliter` räknar satelliter som **spåras**, inte bara de som bidrar till fix.
En fungerande antenn plockar upp något inom en minut även inomhus vid ett
fönster.

**Flera minuter på exakt noll betyder att antenningången är död** — i praktiken
ingen antenn ansluten. Det är en helt annan slutsats än "dålig sikt", och den
har redan bekräftats en gång i det här projektet.

Vanligaste orsaken: **u.FL-varianten av SparkFun-kortet har ingen antenn alls**.
Uttaget är bara ett anslutningsdon. Inköpsdetaljer i
[docs/hardvara.md](../../../docs/hardvara.md).

## Vad firmwaren inte kan svara på

Den kan **inte** skilja "ingen antenn" från "antenn utan sikt". u-blox har
antennövervakning i protokollet, men den kräver kretsar på breakout-kortet som
inte gått att verifiera. Säg det rakt ut i stället för att låta noll satelliter
framstå som ett direkt besked om antennen.

## Följdfel som inte är egna buggar

Utan fix finns ingen fart, och utan fart:

- **Framåtriktningen i ecodrive lärs aldrig in.** Bubblan får rätt storlek men
  godtycklig riktning. Rapporter om att gas ger utslag åt fel håll är väntade i
  det läget — leta inte efter en teckenbugg.
- **Gas/broms/kurva kan inte delas upp**, eftersom uppdelningen görs på farten.
- **Spårknappen är släckt.** Utan position finns inget spår att rita.

Skärmen ska säga vilket läge den är i. Säger den `Riktning: kör, gasa och
bromsa` utan att fix finns är **det** en bugg — uppmaningen ber då om något som
inte kan ge resultat.
