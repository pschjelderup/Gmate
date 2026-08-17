# Ecodrive — hur den räknar, och vad som gick fel

Premissen: **sparsam körning och mjuk körning är samma sak.** Bränsle går åt när
farten ändras, och ju häftigare desto mer. Därför går sparsamhet att mäta med
enbart accelerometern, utan att veta något om motorn.

Allt nedan bor i `firmware/Gmate/eco.cpp`.

---

## Kedjan

1. **Hitta lodlinjen.** Ett lågpassfilter på accelerationen ger tyngdkraftens
   riktning.
2. **Räkna bort den.** `h = a − (a·d)d` lämnar den vågräta accelerationen — det
   som faktiskt känns i sätet.
3. **Normera.** Dela med den uppmätta tyngdkraftens längd.
4. **Lär framåtriktningen** ur GPS-fartens derivata.
5. **Poäng och räknare** ur `|h|`.

Steg 2 gör att kortet får sitta hur som helst. En snett monterad enhet visar
inte konstant utslag, eftersom lutningen räknas bort.

---

## Buggen som är värd att förstå

Den första versionen frös lodlinjefiltret under manöver, så att en lång kurva
inte skulle tolkas som "ned":

```c
const bool manoeuvring = g_prevMagG > ECO_FREEZE_MAG_G || ...;
if (!manoeuvring) { /* uppdatera lodlinjen */ }
```

`g_prevMagG` räknas fram **ur lodlinjen**. Blev lodlinjen en gång fel — enheten
vreds, eller startade medan den bars — blev restvärdet stort, vilket tolkades
som manöver, vilket höll filtret fryst, vilket bevarade den felaktiga lodlinjen.

En återkoppling utan yttre referens. Den intygade sin egen riktighet och kom
aldrig ur det. Symptomet var att skärmen visade HÅRT i all evighet på ett bord.

**Och nödutgången satt låst med samma lås.** Taran krävde `magG < 0.05f` —
samma korrupta värde. Det enda som kunde räta till lodlinjen var utelåst av
precis den lodlinje som skulle rätas till.

### Lärdomen

*Ett villkor som styr en uppskattning får inte härledas ur uppskattningen
själv.* Det behövs en oberoende referens.

Här är den **gyrot**. Det skiljer de två fall accelerometern inte kan hålla
isär: en bil i jämn kurva har stor sidoacceleration **och** tydlig girhastighet
(0,3 g vid 50 km/h ger ~12 °/s), medan ett kort som ligger snett på ett bord har
lika stort utslag men står helt still.

```
vila = gyro < 2,5 °/s  och  råvektorn står still mellan avläsningar
```

| Läge | Filtret |
|---|---|
| Vila | Snabb tidskonstant, 2 s — den uppmätta vektorn *är* tyngdkraften |
| Fart, ingen manöver | Långsam, 30 s |
| Manöver | Fryst, **men aldrig mer än 12 s** |

Taket är ett andra skydd, oberoende av gyrot. En riktig kurva varar inte tolv
sekunder; en felaktig lodlinje varar tills strömmen bryts.

Praktisk följd: flyttar man hållaren rättar den sig själv inom några sekunder.
Taran finns för att slippa vänta och för att starta om riktningsinlärningen.

---

## Skalan

Allt uttrycks som **andel av den uppmätta tyngdkraften**, inte i sensorns
absoluta g:

```c
const float inv = 1.0f / gMag;
const float hx = (s.ax - along * dx) * inv;
```

Enheten i bruk visar 0,89 g i vila. Utan normeringen slog varje gräns in elva
procent för sent — 0,30-gränsen låg i praktiken på 0,34. Kvoten är dessutom
oberoende av mätområdet, så gränserna betyder samma sak vid ±2 g som vid ±16 g.

Loggfilen skriver fortfarande råvärden. Korrigeringen hör hemma i bedömningen,
inte i datat.

---

## Framåtriktningen

En accelerometer som ligger stilla kan omöjligt veta vilket håll som är framåt —
det finns ingen körriktning att observera. Farten vet det:

> Ökar farten pekar den vågräta accelerationen framåt. Minskar den pekar den
> bakåt.

Riktningen uppdateras exponentiellt mot varje sådan observation, projiceras in i
vägplanet så att backar inte tippar den, och sparas i flashminnet när den satt
sig.

**Utan GPS-fix lärs den aldrig in.** Då faller bubblan tillbaka på kortets egna
axlar: storleken stämmer, riktningen är godtycklig. Skärmen säger det rakt ut i
stället för att låtsas — annars ser en godtyckligt vänd bubbla precis lika
trovärdig ut som en inlärd.

`GAS`- och `BROMS`-etiketterna sätts ut först när riktningen är känd. En
felmärkt axel är sämre än ingen.

### Öppen fråga

Vilket håll en kurva ska slå åt när riktningen väl sitter är **inte avgjort**.
Två konsekventa konventioner finns:

| | Gas | Broms | Vänsterkurva |
|---|---|---|---|
| Accelerationsvektorn (nuvarande) | Upp | Ned | Vänster |
| Upplevd kraft | Ned | Upp | Höger |

Ägaren har uttryckt att vänsterkurva borde ge utslag åt höger, men observationen
gjordes utan inlärd riktning och kan därför inte avgöra saken. Frågan står kvar
tills GPS:en fungerar.

---

## Poängen

Sjunker med `(magG − mjuk gräns) × stränghet` per sekund, klättrar med
`100 / fönster` per sekund.

**Fönstret är det som ger siffran innebörd.** Utan det minns poängen allt lika
mycket och slutar säga något om hur det går just nu. En sammanhängande mjuk
sträcka lika lång som fönstret tar poängen från noll till hundra:

| Fönster | Siffran betyder |
|---|---|
| 1–2 min | Hur du kör just nu |
| 5–10 min | Hur den här delen av resan gått |
| 30–60 min | Hur hela resan gått |

---

## Gränserna

Mjuk gräns, hård gräns, bubblans ytterring, stränghet och fönster ställs i en
egen meny som nås **från ecodrive-skärmen**, inte från huvudmenyn. Två skäl:
det är där man står när man vill ändra något, och huvudmenyn är låst under
pågående loggning.

Gränsmenyn får vara öppen under loggning eftersom den bara påverkar bedömningen
på skärmen — aldrig vad som hamnar i filen. Det finns alltså ingen fil som kan
bli inkonsekvent.

Avslutningströskeln härleds ur den hårda i stället för att bli en egen ratt. Det
enda den gör är att hindra ett studsande värde från att räknas som flera
händelser — en knapp till att förstå utan något att välja på.

Startvärdena står överst i `config.h`, tillsammans med listorna över vad menyn
erbjuder.

---

## Gas, broms och kurva

Uppdelningen görs på **GPS-farten**, inte på accelerometerns axlar, just för att
den ska fungera oavsett hur kortet är vänt. Utan GPS räknas allt ihop till en
siffra och skärmen säger att uppdelningen inte går att göra — hellre det än att
visa "hård inbromsning" när föraren svängde.
