# Flashkedjan

Från commit till kort:

```
push till main
   └─ arduino-cli compile
        └─ esptool merge_bin  ×2   → site/gmate-<sha>-a.bin
        │                            site/gmate-<sha>-b.bin
        └─ sed på web/*         → site/index.html, site/manifest.json
             └─ GitHub Pages    → pschjelderup.github.io/Gmate
                  └─ ESP Web Tools i webbläsaren → kortet
```

Två fällor har redan slagit till på riktigt. Båda vaktas nu av bygget.

---

## Fälla 1: fast filnamn för föränderligt innehåll

Firmwarefilen hette `gmate-merged.bin` i varje bygge. **Samma adress för evigt.**
En webbläsare som en gång hämtat filen har ingen anledning att hämta om den, så
gammal firmware kan installeras hur många gånger som helst efter en ny
publicering.

Symptomet var "flashsidan installerar alltid den första versionen", och det tog
ett tag att ringa in eftersom bygget och publiceringen *gjorde helt rätt* —
felet låg i leveransen.

**Åtgärd.** Filnamnet innehåller byggets commit-id. En cachad kopia kan aldrig
återanvändas eftersom adressen är ny varje gång. Manifestet hämtas dessutom med
cache-brytande parameter och `cache: no-store`, satt från JavaScript så att det
fungerar även om själva HTML-sidan ligger kvar i cachen.

**Och — minst lika viktigt — versionen syns nu.** Vid installationsknappen står
version, byggtid och filnamn, läst ur manifestet. På kortet står byggtiden i
**MENY**, uppe till höger. Stämmer de två överens gick flashningen igenom.

Innan dess såg en misslyckad omflashning exakt likadan ut som en lyckad. Det var
den verkliga bristen; cachen var bara det som utlöste den.

---

## Fälla 2: avbildningen raderade NVS

Den sammanslagna filen var **sammanhängande från adress 0**. `merge_bin` fyller
gapen mellan delarna med `0xFF`, så filen täckte även NVS-området
`0x9000–0xE000`. Flashningen raderar varje sektor den skriver.

Följden: inställningar, tarat monteringsläge och **inlärd framåtriktning**
raderades vid varje omflashning — oavsett om man kryssade i *Erase device* eller
inte. Kryssrutan var alltså utan verkan, och riktningsinlärningen som tar några
gaspådrag att bygga upp kastades bort vid varje ny version.

**Åtgärd.** Två delar med ett hål där NVS ligger:

| Del | Adress | Innehåll | Slutar |
|---|---|---|---|
| A | `0x0` | Bootloader + partitionstabell | `0x8C00` |
| *hål* | `0x9000` | **NVS — rörs aldrig** | `0xE000` |
| B | `0xe000` | Boot-väljare + appen | – |

ESP Web Tools stödjer flera delar i manifestet, så det är inget hack.

### `--target-offset` är inte valfri

`merge_bin` padar **alltid från adress 0**, även när den lägsta angivna adressen
är `0xe000`:

```
Wrote 0x11000 bytes ... ready to flash to offset 0x0     <- utan flaggan
Wrote  0x3000 bytes ... ready to flash to offset 0xe000  <- med
```

Utan flaggan blir del B `0x11000` byte och skriver över NVS ändå — alltså exakt
det fel vi försöker undvika, med en fil som *ser* rätt ut.

### Erase har fått en mening

Kryssrutan är nu vägen tillbaka till fabriksinställning i stället för en ruta
utan verkan. Det står på flashsidan och i README.

---

## Vad bygget kontrollerar

Ett fel i paketeringen ska falla i CI, inte upptäckas av någon som undrar vart
taran tog vägen:

| Kontroll | Fångar |
|---|---|
| `SIZE_A` ≤ 36864 | Att del A växer in i NVS-området |
| `test -f` på båda delarna | Att manifestet pekar på en fil som inte finns |
| `grep` efter `__VERSION__`/`__BUILT__` | Stavfel i platshållare — en sida som ser hel ut men laddar ner ingenting |

---

## Övrigt värt att veta

**Grenar byggs via sin pull request.** `push` är begränsad till `main`, eftersom
det är den körningen som publicerar sidan. En push till en gren *utan öppen PR
startar ingenting alls* — lätt att missa när man precis mergat en PR och
fortsätter på samma gren.

**Biblioteksversionerna är låsta med flit** i workflowen. Ett biblioteksbyte ska
vara ett medvetet beslut, inte något som inträffar för att någon annan släppte
en ny version samma dag.

**`FlashMode=qio` i FQBN, men `--flash_mode dio` i merge_bin.** merge_bin skriver
om bootloaderns huvud, så det är dio som gäller på kortet. Avsiktligt — dio
fungerar överallt.

**Artefaktuppladdningen har `continue-on-error`.** Ett fullt artefaktlager är ett
kontoproblem, inte ett fel i koden, och ska inte rapporteras som ett trasigt
bygge. Det har hänt en gång.
