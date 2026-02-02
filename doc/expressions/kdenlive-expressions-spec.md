# Kdenlive Expressions — specyfikacja implementacji

Silnik wyrażeń (expressions) dla Kdenlive, kompatybilny z After Effects Expressions JS. Pozwala parametrom efektów przyjmować wyrażenia JavaScript ewaluowane per-klatka w runtime — bez ręcznego stawiania keyframe'ów.

## Cel

Umożliwić dynamiczne, data-driven efekty w Kdenlive:

```javascript
// Opacity klipu pulsuje z basem muzyki:
amp = audioLevel("Both", time);
linear(amp, 0.05, 0.8, 0.3, 1.0)
```

```javascript
// Blur rośnie w czasie:
linear(time, 0, duration, 0, 20)
```

```javascript
// Losowe drganie pozycji (camera shake):
wiggle(4, 15)
```

Bez expression engine te efekty wymagają ręcznego generowania setek keyframe'ów (przez agenta AI lub człowieka). Z expression engine — jedna linia kodu, żywy link, natychmiastowa zmiana po edycji audio.

---

## Architektura

```
Kdenlive (C++)
  │
  │  Per-klatka: "jaka wartość parametru X w klatce N?"
  │
  ▼
Expression Engine (QuickJS, embedded C)
  │
  │  Ewaluuje JS: "linear(audioLevel('Both', time), 0.05, 0.8, 0.3, 1.0)"
  │
  │  Odczytuje:
  │  ├── time, duration, fps        ← z kontekstu renderowania
  │  ├── value                      ← bazowa wartość parametru (keyframe lub stała)
  │  ├── audioLevel(channel, t)     ← z pre-baked audio cache
  │  └── thisClip.*, thisTrack.*    ← metadane klipu/tracku
  │
  ▼
Wynik: liczba (float) lub wektor [float, float, ...]
  │
  ▼
MLT filter params — parametr efektu przyjmuje obliczoną wartość dla tej klatki
```

### QuickJS jako silnik JS

- **Repozytorium:** https://bellard.org/quickjs/
- **Licencja:** MIT
- **Rozmiar:** ~600KB skompilowany, zero zależności
- **Standard:** ES2020 (pełny, w tym async/await, Proxy, BigInt — choć nie potrzebujemy wszystkiego)
- **Embedding:** czyste C API, łatwe do wbudowania w projekt C++/CMake
- **Porównanie:** V8 = ~30MB + skomplikowany build. Duktape = ES5 only. QuickJS = złoty środek.

### Integracja z Kdenlive

Expression engine NIE jest pluginem MLT. Jest częścią forka Kdenlive:

1. **Przechowywanie** — wyrażenie zapisane w projekcie .kdenlive jako atrybut XML na parametrze efektu (np. `<property name="opacity" expression="linear(time, 0, 5, 0, 100)">50</property>`)
2. **Ewaluacja** — w renderze, przed przekazaniem wartości do MLT, Kdenlive ewaluuje wyrażenie i podstawia wynik
3. **Fallback** — jeśli wyrażenie jest puste lub błędne, używana jest standardowa wartość/keyframe'y (graceful degradation)

To oznacza, że MLT nie musi wiedzieć o wyrażeniach. Kdenlive robi ewaluację "nad" MLT i podaje MLT gotowe wartości per-klatka.

---

## API wyrażeń — co implementujemy

### Faza 1: Minimum Viable (audio-reactive efekty)

#### Globalne zmienne (read-only)

| Zmienna | Typ | Opis | Odpowiednik AE |
|---------|-----|------|-----------------|
| `time` | `float` | Czas aktualnej klatki w sekundach od początku timeline | `time` |
| `frame` | `int` | Numer klatki na timeline (0-based) | `timeToFrames(time)` |
| `duration` | `float` | Czas trwania klipu w sekundach | `thisLayer.outPoint - thisLayer.inPoint` |
| `fps` | `float` | FPS projektu | `1 / thisComp.frameDuration` |
| `value` | `float\|array` | Bazowa wartość parametru (z keyframe'ów lub stała) | `value` |
| `index` | `int` | Indeks klipu na tracku (0-based) | `thisLayer.index` |

#### Audio

| Funkcja | Sygnatura | Opis |
|---------|-----------|------|
| `audioLevel(channel, t)` | `(string, float) → float` | Amplituda audio (0.0–1.0) w czasie `t`. Channel: `"Left"`, `"Right"`, `"Both"`. Dane z pre-baked cache (patrz sekcja Audio). |
| `audioRms(channel, t, window)` | `(string, float, float) → float` | RMS audio w oknie `window` sekund wokół `t`. Gładsze niż peak. |

Uwaga: w AE audio wymaga "Convert Audio to Keyframes" → slider → expression. U nas `audioLevel()` jest natywną funkcją — prostsze i wydajniejsze.

#### Interpolacja (kompatybilne z AE)

| Funkcja | Sygnatura | Opis |
|---------|-----------|------|
| `linear(t, tMin, tMax, vMin, vMax)` | `(float, float, float, float, float) → float` | Liniowa interpolacja. Jeśli `t < tMin` → `vMin`, `t > tMax` → `vMax`. |
| `linear(t, vMin, vMax)` | `(float, float, float) → float` | Skrót: `t` w zakresie 0–1. |
| `ease(t, tMin, tMax, vMin, vMax)` | `(float, float, float, float, float) → float` | Smooth ease in+out (cubic Hermite). |
| `easeIn(t, tMin, tMax, vMin, vMax)` | `(float, float, float, float, float) → float` | Ease tylko na wejściu. |
| `easeOut(t, tMin, tMax, vMin, vMax)` | `(float, float, float, float, float) → float` | Ease tylko na wyjściu. |

#### Random / Noise (kompatybilne z AE)

| Funkcja | Sygnatura | Opis |
|---------|-----------|------|
| `wiggle(freq, amp)` | `(float, float) → float` | Losowe odchylenie od `value` z częstotliwością `freq` Hz i amplitudą `amp`. Deterministyczne (ten sam seed per klip). |
| `wiggle(freq, amp, octaves, ampMult, t)` | `(float, float, int, float, float) → float` | Pełna wersja z oktawami szumu. |
| `random()` | `() → float` | Losowa 0–1 (per-klatka, deterministyczna z seed). |
| `random(min, max)` | `(float, float) → float` | Losowa w zakresie. |
| `noise(v)` | `(float) → float` | Perlin noise 1D, zwraca -1 do 1. |
| `seedRandom(seed, timeless)` | `(int, bool) → void` | Ustaw seed. `timeless=true` → ta sama wartość na każdej klatce. |

#### Utility

| Funkcja | Sygnatura | Opis |
|---------|-----------|------|
| `clamp(val, min, max)` | `(float, float, float) → float` | Ograniczenie do zakresu. |
| `posterizeTime(fps)` | `(float) → void` | Ewaluuj wyrażenie z mniejszą częstotliwością (step effect). |
| `degreesToRadians(deg)` | `(float) → float` | Konwersja. |
| `radiansToDegrees(rad)` | `(float) → float` | Konwersja. |
| `smooth(width, samples)` | `(float, int) → float` | Temporalne wygładzanie `value` (uśrednienie w oknie). |
| `timeToCurrentFormat(t?, fps?, isDuration?)` | `(float?, float?, bool?) → string` | Konwersja czasu na timecode w FPS projektu (domyślnie `thisProject.fps`, nie 30). Odpowiednik AE `timeToCurrentFormat()`. |

#### thisProperty (obiekt AE-kompatybilny)

| Składnia | Opis | Odpowiednik AE |
|----------|------|-----------------|
| `thisProperty.value` | Aktualna wartość parametru (= global `value`) | `thisProperty.value` |
| `thisProperty.numKeys` | Liczba keyframe'ów (= global `numKeys`) | `thisProperty.numKeys` |
| `thisProperty.wiggle(freq, amp, ...)` | Deleguje do globalnego `wiggle()` | `thisProperty.wiggle()` |
| `thisProperty.loopOut(type?, nKeys?)` | Deleguje do globalnego `loopOut()` | `thisProperty.loopOut()` |
| `thisProperty.loopIn(type?, nKeys?)` | Deleguje do globalnego `loopIn()` | `thisProperty.loopIn()` |
| `thisProperty.smooth(width?, samples?)` | Deleguje do globalnego `smooth()` | `thisProperty.smooth()` |
| `thisProperty.valueAtTime(t)` | Deleguje do globalnego `valueAtTime()` | `thisProperty.valueAtTime()` |
| `thisProperty.velocityAtTime(t)` | Deleguje do globalnego `velocityAtTime()` | `thisProperty.velocityAtTime()` |
| `thisProperty.speedAtTime(t)` | Deleguje do globalnego `speedAtTime()` | `thisProperty.speedAtTime()` |
| `thisProperty.key(index)` | Deleguje do globalnego `key()` | `thisProperty.key()` |
| `thisProperty.nearestKey(t)` | Deleguje do globalnego `nearestKey()` | `thisProperty.nearestKey()` |
| `thisProperty.temporalWiggle(...)` | Deleguje do globalnego `temporalWiggle()` | `thisProperty.temporalWiggle()` |
| `thisProperty.loopInDuration(...)` | Deleguje do globalnego `loopInDuration()` | `thisProperty.loopInDuration()` |
| `thisProperty.loopOutDuration(...)` | Deleguje do globalnego `loopOutDuration()` | `thisProperty.loopOutDuration()` |

Obiekt `thisProperty` istnieje wyłącznie dla kompatybilności z AE — wszystkie metody delegują do istniejących globali. Dzięki temu wyrażenia AE typu `thisProperty.wiggle(3, 50)` i `thisProperty.loopOut("cycle")` działają bez zmian.

Standardowe `Math.*` z JavaScript (Math.sin, Math.cos, Math.abs, Math.pow, Math.sqrt, Math.min, Math.max, Math.PI, Math.E, etc.) dostępne natywnie przez QuickJS.

### Faza 2: Rozszerzenia

#### Referencje do innych warstw/klipów

| Składnia | Opis | Odpowiednik AE |
|----------|------|-----------------|
| `thisClip.position` | Pozycja klipu na timeline (frames) | `thisLayer.inPoint` |
| `thisClip.duration` | Czas trwania klipu | `thisLayer.outPoint - thisLayer.inPoint` |
| `thisClip.name` | Nazwa pliku źródłowego | `thisLayer.name` |
| `thisTrack.index` | Indeks tracku | — |
| `clip(name).effect(name).param(name)` | Odczyt parametru z innego klipu po nazwie | `thisComp.layer(name).effect(name)(name)` |
| `clip(index).effect(name).param(name)` | Odczyt parametru z innego klipu po pozycji na tym samym tracku (0-based, od lewej) | `thisComp.layer(index).effect(name)(name)` |
| `clip(thisClip, relIndex)` | Odczyt klipu po offsetcie względem referencji. `clip(thisClip, -1)` = poprzedni, `clip(thisClip, 1)` = następny. | `thisComp.layer(thisLayer, relIndex)` |
| `thisProject.width` | Szerokość projektu w pikselach | `thisComp.width` |
| `thisProject.height` | Wysokość projektu w pikselach | `thisComp.height` |
| `thisProject.fps` | FPS projektu | `1 / thisComp.frameDuration` |
| `thisProject.duration` | Łączny czas trwania timeline w sekundach | `thisComp.duration` |
| `thisProject.frameDuration` | Czas trwania jednej klatki w sekundach (1/fps) | `thisComp.frameDuration` |
| `thisProject.pixelAspect` | Pixel aspect ratio (1.0 = kwadratowe piksele) | `thisComp.pixelAspect` |
| `thisProject.name` | Nazwa pliku projektu | `thisComp.name` |
| `thisProject.fullPath` | Pełna ścieżka do pliku projektu | `thisProject.fullPath` |
| `thisProject.numTracks` | Liczba tracków (video + audio) | `thisComp.numLayers` |
| `thisProject.displayStartTime` | Offset czasu początkowego timeline w sekundach | `thisComp.displayStartTime` |
| `thisProject.bgColor` | Kolor tła jako `[r,g,b,a]` (znormalizowane 0-1) | `thisComp.bgColor` |

#### Looping (kompatybilne z AE)

| Funkcja | Opis |
|---------|------|
| `loopIn("cycle", nKeys)` | Pętla keyframe'ów od początku. |
| `loopOut("cycle", nKeys)` | Pętla keyframe'ów od końca. |
| `loopIn("pingpong", nKeys)` | Ping-pong od początku. |
| `loopOut("pingpong", nKeys)` | Ping-pong od końca. |

#### Wektory (kompatybilne z AE)

Dla parametrów wielowymiarowych (position, scale, color):

| Funkcja | Opis |
|---------|------|
| `add(a, b)` | Dodawanie wektorów. |
| `sub(a, b)` | Odejmowanie. |
| `mul(a, s)` | Mnożenie przez skalar. |
| `length(a)` | Długość wektora. |
| `normalize(a)` | Wektor jednostkowy. |

#### Kolor

| Funkcja | Opis |
|---------|------|
| `rgbToHsl([r,g,b,a])` | RGBA → HSLA. |
| `hslToRgb([h,s,l,a])` | HSLA → RGBA. |
| `hexToRgb("#ff0000")` | Hex → [r,g,b]. |

### Faza 3: Zaawansowane

- `sampleImage(x, y, radius)` — odczyt koloru piksela z klatki (kosztowne, ale potężne)
- `marker.key(i).time` / `marker.nearestKey(t)` — odczyt markerów timeline
- `valueAtTime(t)` — odczyt wartości parametru w innym czasie (do opóźnień, echa) — **ZAIMPLEMENTOWANE** (patrz Faza 1 keyframe access)

**Zaimplementowane z Fazy 2/3:**
- `thisProject.*` — pełny obiekt z właściwościami projektu (odpowiednik AE `thisComp`). Patrz tabela w sekcji "Referencje do innych warstw/klipów".
- `clip(index)` — referencja do klipu po pozycji na tym samym tracku (0-based). Uzupełnia istniejące `clip("name")`.
- `clip(thisClip, relIndex)` — referencja relatywna do klipu (odpowiednik AE `thisComp.layer(thisLayer, relIndex)`). `clip(thisClip, -1)` = poprzedni klip, `clip(thisClip, 1)` = następny.
- `thisProperty` — obiekt reprezentujący aktualny parametr. Deleguje do globalnych funkcji (`wiggle`, `loopOut`, `smooth`, `valueAtTime`, etc.). Pełna kompatybilność z AE patterns `thisProperty.wiggle()` / `thisProperty.loopOut()`.
- `timeToCurrentFormat(t?, fps?, isDuration?)` — konwersja czasu na timecode z użyciem FPS projektu (nie sztywne 30fps jak `timeToTimecode`).
- `marker.key(n).protectedRegion` — alias dla `marker.key(n).duration`, kompatybilny z AE.
- `thisLayer` — globalny alias dla `thisClip` (ten sam obiekt). Wyrażenia AE typu `thisLayer.startTime` działają bez zmian.
- `thisComp` — globalny alias dla `thisProject` (ten sam obiekt). Wyrażenia AE typu `thisComp.width` działają bez zmian.
- `layer()` — globalny alias dla `clip()`. `layer(0)` = `clip(0)`, `layer("name")` = `clip("name")`.
- `thisClip.inPoint`, `.outPoint`, `.startTime`, `.index` — AE-kompatybilne właściwości klipu (odpowiedniki `thisLayer.inPoint`, `.outPoint`, `.startTime`, `.index`).
- `thisClip.hasVideo`, `.hasAudio` — czy klip źródłowy ma video/audio stream.
- `thisClip.source` — sub-obiekt z metadanymi źródła (`.name`, `.width`, `.height`). Odpowiednik AE `thisLayer.source`.
- `clip(N).name`, `.index`, `.inPoint`, `.outPoint`, `.startTime`, `.duration`, `.width`, `.height`, `.hasVideo`, `.hasAudio`, `.source.*` — metadane na obiektach `ClipRef` zwróconych przez `clip()`.

---

## Audio — implementacja

### Pre-baked audio cache

Wyrażenia NIE czytają audio w real-time per-klatka. Zamiast tego:

1. Przy otwarciu projektu (lub przy zmianie audio) Kdenlive analizuje audio tracki
2. Generuje tablicę wartości: `float[] amplitudes` — jedna wartość per klatka, per kanał
3. Tablica jest dostępna dla expression engine jako szybki lookup: `audioLevel("Both", time)` = `amplitudes[frame]`

To jest analogiczne do AE "Convert Audio to Keyframes" — ale automatyczne i niewidoczne dla użytkownika.

### Dane audio w cache

```
audio_cache = {
    "Both":  [0.001, 0.001, 0.003, 0.171, 0.188, ...],  // peak per frame
    "Left":  [...],
    "Right": [...],
    "rms_Both":  [...],  // RMS w oknie ~5 klatek
    "rms_Left":  [...],
    "rms_Right": [...]
}
```

Istniejąca infrastruktura: Kdenlive C++ ma `audioFrameCache` z surowymi int16_t per klatka. Nasz fork ma `scriptGetAudioLevels` (peak per chunk). Trzeba rozszerzyć o:
- RMS per klatka (nie tylko peak)
- Dostęp z expression engine (C → C++ callback)

---

## Format zapisu w .kdenlive XML

Wyrażenie jest atrybutem na property efektu:

```xml
<!-- Standardowy parametr (bez wyrażenia): -->
<filter mlt_service="brightness">
  <property name="level">0.8</property>
</filter>

<!-- Parametr z wyrażeniem: -->
<filter mlt_service="brightness">
  <property name="level" expression="linear(audioLevel('Both', time), 0.05, 0.8, 0.3, 1.0)">0.8</property>
</filter>
```

Atrybut `expression` zawiera kod JS. Wartość tekstowa `0.8` to fallback (użyta gdy wyrażenie nie istnieje lub błędne). Kdenlive bez naszego patcha zignoruje nieznany atrybut i użyje wartości — backward compatible.

---

## Rendering pipeline

```
Dla każdej klatki N:
  Dla każdego klipu z efektami:
    Dla każdego parametru z expression != "":
      1. Ustaw kontekst: time = N/fps, frame = N, duration = clip.length/fps, value = bazowa_wartosc
      2. Załaduj audio cache do kontekstu (jeśli audioLevel/audioRms użyte)
      3. Ewaluuj JS przez QuickJS → wynik float
      4. Przekaż wynik do MLT jako wartość parametru dla tej klatki
```

### Wydajność

- QuickJS ewaluuje proste wyrażenia w ~1-5μs per wywołanie
- Przy 25fps × 38 klipów × ~3 parametry z wyrażeniami = ~2850 ewaluacji/sekundę
- Łączny narzut: <15ms/sekundę renderingu — nieistotny vs czas renderowania MLT

### Optymalizacje

- **Cache wyników** — jeśli wyrażenie nie zależy od `time`/`frame` (np. `value * 2`), ewaluuj raz
- **Detect audio dependency** — skanuj wyrażenie na `audioLevel`/`audioRms`; jeśli brak, nie ładuj audio cache
- **posterizeTime()** — ewaluuj co N klatek, interpoluj resztę

---

## Kompatybilność z After Effects

### Co jest kompatybilne 1:1

- `time`, `value`, `fps`, `duration`
- `linear()`, `ease()`, `easeIn()`, `easeOut()` — identyczne sygnatury i semantyka
- `wiggle()` — identyczna sygnatura (5 parametrów)
- `random()`, `gaussRandom()`, `noise()`, `seedRandom()`
- `clamp()`, `posterizeTime()`, `smooth()`
- `Math.*` — natywny JS
- Wektory: `add()`, `sub()`, `mul()`, `length()`, `normalize()`
- Kolor: `rgbToHsl()`, `hslToRgb()`, `hexToRgb()`
- `loopIn()`, `loopOut()`
- `degreesToRadians()`, `radiansToDegrees()`
- `thisProject.*` — odpowiednik AE `thisComp.*` (width, height, fps, duration, frameDuration, pixelAspect, name, numTracks, displayStartTime, bgColor) + `thisProject.fullPath`
- `clip(index)` — odpowiednik AE `thisComp.layer(index)`, z tą różnicą że szuka na tym samym tracku (0-based, od lewej)
- `clip(thisClip, relIndex)` — odpowiednik AE `thisComp.layer(thisLayer, relIndex)`. Szuka na tym samym tracku, offset od pozycji referencyjnego klipu.
- `thisProperty` — obiekt z `.value`, `.numKeys`, `.wiggle()`, `.loopOut()`, `.smooth()`, `.valueAtTime()`, etc. Pełna kompatybilność z AE `thisProperty.*`.
- `timeToCurrentFormat()` — identyczna sygnatura z AE. Domyślnie używa `thisProject.fps` (nie 30fps).
- `marker.key(n).protectedRegion` — alias dla `.duration`, kompatybilny z AE.

### Co jest inne (świadomie)

| AE | Kdenlive Expressions | Powód |
|----|---------------------|-------|
| `thisComp.layer("Audio Amplitude").effect("Both Channels")("Slider")` | `audioLevel("Both", time)` | AE wymaga ręcznego "Convert Audio to Keyframes". U nas audio jest natywne. |
| `thisComp`, `thisLayer` | `thisProject`, `thisClip`, `thisTrack` | `thisComp` mapuje na `thisProject` (właściwości projektu). `thisLayer` mapuje na `thisClip`/`thisTrack`. Kdenlive ma klipy i tracki, nie kompozycje i warstwy. |
| `thisLayer` (alias) | `thisLayer` → `thisClip` | Globalny alias — `thisLayer` jest dostępne i wskazuje na ten sam obiekt co `thisClip`. Wyrażenia AE typu `thisLayer.startTime` działają bez zmian. |
| `thisComp` (alias) | `thisComp` → `thisProject` | Globalny alias — `thisComp` jest dostępne i wskazuje na ten sam obiekt co `thisProject`. Wyrażenia AE typu `thisComp.width` działają bez zmian. |
| `layer(index)` / `layer("name")` | `layer()` → `clip()` | Globalny alias — `layer()` deleguje do `clip()`. `layer(0)` = `clip(0)`, `layer("file.mp4")` = `clip("file.mp4")`. |
| `thisLayer.inPoint` | `thisClip.inPoint` | Zawsze `0.0` — Kdenlive aplikuje trim na poziomie timeline, więc clip-relative in-point jest zawsze 0. |
| `thisLayer.outPoint` | `thisClip.outPoint` | Clip-relative out-point w sekundach (= `clipDurationFrames / fps`). |
| `thisLayer.startTime` | `thisClip.startTime` | Pozycja klipu na timeline w sekundach. |
| `thisLayer.index` | `thisClip.index` | 0-based pozycja klipu na tracku (= global `index`). |
| `thisLayer.source` | `thisClip.source` | Sub-obiekt z metadanymi źródła: `.name`, `.width`, `.height`. |
| `thisLayer.hasVideo` | `thisClip.hasVideo` | Czy klip źródłowy ma strumień video. |
| `thisLayer.hasAudio` | `thisClip.hasAudio` | Czy klip źródłowy ma strumień audio. |
| `comp("name").layer(i)` | `clip("name")` / `clip(index)` | Uproszczona hierarchia (Kdenlive = 1 timeline, nie zagnieżdżone kompy). `clip(index)` szuka na tym samym tracku. |
| `toWorld()`, `fromComp()` | — (nie implementujemy) | Kdenlive nie ma przestrzeni 3D. |
| `Camera`, `Light` | — (nie implementujemy) | Kdenlive nie ma kamer/świateł 3D. |
| `sampleImage()` | Faza 3 | Kosztowne, odłożone. |

### Co NIE jest kompatybilne (więc trudniejsze do wykonania)

- **3D transforms** (`toWorld`, `fromWorld`, `toComp`, `fromComp` z Z) — Kdenlive jest 2D
- **Camera/Light objects** — brak w Kdenlive
- **Text Source** (`sourceText`, `text.style`) — Kdenlive ma inny model tytułów
- **Mask/Path expressions** — Kdenlive nie eksponuje masek jako obiektów expression
- **Footage/Source data** (`sourceData`, `dataValue`) — brak odpowiednika

---

## Plan implementacji

### Etap 1 — QuickJS + ewaluacja per-klatka

**Zmiany w forku Kdenlive (C++):**

1. Dodać QuickJS jako zależność (vendor jako submodule lub bundlowany .c/.h)
2. Klasa `ExpressionEngine` — wrapper C++ wokół QuickJS:
   - `void setContext(float time, int frame, float duration, float fps, float value)`
   - `float evaluate(const QString &expression)`
   - `void setAudioCache(const QVector<float> &levels)`
3. Zarejestrować globalne funkcje w QuickJS: `linear`, `ease`, `wiggle`, `clamp`, `audioLevel`, `audioRms`, `noise`, `random`, `posterizeTime`, `smooth`
4. W renderze: przed przekazaniem parametrów do MLT, sprawdzić czy parametr ma `expression`; jeśli tak, ewaluować i podstawić wynik

**Zmiany w .kdenlive XML:**
- Nowy atrybut `expression` na `<property>` — backward compatible (stare wersje ignorują)

### Etap 2 — audio cache

1. Przy otwarciu projektu / zmianie audio: wygenerować tablicę peak i RMS per klatka per kanał
2. Udostępnić jako C array dla `ExpressionEngine`
3. Funkcje `audioLevel()` i `audioRms()` czytają z cache (O(1) per lookup)

### Etap 3 — D-Bus + MCP

**Nowe metody D-Bus:**
- `scriptSetExpression(clipId, effectIndex, paramName, expression)` — ustaw wyrażenie
- `scriptGetExpression(clipId, effectIndex, paramName)` — odczytaj wyrażenie
- `scriptClearExpression(clipId, effectIndex, paramName)` — usuń wyrażenie

**Nowe MCP tools:**
- `set_expression(clip_id, effect_id, param_name, expression)` — agent ustawia wyrażenie
- `get_expression(clip_id, effect_id, param_name)` — agent odczytuje
- `clear_expression(clip_id, effect_id, param_name)` — agent usuwa

### Etap 4 — GUI (opcjonalnie)

- Pole tekstowe expression w panelu efektów (obok suwaka parametru)
- Podświetlanie składni JS
- Live preview (ewaluacja w trakcie pisania)
- Przycisk "Convert to Keyframes" — bake wyrażenia na keyframe'y (export do standardowego formatu)

---

## Przykłady użycia w produkcji MV

### Audio-reactive opacity (pulsowanie z basem)

```javascript
// Na klipie video — efekt brightness/opacity:
amp = audioRms("Both", time, 0.1);
ease(amp, 0.05, 0.7, 40, 100)
```

### Camera shake sterowany drop'em

```javascript
// Na efekcie transform/position:
amp = audioLevel("Both", time);
intensity = linear(amp, 0.3, 0.9, 0, 25);
wiggle(8, intensity)
```

### Narastająca desaturacja (sceny 5-12)

```javascript
// Na efekcie saturation:
// index = pozycja klipu na tracku (0-based)
linear(index, 4, 11, 100, 60)
```

### Blur fade-in na outro (sceny 33-38)

```javascript
// Na efekcie blur:
linear(time, 0, duration, 0, 15)
```

### Pulsujący halation na drop'ie

```javascript
// Na efekcie glow/bloom intensity:
amp = audioLevel("Both", time);
base = 20;
pulse = ease(amp, 0.2, 0.9, 0, 40);
base + pulse
```
