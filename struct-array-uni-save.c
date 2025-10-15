#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <conio.h>      
#include <windows.h>  
#include <string.h>
#include <time.h>
#include "myfunc.h"

// ANSI-Farbsequenzen
#define COLOR_RESET   "\033[0m"
#define COLOR_GRAY    "\033[90m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_YELLOW_I  "\033[30;43m"
#define COLOR_YELLOWB  "\033[93m"
#define COLOR_MAGENTAB  "\033[95m"
#define COLOR_BLUE  "\033[34m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_GREEN_I   "\033[30;42m"
#define COLOR_RED     "\033[31m"

#define CATALOG_CHUNK 5

enum FieldType {
  FT_STR,
  FT_INT,
  FT_DBL
};

struct Product {
    char Artikel[11];
    int Artikelnum;
    char Kategorie[11];
    double Preis;
};

struct FieldDef {
    const char *Title;     // label text
    enum FieldType Type;
    int MaxLen;            // buffer len without '\0'
    const char *Mask;      // visual mask

    // layout, filled by layout calc
    int XLabel, XField;    // screen coords
    int Y;

    char *Buffer;          // text buffer (init with Mask)
    int CurIndex;          // caret index (for future input)
};

struct FormDef {
    const char *Title;             // form title
    struct FieldDef *Fields;       // pointer to field array
    int FieldCount;                // number of fields

    int Width;                     // total frame width (fixed)
    int StartX, StartY;            // frame top-left position
    int Height;                    // computed
    int RowGap;                    // vertical spacing between rows

    // button parameters
    char *btnLabel;                // e.g. "<Insert>" or "<Update>"
    int  btnX, btnY;               // position on screen

    // runtime state
    int ActiveIndex;               // 0..FieldCount for fields, ==FieldCount means button
};

struct FieldBinding {
    int fieldIndex;
    size_t destOffset;
    enum FieldType destType;
    int destSize; //nur fuer strings
};

char currentFilename[_MAX_PATH]="\0";

int buffer_to_int(const char *buf, const char *mask, int maxLen, int *outVal);
int buffer_to_double(const char *buf, const char *mask, int maxLen, double *outVal);
int buffer_to_str(const char *buf, const char *mask, int maxLen, char *outStr, int outSize);


//Rechnen die Abmessungen der Elemente aus
int CalculateFormLayout(struct FormDef *form) {
    const int padLR = 2; //Padding
    const int colGap = 3; //Gap between columns
    const int contentX0 = form->StartX + 1;
    const int contentY0 = form->StartY + 1;

    int innerW = form->Width - 2; 

    const int lineLeft  = contentX0 + padLR;
    const int lineRight = contentX0 + innerW - padLR;
    const int usableW   = lineRight - lineLeft + 1; //Usable width
    if (usableW < 3) return 0; //Fehler: zu klein

    int curX = lineLeft; // aktuelle x-position in der zeile (start am linken rand)
    int curY = contentY0 + 2; //// aktuelle y-position der felder; +2 wegen titelzeile und leerzeile
    int maxY = curY; //bisher groesste y-position; dient zur berechnung der gesamthoehe

    // layout fields
    for (int i = 0; i < form->FieldCount; ++i) {
        int wantLabel = (int)strlen(form->Fields[i].Title);
        int wantField = (int)strlen(form->Fields[i].Mask);
        if (wantLabel < 1) wantLabel = 1;
        if (wantField < 1) wantField = 1;

        int wantBlock = wantLabel + 1 + wantField;

        // Wenn nicht einmal in eine leere Zeile passt -> Fehler
        if (wantBlock > usableW)
            return 0;

        int lead = (curX == lineLeft) ? 0 : colGap;
        if (curX + lead + wantBlock - 1 > lineRight) {
            curX = lineLeft;
            curY += form->RowGap;
            lead = 0;
        }

        int x = curX + lead;
        form->Fields[i].XLabel = x;
        form->Fields[i].Y = curY;
        form->Fields[i].XField = x + wantLabel + 1;

        curX = form->Fields[i].XField + wantField;
        if (curY > maxY) maxY = curY;
    }

    // layout button (if exists)
    if (form->btnLabel && form->btnLabel[0]) {
        int btnLen = (int)strlen(form->btnLabel);
        int lead = (curX == lineLeft) ? 0 : colGap;

        // if button does not fit on same line, wrap
        if (curX + lead + btnLen - 1 > lineRight) {
            curX = lineLeft;
            curY += form->RowGap;
            lead = 0;
        }

        // if cannot fit even on empty line -> error
        if (btnLen > usableW)
            return 0;

        form->btnX = curX + lead;
        form->btnY = curY;
        curX = form->btnX + btnLen;
        if (curY > maxY) maxY = curY;
    }

    // height (include last row)
    form->Height = (maxY - contentY0) + 2 + 2;
    return 1;
}

//Fuer Zeilen
int computePageSize(const struct FormDef *form){
    short cw, ch;   //console Width/Height
    getConsoleSize(&cw, &ch);
    int firstRow = form->Height + 4;
    int lastRow  = ch - 4;
    int n = lastRow - firstRow + 1;
    if (n < 1) n = 1;
    return n;
}

int last_visible(int listTop, int pageSize, int catalogCount){
    int last = listTop + pageSize - 1;
    int maxIdx = catalogCount;
    if (last > maxIdx) last = maxIdx;
    return last; 
}

// ==== draw ====
void DrawForm(const struct FormDef *form) {
    // rahmen
    rahmen_pg(form->Width, form->Height, form->StartX, form->StartY, 1);

    // basis-koordinaten
    const int padLR = 2;
    const int contentX0 = form->StartX + 1;
    const int contentY0 = form->StartY + 1;

    // titel
    gotoxy(contentX0 + padLR, contentY0);
    printf(COLOR_CYAN "%s" COLOR_RESET, form->Title);

    // felder
    for (int i = 0; i < form->FieldCount; ++i) {
        const struct FieldDef *f = &form->Fields[i];

        // label (aktiv -> invertiertes gruen)
        gotoxy(f->XLabel, f->Y);
        if (form->ActiveIndex == i)
            printf(COLOR_GREEN_I "%s" COLOR_RESET, f->Title);
        else
            printf(COLOR_GREEN   "%s" COLOR_RESET, f->Title);

        // ein leerzeichen zwischen label und feld
        gotoxy(f->XField - 1, f->Y);
        printf(" ");

        // feldpuffer
        gotoxy(f->XField, f->Y);
        printf(COLOR_WHITE "%s" COLOR_RESET, f->Buffer);
    }

    // taste (button)
    if (form->btnLabel && form->btnLabel[0]) {
        gotoxy(form->btnX, form->btnY);
        if (form->ActiveIndex == form->FieldCount)
            printf(COLOR_YELLOW_I "%s" COLOR_RESET, form->btnLabel);
        else
            printf(COLOR_YELLOW   "%s" COLOR_RESET, form->btnLabel);
    }

    // cursor in aktives feld setzen (falls aktiv ein feld ist)
    if (form->ActiveIndex >= 0 && form->ActiveIndex < form->FieldCount) {
        const struct FieldDef *af = &form->Fields[form->ActiveIndex];
        // sichtbare feldbreite = min(MaxLen, strlen(Mask))
        int fieldW = min2(af->MaxLen, (int)strlen(af->Mask));
        // clamp fuer curIndex
        int caret = af->CurIndex;
        if (caret < 0) caret = 0;  //fuer Sicherheit
        if (caret > fieldW) caret = fieldW; //fuer Sicherheit
        // cursor positionieren
        gotoxy(af->XField + caret, af->Y);
    }
}

// hilfe: anzahl nachkommastellen fuer double aus maske
static int frac_precision(const char *mask){
    int dot=-1, i=0, frac=0;
    for (; mask && mask[i]; ++i){ if (mask[i]=='.'){ dot=i; break; } }
    if (dot<0) return 0;
    for (i=dot+1; mask[i]; ++i) if (mask[i]=='_') ++frac;
    return frac;
}

void DrawHeader(struct FormDef *form) {
     gotoxy(form->StartX,0);
     if (currentFilename[0]) printf("Aktuelle Datei: " COLOR_MAGENTAB "%s" COLOR_RESET, currentFilename);
     else printf("Aktuelle Datei: " COLOR_MAGENTAB "<NEU>" COLOR_RESET, currentFilename);
     printf(" | Hilfe: " COLOR_MAGENTAB " F1" COLOR_RESET " | Exit: " COLOR_MAGENTAB "Ctrl-C" COLOR_RESET);
}

// universeller footer-druck fuer datensaetze [first..last]
void DrawFooter(const void *data, size_t itemSize,
                struct FormDef *form,
                const struct FieldBinding *binds, int bindCount,
                int ind, int first, int last, int count)
{
    int totalWidth=2;
    // kopfzeile
    gotoxy(form->StartX+1, form->Height + 2);
    printf(" #");
    for (int b=0; b<bindCount; b++){
        const struct FieldDef *f = &form->Fields[binds[b].fieldIndex];
        int w = f->MaxLen;
        const char *t = f->Title;
        int tl = (int)strlen(t); //Title lenght
        
        // von hinten alle Leerzeichen und Doppelpunkte entfernen
        while (tl > 0 && (t[tl - 1] == ':' || t[tl - 1] == ' ')) {
              tl--;
        }
        printf(" | %*.*s", w, tl, t); // rechtsbuendig
        totalWidth+=w+3;
    }
    gotoxy(form->StartX+1, form->Height + 3);
    // trennlinie
    for (int k=0; k<totalWidth; k++) putchar('=');
    if (data != NULL && first <= last) {
        // zeilen
        int y = form->Height + 4;
        for (int i=first; i<=last; i++){
          if (i==last && last==count) {
                printf("\n    ");
                if (ind==last) {
                   const char *clr = COLOR_YELLOW_I;
                   printf("%s", clr);
                   form->btnLabel=" <HINZUFUEGEN> ";
                } else {
                   const char *clr = COLOR_RESET;
                   printf("%s", clr);
                   form->btnLabel="<AKTUALISIEREN>";
                }                
                int lPad=(totalWidth-15)/2;
                int rPad=totalWidth-15-lPad;
                for (int j=1; j<totalWidth; j++) {
                    if (j<=lPad) putchar('=');
                    else if (j>=totalWidth-rPad) putchar('=');
                    else if (j==lPad+1) printf("NEU HINZUFUEGEN");
                }
                printf(COLOR_RESET);
          } else {
            const char *row = (const char*)data + (size_t)i * itemSize;
    
            gotoxy(form->StartX+1, y + (i-first));
            // farbe: aktive zeile invertiert
            const char *clr = (i==ind) ? COLOR_YELLOW_I : COLOR_RESET;
            printf("%s", clr);
    
            // indexspalte
            printf("%2d", i);
    
            // spalten aus binds lesen und formatiert ausgeben
            for (int b=0; b<bindCount; b++){
                const struct FieldBinding *bd = &binds[b];
                const struct FieldDef *f = &form->Fields[bd->fieldIndex];
                const char *base = row + bd->destOffset;
                int w = f->MaxLen;
    
                printf(" | ");
                if (bd->destType == FT_STR){
                    // strings: wie im beispiel rechtsbuendig; fuer linksbuendig: "%-*s"
                    printf("%*s", w, (const char*)base);
                } else if (bd->destType == FT_INT){
                    printf("%*d", w, *(const int*)base);
                } else if (bd->destType == FT_DBL){
                    int prec = frac_precision(f->Mask);
                    char fmt[32];
                    // z.B. "%6.2f"
                    snprintf(fmt, sizeof(fmt), "%%%d.%df", w, prec);
                    printf(fmt, *(const double*)base);
                }
            }
            printf(COLOR_RESET);
          }
        }
    }

}

void DrawFiltered(const void *data, size_t itemSize, int recordCount,
                  struct FormDef *form, const struct FieldBinding *binds, int bindCount)
{
    // totalWidth: Breite der Tabelle fuer Kopf- und Trennzeilen
    int totalWidth = 2;

    system("cls");
    printf(COLOR_CYAN "FILTERERGEBNISSE\n\n" COLOR_RESET);

    // kopfzeile
    printf(" #");
    for (int b=0; b<bindCount; b++){
        const struct FieldDef *f = &form->Fields[binds[b].fieldIndex];
        int w = f->MaxLen;
        const char *t = f->Title;
        int tl = (int)strlen(t); //Title lenght
        
        // von hinten alle Leerzeichen und Doppelpunkte entfernen
        while (tl > 0 && (t[tl - 1] == ':' || t[tl - 1] == ' ')) {
              tl--;
        }
        printf(" | %*.*s", w, tl, t); // rechtsbuendig
        totalWidth+=w+3;
    }
    printf("\n");

    // --- Trennlinie ---
    for (int k = 0; k < totalWidth; k++) putchar('=');
    printf("\n");
    
    int hitCount = 0; // Anzahl der gedruckten Treffer

    // alle Datensaetze pruefen
    for (int i = 0; i < recordCount; i++) {
        // recordPtr: Zeiger auf aktuellen Datensatz
        const char *recordPtr = (const char *)data + (size_t)i * itemSize;

        // rowMatches: bleibt 1, solange alle aktiven Filter passen
        int rowMatches = 1;

        // alle Bindings als potentielle Filter interpretieren
        for (int b = 0; b < bindCount && rowMatches; b++) {
            const struct FieldBinding *fieldBinding = &binds[b];
            const struct FieldDef *fieldDef = &form->Fields[fieldBinding->fieldIndex];

            // dataPtr: Feldinhalt im Datensatz (via Offset)
            const char *dataPtr = recordPtr + fieldBinding->destOffset;

            // filterRaw: Benutzer-Eingabe fuer dieses Feld (roh, gemaess Maske extrahiert)
            char filterRaw[256];
            if (!buffer_to_str(fieldDef->Buffer, fieldDef->Mask, fieldDef->MaxLen, filterRaw, (int)sizeof(filterRaw))) {
                // Parsing fehlgeschlagen -> kein Filter fuer dieses Feld anwenden
                continue;
            }
            if (filterRaw[0] == '\0') {
                // leeres Filterfeld -> kein Filter fuer dieses Feld
                continue;
            }

            if (fieldBinding->destType == FT_STR) {
                // String: Teilstring, case-insensitive
                char recordStr[512];
                char filterStr[512];

                // Feldwert aus Datensatz
                strncpy(recordStr, (const char *)dataPtr, sizeof(recordStr) - 1);
                recordStr[sizeof(recordStr) - 1] = '\0';

                // Filtertext aus Eingabe
                strncpy(filterStr, filterRaw, sizeof(filterStr) - 1);
                filterStr[sizeof(filterStr) - 1] = '\0';

                // beide Seiten klein schreiben
                strlwr(recordStr);
                strlwr(filterStr);

                if (!strstr(recordStr, filterStr)) {
                    rowMatches = 0; // Teilstring nicht enthalten
                }

            } else if (fieldBinding->destType == FT_INT) {
                // Integer: exakte Gleichheit
                int filterInt = 0;
                if (!buffer_to_int(fieldDef->Buffer, fieldDef->Mask, fieldDef->MaxLen, &filterInt)) {
                    rowMatches = 0; // ungueltiger Filter
                    break;
                }
                //int dataInt = *(const int *)dataPtr;
                int dataInt = 0;
                memcpy(&dataInt, dataPtr, sizeof dataInt);
                if (dataInt != filterInt) {
                    rowMatches = 0;
                }

            } else if (fieldBinding->destType == FT_DBL) {
                // Double: Gleichheit nach Masken-Praezision (runden/skalieren)
                double filterDouble = 0.0;
                if (!buffer_to_double(fieldDef->Buffer, fieldDef->Mask, fieldDef->MaxLen, &filterDouble)) {
                    rowMatches = 0; // ungueltiger Filter
                    break;
                }
                //double dataDouble = *(const double *)dataPtr;
                double dataDouble = 0.0;
                memcpy(&dataDouble, dataPtr, sizeof dataDouble);

                int precision = frac_precision(fieldDef->Mask); // Anzahl Nachkommastellen
                double scale = 1.0;
                for (int s = 0; s < precision; ++s) scale *= 10.0;

                long long roundedFilter = (long long)(filterDouble * scale + (filterDouble >= 0 ? 0.5 : -0.5));
                long long roundedData   = (long long)(dataDouble   * scale + (dataDouble   >= 0 ? 0.5 : -0.5));

                if (roundedData != roundedFilter) {
                    rowMatches = 0;
                }
            }
        }

        if (!rowMatches) continue;

        // --- Zeile drucken, wenn Filter passen ---
        printf("%2d", i);
        for (int b = 0; b < bindCount; ++b) {
            const struct FieldBinding *fieldBinding = &binds[b];
            const struct FieldDef *fieldDef = &form->Fields[fieldBinding->fieldIndex];
            const char *dataPtr = recordPtr + fieldBinding->destOffset;

            int columnWidth = fieldDef->MaxLen;
            printf(" | ");

            if (fieldBinding->destType == FT_STR) {
                printf("%*s", columnWidth, (const char *)dataPtr);
            } else if (fieldBinding->destType == FT_INT) {
                printf("%*d", columnWidth, *(const int *)dataPtr);
            } else if (fieldBinding->destType == FT_DBL) {
                int precision = frac_precision(fieldDef->Mask);
                char fmt[32];
                snprintf(fmt, sizeof(fmt), "%%%d.%df", columnWidth, precision);
                printf(fmt, *(const double *)dataPtr);
            }
        }
        printf("\n");
        hitCount++;
    }

    // Abschlusslinie und Zusammenfassung
    for (int k = 0; k < totalWidth; k++) putchar('-');
    printf("\n");

    if (hitCount == 0) {
        printf("Keine Ergebnisse gefunden.\n");
    } else {
        printf("%d Treffer gefunden.\n", hitCount);
    }

    printf("\nDruecken Sie eine Taste, um zurueckzukehren...");
    getch();
    system("cls");
}


void redraw(const struct FormDef *form,
            struct Product *catalog, int catalogCount,
            struct FieldBinding *prodBinds, int fieldCount,
            int catalogIndex, int listTop, int pageSize)
{
    int last  = last_visible(listTop, pageSize, catalogCount); // <= допускает count
    int first = listTop;
    if (first > last) first = last; // на всякий случай прижать

    DrawFooter(catalog, sizeof(struct Product), (struct FormDef*)form,
               prodBinds, fieldCount,
               catalogIndex, first, last, catalogCount);
    DrawForm(form);
}

void DrawHelp(void) {
     system("cls");
     printf(COLOR_MAGENTAB "Tab/Enter" COLOR_RESET ": Naechstes Feld aktivieren.");
     printf(COLOR_MAGENTAB "\nEnter" COLOR_RESET " auf der Schaltflache: Hinzufugen/Aktualisieren von Eintragen.");
     printf(COLOR_MAGENTAB "\nPfeiltasten ^/v" COLOR_RESET ": Zwischen Datensatzen wechseln.");
     printf(COLOR_MAGENTAB "\nPos1 (Home)" COLOR_RESET ": Zum ersten Eintrag springen.");
     printf(COLOR_MAGENTAB "\nEnde" COLOR_RESET ": Zum letzten Eintrag springen.");
     printf(COLOR_MAGENTAB "\nCtrl+F" COLOR_RESET ": Suche mit Ausgabe basierend auf den Daten in den Eingabefeldern.");
     printf(COLOR_MAGENTAB "\nCtrl+S" COLOR_RESET ": Array in Datei speichern (default.db).");
     printf(COLOR_MAGENTAB "\nCtrl+L" COLOR_RESET ": Array aus Datei laden (default.db).");
     printf(COLOR_GRAY "\nCtrl+N: Neues Array erstellen (noch nicht implementiert)." COLOR_RESET);
     printf(COLOR_GRAY "\nCtrl+D: Loschen des aktiven Eintrags (noch nicht implementiert)." COLOR_RESET);
     printf(COLOR_MAGENTAB "\nCtrl+C" COLOR_RESET ": Programm beenden.");
     printf("\n\nDruecken Sie eine beliebige Taste, um fortzufahren...");
     getch();
}

///// PARSING

// === parse: buffer -> int ===
// regel: sammle ziffern an '_' positionen von links, unterstriche ignorieren
int buffer_to_int(const char *buf, const char *mask, int maxLen, int *outVal) {
    int nMask = (int)strlen(mask);
    int nBuf  = (int)strlen(buf);
    int n     = min2(maxLen, min2(nMask, nBuf));

    long v = 0;
    int any = 0;
    for (int i=0; i<n; ++i) {
        if (mask[i] == '_') {
            char c = buf[i];
            if (c >= '0' && c <= '9') {
                v = v * 10 + (c - '0');
                any = 1;
            }
        }
    }
    if (!any) {                 // leer -> gilt als 0 und OK
        if (outVal) *outVal = 0;
        return 1;
    }
    if (outVal) *outVal = (int)v;
    return 1;
}

// === parse: buffer -> double ===
// regel: maske enthaelt eine '.' position; ziffern an '_' links davon -> ganzteil,
// rechts davon -> fraction; unterstriche werden ignoriert
int buffer_to_double(const char *buf, const char *mask, int maxLen, double *outVal) {
    int nMask = (int)strlen(mask);
    int nBuf  = (int)strlen(buf);
    int n     = min2(maxLen, min2(nMask, nBuf));

    int dotSeen = 0;
    long ip = 0;          // ganzteil
    long fp = 0;          // fraction ohne punkt
    int  fracLen = 0;     // anzahl fraction-digits
    int  any = 0;

    for (int i=0; i<n; ++i) {
        char m = mask[i];
        char b = buf[i];
        if (m == '.') { dotSeen = 1; continue; }
        if (m == '_' && b >= '0' && b <= '9') {
            any = 1;
            if (!dotSeen) ip = ip * 10 + (b - '0');
            else { fp = fp * 10 + (b - '0'); ++fracLen; }
        }
    }

    if (!any) {                 // leer -> gilt als 0.0 und OK
        if (outVal) *outVal = 0.0;
        return 1;
    }

    double scale = 1.0;
    for (int k=0; k<fracLen; ++k) scale *= 0.1;
    if (outVal) *outVal = (double)ip + (double)fp * scale;
    return 1;
}

// === parse: buffer -> string (trim trailing '_') ===
// regel: nimm nur zeichen aus buf an '_' positionen der maske; schneide end-unterstriche ab
int buffer_to_str(const char *buf, const char *mask, int maxLen, char *outStr, int outSize) {
    if (outSize <= 0) return 0;
    int nMask = (int)strlen(mask);
    int nBuf  = (int)strlen(buf);
    int n     = min2(maxLen, min2(nMask, nBuf));

    int k = 0;
    for (int i=0; i<n && k < outSize-1; ++i) {
        if (mask[i] == '_') {
            char c = buf[i];
            if (c != '_' && c != '\0') {
                outStr[k++] = c;
            }
        }
    }
    outStr[k] = '\0';
    return 1;
}

// === format: int -> buffer ===
// regel: schreibe ziffern links->rechts in '_' der maske; rest bleibt '_' ; zu viele ziffern -> fehler
int int_to_buffer(int value, const char *mask, int maxLen, char *outBuf) {
    char num[64];
    if (value < 0) return 0; // negativ nicht gefordert
    snprintf(num, sizeof(num), "%d", value);

    int mLen = min2((int)strlen(mask), maxLen);
    for (int i=0; i<mLen; ++i) outBuf[i] = mask[i];
    outBuf[mLen] = '\0';

    int j = 0, nd = (int)strlen(num);
    for (int i=0; i<mLen && j<nd; ++i) {
        if (mask[i] == '_') outBuf[i] = num[j++];
    }
    // passt nicht in die anzahl der '_' ?
    if (j < nd) return 0;
    return 1;
}

// === format: double -> buffer ===
// regel: links vom '.' anzahl '_' = intSlots -> fuehre integer mit fuehrenden '0' auf diese breite
// rechts vom '.' anzahl '_' = fracSlots -> precision und fuelle mit '0' rechts
int double_to_buffer(double value, const char *mask, int maxLen, char *outBuf) {
    if (value < 0.0) return 0; // negativ nicht gefordert

    int mLen = min2((int)strlen(mask), maxLen);

    // finde '.' und slots
    int dotPos = -1, intSlots = 0, fracSlots = 0;
    for (int i=0; i<mLen; ++i) { if (mask[i]=='.') { dotPos = i; break; } }
    if (dotPos >= 0) {
        for (int i=0; i<dotPos; ++i) if (mask[i]=='_') ++intSlots;
        for (int i=dotPos+1; i<mLen; ++i) if (mask[i]=='_') ++fracSlots;
    } else {
        // keine '.' in maske: behandle wie int maske
        return int_to_buffer((int)(value + 0.5), mask, maxLen, outBuf);
    }

    // baue string mit genau fracSlots nachkommastellen
    char fmt[16], num[128];
    snprintf(fmt, sizeof(fmt), "%%.%df", fracSlots);
    snprintf(num, sizeof(num), fmt, value); // z.B. "15.50"

    // separiere integer und fraction
    char *pDot = strchr(num, '.');
    char ipStr[64] = {0}, fpStr[64] = {0};
    if (pDot) {
        size_t il = (size_t)(pDot - num);
        if (il >= sizeof(ipStr)) il = sizeof(ipStr)-1;
        memcpy(ipStr, num, il); ipStr[il] = '\0';
        strncpy(fpStr, pDot+1, sizeof(fpStr)-1);
    } else {
        strncpy(ipStr, num, sizeof(ipStr)-1);
        fpStr[0] = '\0';
    }

    // pruefe integer-breite und pad mit '0' links
    int ipLen = (int)strlen(ipStr);
    if (ipLen > intSlots) return 0;
    char ipPad[64];
    int pad = intSlots - ipLen;
    for (int i=0; i<pad; ++i) ipPad[i] = '0';
    memcpy(ipPad + pad, ipStr, (size_t)ipLen);
    ipPad[intSlots] = '\0';

    // fraction exakt fracSlots zeichen (snprintf hat das bereits)
    // sicherheit: falls kuerzer, fuelle rechts
    int fpLen = (int)strlen(fpStr);
    char fpPad[64];
    if (fpLen < fracSlots) {
        memcpy(fpPad, fpStr, (size_t)fpLen);
        for (int i=fpLen; i<fracSlots; ++i) fpPad[i] = '0';
        fpPad[fracSlots] = '\0';
    } else {
        // clip falls laenger (sollte nicht passieren)
        memcpy(fpPad, fpStr, (size_t)fracSlots);
        fpPad[fracSlots] = '\0';
    }

    // aufbau im outBuf gemaess maske
    for (int i=0; i<mLen; ++i) outBuf[i] = mask[i];
    outBuf[mLen] = '\0';

    // links vom punkt: schreibe ipPad in '_' von links
    int j = 0;
    for (int i=0; i<dotPos && j<intSlots; ++i) {
        if (mask[i]=='_') outBuf[i] = ipPad[j++];
    }
    // punkt bleibt wie in maske

    // rechts vom punkt: schreibe fpPad in '_' von links
    j = 0;
    for (int i=dotPos+1; i<mLen && j<fracSlots; ++i) {
        if (mask[i]=='_') outBuf[i] = fpPad[j++];
    }
    return 1;
}

// === format: string -> buffer ===
// regel: schreibe s in '_' von links; rest '_' bleibt; literalzeichen der maske bleiben
int str_to_buffer(const char *s, const char *mask, int maxLen, char *outBuf) {
    int mLen = min2((int)strlen(mask), maxLen);
    for (int i=0; i<mLen; ++i) outBuf[i] = mask[i];
    outBuf[mLen] = '\0';

    int j = 0, ns = (int)strlen(s);
    for (int i=0; i<mLen && j<ns; ++i) {
        if (mask[i] == '_') outBuf[i] = s[j++];
    }
    return 1;
}

// sorgt dafuer, dass fuer index eine Kapazitaet vorhanden ist
int ensure_catalog_capacity(struct Product **pcat, int *pcap, int index){
    if (index < *pcap) return 1;
    int newCap = *pcap;
    while (newCap < (index + 1)) newCap += CATALOG_CHUNK; // wachsen in CATALOG_CHUNK Schritten
    void *newBuf = realloc(*pcat, (size_t)newCap * sizeof(struct Product));
    if (!newBuf) return 0; //Fehler bei memory allocation
    *pcat = (struct Product*)newBuf;
    *pcap = newCap;
    return 1;
}

// Speichert Daten aus den Eingabefeldern (Form) in eine Struktur (rec)
int save_record(void *rec, const struct FormDef *form, const struct FieldBinding *binds, int bindCount) {

    // Schleife ueber alle Feldbindungen
    for (int i = 0; i < bindCount; ++i) {
        const struct FieldBinding *b = &binds[i];              // aktuelle Bindung
        const struct FieldDef *f = &form->Fields[b->fieldIndex]; // passendes Eingabefeld
        char *base = (char*)rec + b->destOffset;               // Zieladresse im Datensatz

        if (b->destType == FT_STR) { // String
            if (!buffer_to_str(f->Buffer, f->Mask, f->MaxLen, (char*)base, b->destSize))
                return 0; // Fehler beim Konvertieren
        }
        else if (b->destType == FT_INT) { // Ganzzahl
            int v = 0;
            if (!buffer_to_int(f->Buffer, f->Mask, f->MaxLen, &v))
                return 0;
            *(int*)base = v; // Wert in Struktur schreiben
        }
        else if (b->destType == FT_DBL) { // Gleitkommazahl
            double d = 0;
            if (!buffer_to_double(f->Buffer, f->Mask, f->MaxLen, &d))
                return 0;
            *(double*)base = d; // Wert in Struktur schreiben
        }
    }
    return 1; // alles OK
}

// hilfe: finde erste Position des Platzhalters '_' im Buffer
static int first_placeholder(const char *buf, int maxLen) {
    for (int i = 0; i < maxLen; ++i)
        if (buf[i] == '_') return i;
    return maxLen; // kein '_' gefunden -> ans Ende
}

int load_record(const void *rec, struct FormDef *form,
                const struct FieldBinding *binds, int bindCount)
{
    for (int i = 0; i < bindCount; ++i) {
        const struct FieldBinding *b = &binds[i];
        struct FieldDef *f = &form->Fields[b->fieldIndex];
        const char *base = (const char*)rec + b->destOffset;

        if (b->destType == FT_STR) {
            str_to_buffer((const char*)base, f->Mask, f->MaxLen, f->Buffer);
        } else if (b->destType == FT_INT) {
            int value = *(const int*)base;
            int_to_buffer(value, f->Mask, f->MaxLen, f->Buffer);
        } else if (b->destType == FT_DBL) {
            double dvalue = *(const double*)base;
            double_to_buffer(dvalue, f->Mask, f->MaxLen, f->Buffer);
        }

        // caret setzen: erste Luecke ('_'), sonst ans Ende
        f->CurIndex = first_placeholder(f->Buffer, f->MaxLen);
    }
    return 1;
}

// Universelles, binarkompatibles Format ohne Header.
// Die Datei enthaelt einfach N Datensaetze hintereinander.
// Jeder Datensatz ist die Aneinanderreihung der Felder gemaess FieldBindings.
//
// Rueckgabecodes:
//   0 = Erfolg
//   1 = Datei konnte nicht geoeffnet werden
//   2 = Schreib-/Lesefehler
//   3 = Trunkierte letzte Datei-Record (unvollstaendig gelesen)
//   4 = Speicherallokation fehlgeschlagen
//
// Erwartete Felder in struct FieldBinding:
//   size_t destOffset;   // Byte-Offset im Record
//   int    destType;     // z. B. FT_STR, FT_INT, FT_DBL
//   size_t strMaxLen;    // fuer FT_STR: max. Zeichenanzahl ohne '\0' (hier: destSize)
//   size_t binSize;      // fuer FT_BIN: feste Laenge in Bytes (optional, hier nicht genutzt)

int save_array_bin(
    const char *filename,
    const void *items,           // Zeiger auf erstes Element des Arrays
    size_t itemSize,             // sizeof(Record)
    size_t count,                // Anzahl Elemente im Array
    const struct FieldBinding *binds,
    int bindCount
){
    FILE *fp = fopen(filename, "wb");
    if (!fp) return 1;

    for (size_t idx = 0; idx < count; ++idx) {
        const char *rec = (const char*)items + idx * itemSize;

        for (int b = 0; b < bindCount; ++b) {
            const struct FieldBinding *bd = &binds[b];
            const char *base = rec + bd->destOffset;

            if (bd->destType == FT_STR) {
                // Schreibe exakt destSize Bytes:
                // kuerzere Strings werden mit 0-Bytes aufgefuellt,
                // laengere werden hart abgeschnitten.
                size_t n = (size_t)bd->destSize;
                for (size_t i = 0; i < n; ++i) {
                    unsigned char ch = 0;
                    if (((const char*)base)[i] != '\0') ch = (unsigned char)((const char*)base)[i];
                    if (fwrite(&ch, 1, 1, fp) != 1) { fclose(fp); return 2; }
                    if (ch == 0) {
                        for (size_t j = i + 1; j < n; ++j) {
                            unsigned char z = 0;
                            if (fwrite(&z, 1, 1, fp) != 1) { fclose(fp); return 2; }
                        }
                        break; // rest bereits gepolstert
                    }
                }
            } else if (bd->destType == FT_INT) {
                if (fwrite(base, sizeof(int), 1, fp) != 1) { fclose(fp); return 2; }
            } else if (bd->destType == FT_DBL) {
                if (fwrite(base, sizeof(double), 1, fp) != 1) { fclose(fp); return 2; }
            } else {
                // unbekannter Typ (hier nicht vorgesehen)
                fclose(fp);
                return 2;
            }
        }
    }

    fclose(fp);
    return 0;
}

int load_array_bin(
    const char *filename,
    void **outItems,             // Rueckgabe: malloc-Buffer mit Datensaetzen
    size_t itemSize,             // sizeof(Record)
    size_t *outCount,            // Rueckgabe: Anzahl gelesener Elemente
    const struct FieldBinding *binds,
    int bindCount){
        
    *outItems = NULL;
    if (outCount) *outCount = 0;

    FILE *fp = fopen(filename, "rb");
    if (!fp) return 1;

    // sequenzielles Einlesen ohne fseek: wir versuchen Record fuer Record zu lesen
    size_t capacity = 16;
    size_t count = 0;
    void *buf = NULL;

    buf = malloc(capacity * itemSize);
    if (!buf) { fclose(fp); return 4; }

    while(1) {
        // Lese einen Record Feld fuer Feld.
        // Falls wir gleich am Anfang eines Records auf EOF stossen (0 gelesene Bytes beim ersten Feld),
        // brechen wir sauber ab. Falls mitten im Record weniger als benoetigt gelesen wird -> Fehler 3.

        // Zieladresse des naechsten Records
        char *rec = (char*)buf + count * itemSize;

        // Merker, ob wir ueberhaupt irgendetwas fuer diesen Record aus der Datei gelesen haben
        int anyByteReadForThisRecord = 0;

        for (int b = 0; b < bindCount; ++b) {
            const struct FieldBinding *bd = &binds[b];
            char *base = rec + bd->destOffset;

            if (bd->destType == FT_STR) {
                size_t nbytes = (size_t)bd->destSize;
                if (nbytes == 0) { free(buf); fclose(fp); return 3; }

                size_t got = fread(base, 1, nbytes, fp);
                if (got == 0) {
                    // EOF oder Fehler bevor dieser Record begann?
                    // wenn bisher in diesem Record noch nichts gelesen wurde: sauberes EOF -> Ende
                    if (!anyByteReadForThisRecord) {
                        fclose(fp);
                        if (outCount) *outCount = count;
                        *outItems = buf;
                        return 0;
                    }
                    // sonst mitten im Record abgebrochen -> trunkierter Record
                    free(buf); fclose(fp); return 3;
                }
                anyByteReadForThisRecord = 1;

                if (got < nbytes) {
                    // mitten im Feld abgeschnitten -> trunkiert
                    free(buf); fclose(fp); return 3;
                }

                // String im Ziel sicher nullterminieren (harte Abschneidung moeglich)
                ((char*)base)[nbytes - 1] = '\0';

            } else if (bd->destType == FT_INT) {
                size_t got = fread(base, sizeof(int), 1, fp);
                if (got != 1) {
                    if (!anyByteReadForThisRecord) {
                        // sauberes EOF vor neuem Record
                        fclose(fp);
                        if (outCount) *outCount = count;
                        *outItems = buf;
                        return 0;
                    }
                    free(buf); fclose(fp); return 3;
                }
                anyByteReadForThisRecord = 1;

            } else if (bd->destType == FT_DBL) {
                size_t got = fread(base, sizeof(double), 1, fp);
                if (got != 1) {
                    if (!anyByteReadForThisRecord) {
                        fclose(fp);
                        if (outCount) *outCount = count;
                        *outItems = buf;
                        return 0;
                    }
                    free(buf); fclose(fp); return 3;
                }
                anyByteReadForThisRecord = 1;

            } else {
                free(buf); fclose(fp); return 2;
            }
        }

        // kompletter Record gelesen -> zaehlen und ggf. Puffer vergroessern
        ++count;
        if (count == capacity) {
            size_t newCap = capacity * 2;
            void *nb = realloc(buf, newCap * itemSize);
            if (!nb) { free(buf); fclose(fp); return 4; }
            buf = nb;
            capacity = newCap;
        }
    }
}



////UNIT TESTS
static int tests_total = 0;
static int tests_fail  = 0;

static void expect_int_eq(const char *name, int ok, int got, int want){
    ++tests_total;
    if (!ok || got != want){
        ++tests_fail;
        printf(COLOR_RED "[FAIL] %s  ok=%d got=%d want=%d" COLOR_RESET "\n", name, ok, got, want);
    } else {
        printf(COLOR_GREEN "[OK]  " COLOR_RESET "%s -> %d\n", name, got);
    }
}

static void expect_str_eq(const char *name, int ok, const char *got, const char *want){
    ++tests_total;
    if (!ok || strcmp(got, want) != 0){
        ++tests_fail;
        printf(COLOR_RED "[FAIL] %s  ok=%d got=\"%s\" want=\"%s\"" COLOR_RESET "\n", name, ok, got, want);
    } else {
        printf(COLOR_GREEN "[OK]  " COLOR_RESET "%s -> \"%s\"\n", name, got);
    }
}

static double dabs(double x){ return x < 0 ? -x : x; }

static void expect_dbl_eq(const char *name, int ok, double got, double want, double eps){
    ++tests_total;
    if (!ok || dabs(got - want) > eps){
        ++tests_fail;
        printf(COLOR_RED "[FAIL] %s  ok=%d got=%.12g want=%.12g" COLOR_RESET "\n", name, ok, got, want);
    } else {
        printf(COLOR_GREEN "[OK]  " COLOR_RESET "%s -> %.6f\n", name, got);
    }
}

int run_mask_tests(void){
    printf(COLOR_CYAN "== masken-tests ==" COLOR_RESET "\n");

    // --- int: buffer -> int
    {
        int v=0, ok=0;
        ok = buffer_to_int("1__2__", "______", 6, &v);
        expect_int_eq("buffer_to_int 1__2__", ok, v, 12);

        ok = buffer_to_int("_12___", "______", 6, &v);
        expect_int_eq("buffer_to_int _12___", ok, v, 12);
      
        ok = buffer_to_int("______", "______", 6, &v);
        expect_int_eq("buffer_to_int empty", ok, v, 0); //ok==0 erwartet
    }

    // --- int: int -> buffer
    {
        char out[32]; int ok=0;
        ok = int_to_buffer(12, "______", 6, out);
        expect_str_eq("int_to_buffer 12", ok, out, "12____");

        ok = int_to_buffer(140, "______", 6, out);
        expect_str_eq("int_to_buffer 140", ok, out, "140___");

        ok = int_to_buffer(1234567, "______", 6, out); // zu lang -> ok==0
        ++tests_total;
        if (!ok){
            printf(COLOR_GREEN "[OK]  " COLOR_RESET "int_to_buffer overflow -> ok=0\n");
        } else {
            ++tests_fail;
            printf(COLOR_RED "[FAIL] int_to_buffer overflow erwartete ok=0" COLOR_RESET "\n");
        }
    }

    // --- double: buffer -> double
    {
        double d=0; int ok=0;
        ok = buffer_to_double("1__.5_", "___.__", 6, &d);
        expect_dbl_eq("buffer_to_double 1__.5_", ok, d, 1.5, 1e-9);

        ok = buffer_to_double("1_5._5", "___.__", 6, &d);
        expect_dbl_eq("buffer_to_double 1_5._5", ok, d, 15.5, 1e-9);

        ok = buffer_to_double("_15.5_", "___.__", 6, &d);
        expect_dbl_eq("buffer_to_double _15.5_", ok, d, 15.5, 1e-9);

        ok = buffer_to_double("___.01", "___.__", 6, &d);
        expect_dbl_eq("buffer_to_double ___.01", ok, d, 0.01, 1e-9);
    }

    // --- double: double -> buffer
    {
        char out[32]; int ok=0;
        ok = double_to_buffer(15.5, "___.__", 6, out);
        expect_str_eq("double_to_buffer 15.5", ok, out, "015.50");

        ok = double_to_buffer(0.01, "___.__", 6, out);
        expect_str_eq("double_to_buffer 0.01", ok, out, "000.01");

        ok = double_to_buffer(1234.56, "___.__", 6, out); // zu viele int-digits -> ok==0
        ++tests_total;
        if (!ok){
            printf(COLOR_GREEN "[OK]  " COLOR_RESET "double_to_buffer overflow -> ok=0\n");
        } else {
            ++tests_fail;
            printf(COLOR_RED "[FAIL] double_to_buffer overflow erwartete ok=0" COLOR_RESET "\n");
        }
    }

    // --- string: buffer -> str
    {
        char s[32]; int ok=0;
        ok = buffer_to_str("brot____", "________", 8, s, sizeof s);
        expect_str_eq("buffer_to_str brot____", ok, s, "brot");

        ok = buffer_to_str("________", "________", 8, s, sizeof s);
        expect_str_eq("buffer_to_str leer", ok, s, ""); // leer ist erlaubt -> ""
    }

    // --- string: str -> buffer
    {
        char out[32]; int ok=0;
        ok = str_to_buffer("brot", "________", 8, out);
        expect_str_eq("str_to_buffer brot", ok, out, "brot____");

        ok = str_to_buffer("abcdefgh", "____", 4, out);
        expect_str_eq("str_to_buffer clip", ok, out, "abcd");
    }

    // --- zusammenfassung
    printf(COLOR_CYAN "== result: %d tests, %d fails ==" COLOR_RESET "\n", tests_total, tests_fail);
    return tests_fail;
}

int DotPos(const char *txt) {
    int pos=-1;
    for (int i=0;txt[i]!='\0';i++) {
        if (txt[i]=='.') {
           pos=i;
           break;
        }
    }
    return pos;    
}

int main() {
    struct Product *catalog = NULL;     // dynamisches Array
    int catalogIndex = 0;               // aktueller Index (Zeile)
    int catalogCount = 0;               // Anzahl belegter Elemente
    int catalogCap = CATALOG_CHUNK;     // Kapazitaet der Allokation
    int listTop = 0;     // Index der ersten sichtbaren Zeile
    int pageSize = 0;    // wie viele Zeilen tatsaechlich Platz finden
    int ch;
  
    catalog = (struct Product*)malloc((size_t)catalogCap * sizeof(struct Product));
    if (!catalog) { printf("Speicherfehler\n"); return 1; }
    
    struct FieldDef productFields[] = {
        { "Artikel: ", FT_STR, 10, "__________", 0, 0, 0, NULL, 0},
        { "Nummer: ", FT_INT, 10, "__________", 0, 0, 0, NULL, 0},
        { "Kategorie: ", FT_STR, 10, "__________", 0, 0, 0, NULL, 0},
        { "Preis: ", FT_DBL, 6, "___.__", 0, 0, 0, NULL, 0}
    };
    /*
    struct FieldDef productFields[] = {
        { "Artikel:", FT_STR, 10, "__________", 0,0,0, NULL,0 },
        { "Nummer:",  FT_INT, 10, "__________", 0,0,0, NULL,0 },
        { "Preis:",   FT_DBL,  6, "___.__",    0,0,0, NULL,0 },
        { "Ort:",     FT_STR,  4,  "____",     0,0,0, NULL,0 },
        { "Nur Label",     FT_STR,  0,  "",     0,0,0, NULL,0 },
        { "Count:",   FT_INT,  6,  "______",   0,0,0, NULL,0 },
        { "ID:",      FT_INT,  6,  "______",   0,0,0, NULL,0 },
    };*/
    
    system("cls");
    if (run_mask_tests()>0) getch();
    system("cls");
    
    int fieldCount = sizeof(productFields) / sizeof(productFields[0]);
    //
    for (int i = 0; i < fieldCount; i++) {
        productFields[i].Buffer = (char*)malloc(productFields[i].MaxLen + 1);
        strcpy(productFields[i].Buffer, productFields[i].Mask);
    }
    
    struct FormDef form = {
        .Title = "Hinzufugen eines neuen Produkts:",
        .Fields = productFields,
        .FieldCount = (int)(sizeof(productFields)/sizeof(productFields[0])),
        .Width = 60,
        .StartX = 3,
        .StartY = 2,
        .RowGap = 2,
        .btnLabel = " <HINZUFUEGEN> ",
        .ActiveIndex = 0
    };
    
    struct FieldBinding prodBinds[] = {
      {0, offsetof(struct Product, Artikel), FT_STR, sizeof(((struct Product*)0)->Artikel)},
      {1, offsetof(struct Product, Artikelnum), FT_INT, 0},
      {2, offsetof(struct Product, Kategorie), FT_STR, sizeof(((struct Product*)0)->Kategorie)},
      {3, offsetof(struct Product, Preis), FT_DBL, 0},
    };
    
    if (!CalculateFormLayout(&form)) {
        gotoxy(form.StartX, form.StartY);
        printf(COLOR_RED "Fehler: Rahmenbreite zu klein fuer Felder oder Schaltflaechen." COLOR_RESET);
        getch();
        return 0;
    } else {
        pageSize = computePageSize(&form);
        //pageSize = 5;
        
        DrawHeader(&form);
        DrawFooter(catalog, sizeof(struct Product), &form, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, 0, 0, catalogCount);
        DrawForm(&form);
        
        
    }
    
    do {
        ch=getch();
        if (ch == 9) { //Tab
           if (form.ActiveIndex < form.FieldCount) form.ActiveIndex++; else form.ActiveIndex=0;
           DrawForm(&form);
        } else if (ch == 13) { //Enter
           if (form.ActiveIndex < form.FieldCount) form.ActiveIndex++; else {
              form.ActiveIndex=0;
              //Speichern hier
              // sicherstellen, dass Platz fuer aktuellen Index existiert
              if (!ensure_catalog_capacity(&catalog, &catalogCap, catalogIndex)) {
                  printf("Speicherfehler (realloc)\n");
                  return 0;
              }
              save_record(&catalog[catalogIndex], &form, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]));
              catalogIndex++;
              if (catalogCount<catalogIndex) {
                  catalogCount++;
              } else {
                 load_record(&catalog[catalogIndex], &form, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]));
              }
              if (catalogCount==catalogIndex) {                 
                     for (int i = 0; i < fieldCount; i++) {
                         strcpy(productFields[i].Buffer, productFields[i].Mask);
                         productFields[i].CurIndex=0;
                     }
              }
           }
           redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
        } else if (ch>='0' && ch<='9' && form.ActiveIndex<form.FieldCount) {
          if (form.Fields[form.ActiveIndex].CurIndex<form.Fields[form.ActiveIndex].MaxLen) {
               if (form.Fields[form.ActiveIndex].Type==FT_DBL) {
                     int dp = DotPos(form.Fields[form.ActiveIndex].Buffer);
                     if (dp==form.Fields[form.ActiveIndex].CurIndex) {
                        form.Fields[form.ActiveIndex].CurIndex++;
                        printf(".");
                     }
               }
               form.Fields[form.ActiveIndex].Buffer[form.Fields[form.ActiveIndex].CurIndex++]=(char)ch;
               printf("%c",ch);
               //DrawForm(&form);
          }
        } else if (ch == 8 && form.ActiveIndex<form.FieldCount) { // Backspace
          if (form.Fields[form.ActiveIndex].CurIndex > 0) {
                  if (form.Fields[form.ActiveIndex].Type==FT_DBL) {
                     int dp = DotPos(form.Fields[form.ActiveIndex].Mask);
                     if (dp==form.Fields[form.ActiveIndex].CurIndex-1) {
                        printf("\b\b_\b");
                        form.Fields[form.ActiveIndex].CurIndex-=2;
                        form.Fields[form.ActiveIndex].Buffer[form.Fields[form.ActiveIndex].CurIndex]='_';
                     } else {
                        printf("\b_\b");
                        form.Fields[form.ActiveIndex].Buffer[--form.Fields[form.ActiveIndex].CurIndex]='_';
                     }
                  } else
                  {
                      form.Fields[form.ActiveIndex].Buffer[--form.Fields[form.ActiveIndex].CurIndex]='_';
                      printf("\b_\b");
                  }
          }
        } else if (((ch>='A' && ch<='Z') || (ch>='a' && ch<='z') || (ch==32)) && (form.ActiveIndex<form.FieldCount)) {
              if (form.Fields[form.ActiveIndex].Type==FT_STR && form.Fields[form.ActiveIndex].CurIndex<form.Fields[form.ActiveIndex].MaxLen) {
                 form.Fields[form.ActiveIndex].Buffer[form.Fields[form.ActiveIndex].CurIndex++]=(char)ch;
                 printf("%c",ch);
              }
        } else if ((ch=='.' || ch==',') && form.ActiveIndex<form.FieldCount) {
               if (form.Fields[form.ActiveIndex].Type==FT_DBL && form.Fields[form.ActiveIndex].CurIndex<form.Fields[form.ActiveIndex].MaxLen) {
                  int dp;
                  dp = DotPos(form.Fields[form.ActiveIndex].Buffer);
                  if (dp>=0) {
                     gotoxy(form.Fields[form.ActiveIndex].XField+dp+1,form.Fields[form.ActiveIndex].Y);
                     form.Fields[form.ActiveIndex].CurIndex=dp+1;
                  }
               }
        } else if (ch==0 || ch==224) {
               ch=getch();
               if (ch==72 && catalogIndex>0) { //UP
                  catalogIndex--;
                  if (catalogIndex<listTop) listTop=catalogIndex;
                  load_record(&catalog[catalogIndex], &form, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]));
                  redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
               } else  if (ch==80 && catalogIndex<catalogCount) { //DOWN
                  catalogIndex++;
                  if (catalogIndex!=catalogCount) load_record(&catalog[catalogIndex], &form, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]));
                  else {
                     for (int i = 0; i < fieldCount; i++) {
                       strcpy(productFields[i].Buffer, productFields[i].Mask);
                       productFields[i].CurIndex=0;
                     }
                  }
                  if (catalogIndex>last_visible(listTop, pageSize, catalogCount)) listTop++;
                  redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
               } else if (ch==79 && catalogCount>0) { //End
                     catalogIndex=catalogCount;
                     listTop=catalogIndex-pageSize+1;
                     if (listTop<0) listTop=0;
                     redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
               } else if (ch==71 && catalogCount>0) { //Home
                     catalogIndex=0;
                     listTop=0;
                     redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
               } else if (ch==59) { //F1
                  DrawHelp();
                  system("cls");
                  DrawHeader(&form);
                  redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
               }
        } else if (ch == 12) { //Ctrl-L
               if (catalog) { free(catalog); catalog = NULL; } // alten Speicher freigeben und neu laden
               size_t loadedCount = 0;
               void *loaded = NULL;
               //euturn code
               int rc = load_array_bin("default.db", &loaded, sizeof(struct Product), &loadedCount, prodBinds, form.FieldCount);
               if (rc != 0) {
                    printf("Fehler beim Laden!\n");
                    // bei Fehler: neu initialisieren mit leerem Katalog
                    catalogCap = CATALOG_CHUNK;
                    catalog = (struct Product*)malloc((size_t)catalogCap * sizeof(struct Product));
                    if (!catalog) { printf("Speicherfehler\n"); return 0; }
                    catalogCount = 0;
                    catalogIndex = 0;
               } else {
                    catalog = (struct Product*)loaded;
                    catalogCount = (int)loadedCount;
                    catalogCap = (int)loadedCount;       // Kapazitaet = exakt geladen
                    if (catalogCap < CATALOG_CHUNK) {    // mindestens Startgroesse
                        catalogCap = CATALOG_CHUNK;
                        void *nb = realloc(catalog, (size_t)catalogCap * sizeof(struct Product));
                        if (!nb) { printf("Speicherfehler\n"); free(catalog); return 0; }
                        catalog = (struct Product*)nb;
                    }
                    catalogIndex = catalogCount;
                    strcpy(currentFilename,"default.db");
                    system("cls");
                    printf("Aus " COLOR_CYAN "default.db" COLOR_RESET " wurden %d Eintraege geladen. Druecken Sie eine beliebige Taste, um fortzufahren...",catalogCount);
                    getch();
                    system("cls");
               }
               listTop=catalogIndex-pageSize+1;
               if (listTop<0 || listTop>catalogCount) listTop=0;
               DrawHeader(&form);
               redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
                  
        } else if (ch == 19) { //Ctrl-S
               if (save_array_bin("default.db", catalog, sizeof(struct Product), catalogCount, prodBinds, form.FieldCount) != 0)
                  printf("Fehler beim Speichern!\n");
               else strcpy(currentFilename,"default.db");
               short int x,y;
               getxy(&x,&y);
               DrawHeader(&form);
               gotoxy(x,y);
        } else if (ch == 6) { //Ctrl-F
               DrawFiltered(catalog, sizeof(struct Product), catalogCount, &form, prodBinds, fieldCount);
               DrawHeader(&form);
               redraw(&form, catalog, catalogCount, prodBinds, sizeof(prodBinds)/sizeof(prodBinds[0]), catalogIndex, listTop, pageSize);
               
        /*} else if (ch == 2) {
                     short int x,y;
                     getxy(&x,&y);
                     gotoxy(5,27);
                     printf("ci:%d;buf:%s!         ",form.Fields[form.ActiveIndex].CurIndex, form.Fields[form.ActiveIndex].Buffer);
                     gotoxy(x,y);
        } else if (ch == 14) {
                     short int x,y;
                     getxy(&x,&y);
                     gotoxy(5,28);
                     printf("CatalogIndex:%d;catalogCount:%d;listTop=%d                   ",catalogIndex,catalogCount, listTop);
                     gotoxy(x,y);*/
        }
    } while (ch!=3);
    
    //gotoxy(2,20);
    //system("pause");
    return 0;
}
