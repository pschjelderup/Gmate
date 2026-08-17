---
name: slapp-version
description: Släpp en ny firmwareversion till flashsidan i Gmate-projektet. Använd när ändringar i firmware/ eller web/ ska ut till kortet, när användaren frågar vilken version som ligger på flashsidan, eller när ett bygge behöver följas från pull request till publicerad sida. Täcker även varför en push ibland inte startar något bygge alls.
---

# Släpp en version

Gmate byggs av GitHub Actions och installeras från en webbsida. Kedjan har några
steg där tystnad ser ut som framgång, så varje steg verifieras.

## Ordningen

### 1. Grenen måste ha en öppen pull request

`push` bygger **bara `main`**. Grenar byggs via sin pull request. En push till en
gren utan öppen PR startar **ingenting** — inget fel, ingen körning, bara tyst.

Det inträffar särskilt lätt direkt efter att en PR mergats: grenen finns kvar,
man fortsätter på den, pushar, och väntar på ett bygge som aldrig startade.

Är föregående PR mergad: starta om grenen från `main` innan nytt arbete.

```bash
git fetch origin main && git checkout -B <gren> origin/main
```

### 2. Följ CI på **rätt** commit

Fråga aldrig efter "senaste körningen på grenen" — den kan vara den föregående
commitens, och då rapporterar man grönt för fel kod. Filtrera på commit-id:

```python
runs = [r for r in alla if r["head_sha"].startswith(SHA)]
```

### 3. Efter merge: följ bygget på `main`

Det är den körningen som publicerar. Vänta på **båda** jobben:

```
build:completed/success  publicera:completed/success
```

### 4. Bekräfta att sidan faktiskt fick den

```bash
curl -sS -H "Authorization: Bearer $GITHUB_TOKEN" \
  "https://api.github.com/repos/pschjelderup/Gmate/deployments?environment=github-pages&per_page=2" \
| python3 -c 'import sys,json;[print(d["sha"][:7], d["created_at"]) for d in json.load(sys.stdin)]'
```

Översta raden ska vara commiten du just mergade. `pschjelderup.github.io` går
inte att hämta från byggmiljön — API:et är vägen.

### 5. Ge användaren versionen

Säg **vilket commit-id** som ska stå på flashsidan. Utan det kan de inte skilja
en lyckad omflashning från en misslyckad.

## Vanliga lägen

| Läge | Vad det betyder |
|---|---|
| Ingen körning för commiten | Grenen saknar öppen PR — öppna en |
| `cancelled` | En nyare push avbröt den. Följ den nya i stället |
| `publicera:skipped` | Körningen gällde en gren, inte `main`. Väntat |
| Artefaktsteget rött, bygget grönt | Fullt artefaktlager. Kontoproblem, inte kodfel |
| Sidan visar en äldre version | Bygget kan fortfarande pågå — kontrollera innan du kallar det fel |

## Berätta det som är sant

Bygget verifierar att koden **kompilerar**. Ingenting mer. Skilj alltid på det
och på att en funktion fungerar — den här firmwaren utvecklas utan hårdvara i
miljön, och ägaren är den som provkör.

Har paketeringen ändrats: säg vad bygget kontrollerade, inte bara att det blev
grönt. Se [docs/flashkedjan.md](../../../docs/flashkedjan.md).
