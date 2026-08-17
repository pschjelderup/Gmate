# Gmate — orientering för den som tar över

G-kraftslogger och ecodrive-mätare på ett **Waveshare ESP32-S3-Touch-AMOLED-2.41**.
Arduino-skiss, byggd av GitHub Actions, installerad från en webbsida.

**Ägaren skriver ingen kod.** Skärmen och `README.md` är hela gränssnittet, och
båda är på svenska. Ett fel som bara syns i en kompilatorvarning är osynligt för
den som använder kortet — därför sitter mycket möda i att göra tillstånd
avläsbara på skärmen.

---

## Var saker ligger

| Sökväg | Innehåll |
|---|---|
| `firmware/Gmate/` | Arduino-skissen. En fil per ansvarsområde |
| `web/` | Flashsidan: `index.html` + `manifest.json` med platshållare |
| `.github/workflows/build.yml` | Bygger, paketerar och publicerar |
| `docs/` | Hur och varför — se nedan |
| `README.md` | Bruksanvisning på svenska, för en icke-programmerare |

Djupdykningar: [hårdvara](docs/hardvara.md) · [ecodrive](docs/ecodrive.md) ·
[flashkedjan](docs/flashkedjan.md) · [planerad uppkoppling](docs/plan-uppkoppling.md)

---

## Den enda arkitekturregel som betyder något

**All i2c-trafik och alla kortskrivningar sker i loggerns avläsningstråd.**

`logger.cpp` driver en FreeRTOS-tråd på kärna 0. Den läser sensorn och anropar
sedan `gnss::poll()`, `track::tick()` och `eco::tick()` i tur och ordning.
Ingenting annat rör bussen eller kortet.

Det gör samtidighetsfel strukturellt omöjliga i stället för osannolika. Bryt
inte mot det för att spara en rad. Taran i `eco.cpp` ser krånglig ut — knappen
lämnar en begäran och väntar på svar — och det är precis därför: att skriva
lodlinjen från pekskärmens tråd hade varit två rader kortare och ett
kapplöpningsfel.

UI-tråden läser lägesbilder genom `status()`-funktioner som tar mutex. Den
skriver aldrig.

---

## Konventioner

**Kommentarer förklarar varför, inte vad.** Koden säger redan vad den gör.
Kommentarerna i det här förrådet bär resonemanget — vilket alternativ som
valdes bort och vad som går sönder om någon ändrar tillbaka. Flera av dem är
det enda som hindrar att en riktig bugg återinförs som en "förenkling".

**Kod skrivs utan svenska diakriter.** Kommentarer och identifierare i `.cpp`
och `.h` använder `a` och `o` i stället för å, ä, ö. Strängar som visas på
skärmen har dem förstås.

**Text på skärmen går genom `sv()`** i `ui.cpp`. Det inbyggda typsnittet har en
glyf per byte enligt CP437, så UTF-8 måste översättas. Hanterade tecken: å ä ö
Å Ä Ö, grad och mittpunkt. **Allt annat blir `?`** — ett tecken som saknas i
tabellen ritas som skräp, vilket redan hänt två gånger. Utökar du texten,
utöka `sv()`.

---

## Skärmens geometri

Panelen är 450×600. Knappytor är `Rect`-konstanter i `ui.cpp` och används både
för att rita och för att avgöra var man tryckte — samma rektangel på båda
ställen, så de kan aldrig glida isär.

Textbredd är `antal tecken × 6 × storlek` pixlar. Räkna innan du lägger till en
etikett; överlappande text upptäcks annars först på riktig hårdvara.
Se [`skarmlayout`-skillen](.claude/skills/skarmlayout/SKILL.md).

---

## Bygga och släppa

Grenar byggs via sin pull request, `main` bygger för sig självt och publicerar
flashsidan. **En push till en gren utan öppen PR startar ingenting** — det har
lurat mig en gång, så öppna PR:en först.

Hela flödet med verifieringssteg: [`slapp-version`-skillen](.claude/skills/slapp-version/SKILL.md).

Två fällor i paketeringen som bygget nu vaktar åt dig, båda beskrivna i
[docs/flashkedjan.md](docs/flashkedjan.md): firmwarefilens namn måste vara unikt
per bygge, och avbildningen måste ha ett hål där NVS ligger.

---

## Vad som är verifierat och vad som inte är det

Det här är ett projekt utan hårdvara i utvecklingsmiljön. Skilj på:

| | |
|---|---|
| **Verifierat** | Pinnar (två oberoende källor), i2c-adresser, Qwiic-stiftordning (avläst från kortet), delarnas offsets (esptool lokalt), att allt kompilerar |
| **Bekräftat av ägaren** | Skärm, pekskärm, kortläsare, tara, gränsmenyn, att GPS-modulen svarar |
| **Oprövat** | Bubblans teckenkonvention vid inlärd riktning, GPX-filens kraschtålighet, fjortondagarsdrift |

Påstå inte att något fungerar för att det kompilerar. Ägaren provkör och
rapporterar; skriv så att en klistrad logg räcker för att ställa diagnos — se
[`gps-felsok`-skillen](.claude/skills/gps-felsok/SKILL.md) för hur den principen
såg ut när den behövdes.
