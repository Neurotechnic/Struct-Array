# Struct-Array
Umschulung Projekt in C zur universellen Speicherung strukturierter Daten 

## Überblick

Diese Engine ist ein generischer Kern für formularbasierte Datensatz‑Verarbeitung in C. Sie trennt **UI-Layout**, **Parsing/Formatting**, **Record‑Zugriff (über Bindings)**.  
Durch Austausch von `FormDef`, `FieldDef` und `FieldBinding` kann dieselbe Engine mit beliebigen Record‑Typen arbeiten (z. B. `Product`, `Kunde`, `Auftrag`, `Lagerposition`).


---

## Architekturüberblick

```
UI-Buffer  <-->  parse/format  <-->  Record-Speicher  <-->  save/load Datei
   ^                                                       ^
   |---------------------- DrawForm / DrawFooter ----------|
```

**Schichten**
- **UI/Layout**: Positionsberechnung für Labels, Felder, Button; Rendering in der Konsole.
- **Parsing/Formatting**: Konvertierung zwischen Textpuffern der Felder und nativen Datentypen (`int`, `double`, `char[]`) anhand von Masken.
- **Bindings**: Deklarative Kopplung eines UI‑Feldes an ein Record‑Feld per Byte‑Offset (`offsetof`).

---

## Kernstrukturen

### Feldtypen
```c
enum FieldType { FT_STR, FT_INT, FT_DBL };
```

### `struct FieldDef` – UI-Feldbeschreibung
```c
struct FieldDef {
    const char *Title;     // Label-Text
    enum FieldType Type;   // FT_STR / FT_INT / FT_DBL
    int MaxLen;            // sichtbare Zeichen ohne '\0'
    const char *Mask;      // z. B. "__________", "___.__"
    // Layout (gefüllt von CalculateFormLayout)
    int XLabel, XField;    // Bildschirmkoordinaten
    int Y;
    // Laufzeit
    char *Buffer;          // Textpuffer (initial = Mask)
    int CurIndex;          // Caret-Position
};
```

### `struct FormDef` – Formular-Container
```c
struct FormDef {
    const char *Title;
    struct FieldDef *Fields;
    int FieldCount;
    int Width, StartX, StartY, Height, RowGap;
    // Button
    char *btnLabel;
    int  btnX, btnY;
    // State
    int ActiveIndex;       // 0..FieldCount-1 Felder, ==FieldCount => Button
};
```

### `struct FieldBinding` – Kopplung UI ↔ Record
```c
struct FieldBinding {
    int fieldIndex;            // Index in FormDef.Fields
    size_t destOffset;         // Offset im Ziel-Record (offsetof(...))
    enum FieldType destType;   // FT_STR / FT_INT / FT_DBL
    int destSize;              // nur für Strings: Zielpuffergröße
};
```

**Vertrag**
- `destOffset` MUSS via `offsetof(RecordType, Feld)` ermittelt werden; keine Annahmen über Padding.
- Für `FT_STR` ist `destSize == sizeof(char-array im Record)`; beim Laden wird `dest[destSize-1]='\0'` gesetzt.
- Reihenfolge der `FieldBinding`‑Einträge definiert die Serialisierungsreihenfolge eines Records.

---

## Masken & Typkonvertierung

**Maskenregeln**
- `_` = Eingabeplatzhalter; `.` = Dezimaltrennzeichen; alle anderen Zeichen sind literal und bleiben stehen.
- `FT_INT`: Ziffern werden an `'_'`‑Positionen von links gefüllt.
- `FT_DBL`: Maske `___.__` definiert Ganz‑ und Nachkommastellen; Links wird mit führenden `0` aufgefüllt, rechts mit `0` ausgestellt.
- `FT_STR`: Nur Zeichen auf `'_'`‑Positionen werden aus dem Buffer übernommen; fehlende Zeichen bleiben `_`.

**Parser/Formatter‑API**
```c
int buffer_to_int   (const char *buf, const char *mask, int maxLen, int    *outVal);
int buffer_to_double(const char *buf, const char *mask, int maxLen, double *outVal);
int buffer_to_str   (const char *buf, const char *mask, int maxLen, char *outStr, int outSize);

int int_to_buffer   (int value,        const char *mask, int maxLen, char *outBuf);
int double_to_buffer(double value,     const char *mask, int maxLen, char *outBuf);
int str_to_buffer   (const char *s,    const char *mask, int maxLen, char *outBuf);
```

**Hinweise**
- Negative Zahlen sind in den Beispielparsern nicht unterstützt.

---

## Layout & Rendering

**Layoutberechnung**
```c
int CalculateFormLayout(struct FormDef *form);
int computePageSize(const struct FormDef *form);
int last_visible(int listTop, int pageSize, int catalogCount);
```

**Rendering**
```c
void DrawForm  (const struct FormDef *form);
void DrawHeader(struct FormDef *form);
void DrawFooter(const void *data, size_t itemSize,
                struct FormDef *form,
                const struct FieldBinding *binds, int bindCount,
                int index, int first, int last, int count);

void redraw(const struct FormDef *form,
            struct Product *catalog, int catalogCount,
            struct FieldBinding *prodBinds, int bindCount,
            int catalogIndex, int listTop, int pageSize);
```

**Rolle der Renderer**
- `DrawForm()` rendert Titel, Labels, Feldbuffer, aktives Feld (Cursorposition anhand `CurIndex`).
- `DrawFooter()` rendert eine tabellarische Listenansicht über die `FieldBinding`‑Spalten; die letzte „Zeile“ (`index == count`) repräsentiert den **<NEU HINZUFÜGEN>**‑Platzhalter.
- `redraw()` orchestriert Footer + Form; Pagination via `listTop/pageSize`.

---

## Record ↔ UI Transfer (Bindings)

```c
int save_record(void *rec,
                const struct FormDef *form,
                const struct FieldBinding *binds,
                int bindCount);

int load_record(const void *rec,
                struct FormDef *form,
                const struct FieldBinding *binds,
                int bindCount);
```

- **`save_record`**: Liest Feldpuffer, parst gemäß Feldtyp/Masken und schreibt in die Record‑Speicherfelder (`destOffset`/`destType`).
- **`load_record`**: Liest Record‑Felder, formatiert Werte gemäß Masken in die UI‑Buffer, setzt `CurIndex` auf erste `'_'`‑Position (oder Ende).

Beide Funktionen sind **schema‑agnostisch** (kennen den Record‑Typ nicht).

---

## Serialisierung (Binär, headerlos)

**Dateiformat**
- Datei besteht aus `N` Records hintereinander.
- Ein Record ist die konkatenierte Folge der Felder in **Binding‑Reihenfolge**.
- Feldformate:
  - `FT_STR`: exakt `destSize` Bytes; kürzere Strings Null‑Padding, längere hart abgeschnitten; keine `\0`‑Pflicht auf Platte.
  - `FT_INT`: `sizeof(int)` rohe Bytes.
  - `FT_DBL`: `sizeof(double)` rohe Bytes.

**API & Fehlercodes**
```c
int save_array_bin(const char *filename,
                   const void *items, size_t itemSize, size_t count,
                   const struct FieldBinding *binds, int bindCount);

int load_array_bin(const char *filename,
                   void **outItems, size_t itemSize, size_t *outCount,
                   const struct FieldBinding *binds, int bindCount);
/*
0 = Erfolg
1 = Datei konnte nicht geöffnet werden
2 = Schreib-/Lesefehler
3 = Trunkierter letzter Record (mitten im Record EOF)
4 = Speicherallokation fehlgeschlagen
*/
```

---

## Dynamischer Speicher / Datensatzarray

Katalog ist ein dynamisches Array von Records (z. B. `struct Product`). Wachstum in Schritten (`CATALOG_CHUNK = 5`).

```c
int ensure_catalog_capacity(struct Product **pcat, int *pcap, int index);
```

**Lebenszyklus bei „Laden“**
- Alter Buffer wird freigegeben.
- `load_array_bin()` liefert neuen `malloc`‑Puffer + Count.

---

## Beispiel: Neues Datenschema integrieren

```c
struct Kunde {
    char   Name[21];
    int    Kundennr;
    double Guthaben;
};

struct FieldDef kundeFields[] = {
    { "Name: ",     FT_STR, 20, "____________________", 0,0,0, NULL,0 },
    { "Nr: ",       FT_INT, 10, "__________",          0,0,0, NULL,0 },
    { "Guthaben: ", FT_DBL,  6, "___.__",              0,0,0, NULL,0 }
};

struct FieldBinding kundeBinds[] = {
  {0, offsetof(struct Kunde, Name),     FT_STR, sizeof(((struct Kunde*)0)->Name)},
  {1, offsetof(struct Kunde, Kundennr), FT_INT, 0},
  {2, offsetof(struct Kunde, Guthaben), FT_DBL, 0},
};

// Danach funktionieren save_record / load_record / DrawFooter /
// save_array_bin / load_array_bin unverändert.
```

---

## Eingabeschleife & Zustände (Konsole)

**High‑Level**
- Navigation über Felder/Buttons via `ActiveIndex`.
- Zeichenverarbeitung abhängig vom Feldtyp (`FT_STR`, `FT_INT`, `FT_DBL`), inkl. Maskenlogik (Dezimalpunkt‑Handling).
- Listen‑Navigation (↑/↓, Home/Ende), Pagination (`listTop/pageSize`), Insert‑Zeile (am Ende), Umschalten Button‑Label (Hinzufügen/Aktualisieren).

> Dies ist Implementierungsdetail des Demos; der Engine‑Kern (Layout, Bindings, Parser, IO) bleibt davon unabhängig.

---

## Tests

**Masken‑Unit‑Tests**
```c
int run_mask_tests(void);
```
- Prüft `buffer_to_*` und `*_to_buffer` (inkl. Überläufe/Leerwerte).

---

## Invarianten & Designregeln (Kurzfassung)

* Bindings sind Single‑Source‑of‑Truth für: Feldreihenfolge, Typinterpretation und Serialisierung.
* `offsetof` ist Pflicht für `destOffset` (kein manuelles Rechnen, kein Casting‑Undefined‑Behavior).
* UI‑Masken definieren sowohl Darstellung als auch erlaubte Eingabeplätze.
* Serialisierung bleibt headerlos & minimal; Robustheit über vollständiges Feld‑lesen/schreiben pro Record.
* Fehlerfälle werden über Rückgabecodes propagiert (keine `errno`‑Kopplung).

---

## Erweiterungspunkte

* **Negativzahlen** und Vorzeichen‑Parsing/Formatting.
* **Masken‑Validator** (Compile‑Time/Init‑Check gegen Typ/MaxLen).
* **Neue Feldtypen**.

---