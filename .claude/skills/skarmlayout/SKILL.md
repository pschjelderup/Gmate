---
name: skarmlayout
description: Rita och placera text, knappar och paneler på Gmates 450x600-skärm. Använd när en knapp, meny, etikett eller menyrad ska läggas till eller flyttas i ui.cpp, när svensk text ska visas på skärmen, eller när något ser ut att hamna ovanpå något annat. Räknar bredd och höjd i förväg, eftersom överlapp annars upptäcks först på riktig hårdvara.
---

# Skärmlayout

Panelen är **450×600**. Allt ritas i en PSRAM-buffert och skickas i ett svep med
`flush()`. Utvecklingsmiljön har ingen hårdvara, så **överlapp måste räknas ut,
inte ses**.

## Måtten

| Storlek | Bredd per tecken | Höjd |
|---|---|---|
| 1 | 6 px | 8 px |
| 2 | 12 px | 16 px |
| 3 | 18 px | 24 px |

`bredd = antal tecken × 6 × storlek`

Räkna **efter** `sv()`-översättningen: å, ä, ö blir ett tecken var, inte två.

## Räkna innan du skriver

1. Bredd på texten, jämför med panelens innerkant och med nästa element.
2. Höjd på raden, jämför med radens underkant.
3. Vid en ny rad i en lista: räcker utrymmet ned till knappen under?

Konkret exempel som redan fällt en ändring: menyraderna har minusknappen på
`x=228`. En etikett från `x=32` får alltså vara **högst 196 px**, det vill säga
16 tecken vid storlek 2. `"Bubblans ytterring"` är 18 tecken och hamnade under
knappen; `"Ytterring"` med en mindre förklaringsrad under löste det.

## Knappytor

En `Rect`-konstant i `ui.cpp` används **både** för att rita och för att avgöra
var man tryckte:

```c
const Rect kBtnEcoLimits = {159, 528, 131, 56};
...
drawButton(kBtnEcoLimits, C_PANEL, "GRÄNSER", 2, C_TEXT);   // rita
if (ui::kBtnEcoLimits.contains(x, y)) { ... }               // träffyta
```

Ha aldrig två uppsättningar koordinater för samma knapp. De glider isär, och
resultatet är en knapp som ser rätt ut men reagerar bredvid sig själv.

Rader med olika täthet har **egna** geometrifunktioner — `settingsMinus/Plus`
för inställningsmenyns fyra rader, `ecoMinus/Plus` för gränsmenyns fem. Ändra
inte den ena i tron att den bara används på ett ställe.

## Svenska tecken

All text går genom `sv()`. Typsnittet har en glyf per byte enligt CP437, så
UTF-8 måste översättas.

Hanterade: `å ä ö Å Ä Ö`, gradtecken, mittpunkt. **Allt annat blir `?`.**

Det har redan gått fel två gånger — en mittpunkt som ritades som två skräptecken
och ett `✕` som inte fanns i tabellen. Lägger du till ett tecken utanför listan,
utöka `sv()` samtidigt.

## Färgerna betyder något

`C_GREEN`, `C_WARN` och `C_RED` följer hur hårt det körs och används likadant
över hela ecodrive-skärmen, så att läget uppfattas i ögonvrån utan att läsas.
Använd dem inte som dekoration.

Element som betyder olika saker ska ha **skilda** färger. Bubblans tre ringar —
mjuk gräns, hård gräns, toppvärde — var nära att bli två nyanser av orange, och
hade då varit oläsbara.

## Visa tillstånd som inte går att gissa

Skärmen är hela gränssnittet för någon som inte läser kod. Ett läge som inte
syns finns inte:

- Grå kontra gul GPS-prick skiljer "ingen mottagare" från "mottagare utan
  position". Att visa ingenting alls hade sett likadant ut som båda.
- Riktningsraden i ecodrive säger om bubblan är inlärd eller godtyckligt vänd.
  Utan den ser en godtycklig bubbla lika trovärdig ut som en riktig.
- Byggtiden i **MENY** är det enda sättet att avgöra om en omflashning gick
  igenom.

Lägger du till ett läge koden kan hamna i: ge det en rad.
