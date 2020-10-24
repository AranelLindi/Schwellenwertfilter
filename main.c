/// Standardbibliotheken
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/// Makros
#define imagepath "Erde.bmp"

// #######
// VARIABLEN
// #######
uint8_t header[54];     // 14 Bytes (header) + 40 Bytes (info header) = 54 Bytes
uint32_t dataPos;       // Position wo in der Datei die Pixeldaten anfangen
uint32_t width, height; // Breite/Höhe des Bildes (können wohl auch negativ sein, MÖGLICHE FEHLERQUELLE!)
uint32_t imageSize;     // Größe des Bildes in Byte

uint8_t *data;    // Enthält sämtliche Hex-Werte aller Pixel (darf nach einlesen, nicht bearbeitet werden!)
uint8_t *errdata; // Enthält die erodierte Menge die mit dem strukturierenden Element abgeblichen wurde
uint8_t *subdata; // Enthält die subtrahierte Menge, der oben beiden Arrays

// #######
// STRUKTUREN
// #######
typedef struct rgbform
{
    uint8_t r;      // rot-Anteil
    uint8_t g;      // grün-Anteil
    uint8_t b;      // blau-Anteil
    char error : 1; // Enthält 0, falls kein Fehler aufgetreten ist, andernfalls 1 (Bitfeld!)
} RGBform, erosion;

struct rgbform strElement[3][3] = {0, 0, 0}; // 3x3 großes Element, dass als sturkturierendes Element dient

// ********************************************************************
// // Gibt die Farbwerte in Bezug auf eine Koordinate des Bildes zurück
struct rgbform getRGBbyCoordinate(uint64_t x, uint64_t y, uint8_t *data_array)
{
    // Koordinatensystem beginnt links oben, Richtung: rechts unten
    RGBform rgb;

    if ((x > width) || (y > height)) // prüfen ob übergebene Koordinaten im Rahmen liegen
    {
        rgb.error = 1; // Leeres struct zurückgeben, mit Fehlerflag!
        return rgb;
    }

    // Die Berechnung, wieviele Fehler für eine bestimmte Koordinate übersprungen werden müssen, folgt der Gleichung:
    // pixel = ((x_1 - x_0) + (y_1 - y_0) * [Zeilenbreite]) * 3
    // x_1, y_1 : Zielkoordinaten
    // x_0, y_0 : Koordinaten Ursprung (0,0)
    // [Zeilenbreite] : Bildbreite, hier: width
    // * 3 : Ein Pixel, drei Farbwerte (RGB)
    uint32_t pixel = (x + (y * width)) * 3; // x-Koordinate einfach addieren, Zeilensprünge (y-Koordianten) mit Anzahl Pixel pro "Zeile" multiplizieren. Ergebnis *3 (rgb)

    // Hex-Werte von rgb zuweisen (insgesamt drei Array-Felder)
    rgb.r = data_array[pixel];
    rgb.g = data_array[pixel + 1];
    rgb.b = data_array[pixel + 2];
    rgb.error = 0;

    return rgb;
}
// ********************************************************************

// ********************************************************************
// Setzt die Farbwerte einer Koordinate auf die übergeben Farbwerte
void setRGBbyCoordinate(uint32_t x, uint32_t y, struct rgbform *rgb, uint8_t *data_array)
{
    // Bei einem Fehler wird im RGB Element der Error-Member auf 1 gesetzt!
    if ((x > width) || (y > height)) // prüfen ob übergebene Koordiaten im Rahmen liegen
    {
        rgb->error = 1;
        return;
    }

    uint32_t pixel = ((x) + ((y)*width)) * 3;

    data_array[pixel] = (*rgb).r;
    data_array[pixel + 1] = (*rgb).g;
    data_array[pixel + 2] = (*rgb).b;
}
// ********************************************************************

// ********************************************************************
// Erstellt ein neues Bitmap-File mit Header des Ausgangsbitmaps und mit Farbwerten in 'data'
void CreateBitmap(uint8_t *arr, const char *name) // arr enthält die Pixelwerte; Header ist immer der Gleiche!
{

    // ## !Hier wird davon ausgegangen, dass sämtliche Farbwerte der Pixel bereits in das Array data_plus geschrieben wurden! ##

    // 2. Sämtliche Array-felder aus data_plus in Hexa-dezimalform in die Datei schreiben
    FILE *file = fopen(name, "w");

    fwrite(header, 1, 54, file); // Den Header in das File schreiben

    fwrite(arr, 1, imageSize, file); // Pixeldaten in das File schreiben

    fclose(file);

    printf("\nBitmap erstellt!\n");
}
// ********************************************************************

// ********************************************************************
// Prüft für die angegebene Koordinate, ob die Menge erosiert werden muss
int menge(uint32_t x, uint32_t y, struct rgbform *bezugspunkt, uint8_t *data_array)
{

    int rows, columns;
    rows = 3;                         // Spalten
    columns = 3;                      // Zeilen
    int bezugspunkt_x, bezugspunkt_y; // Position des Bezugspunktes im strukturierenden Element (relativ zum Array)
    bezugspunkt_x = 2;
    bezugspunkt_y = 2;

    uint32_t p_arr[rows][columns]; // Array mit größe stukturierendes Element, dass speichert, welcher Pixel geprüft werden soll (falls Element teilweise aus Bild ragt)

    // Initialisierung des Arrays mit Standartwert 1
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            p_arr[i][j] = 1;
        }
    }

    if (bezugspunkt->error == 1)
        return 0; // Liegt der Bezugspunkt außerhalb der Bildkoordinaten, abbrechen

    if (x - (rows - bezugspunkt_x) < 0) // linke Seite ragt aus dem Bild
    {
        p_arr[0][0] = 0;
        p_arr[1][0] = 0;
        p_arr[2][0] = 0;
    }
    if (x + (rows - bezugspunkt_x) > width) // rechte Seite ragt aus dem Bild
    {
        p_arr[0][2] = 0;
        p_arr[1][2] = 0;
        p_arr[2][2] = 0;
    }

    if (y - (columns - bezugspunkt_y) < 0) // Kopf ragt aus dem Bild
    {
        p_arr[0][0] = 0;
        p_arr[0][1] = 0;
        p_arr[0][2] = 0;
    }
    if (y + (columns - bezugspunkt_y) > height) // Boden ragt aus dem Bild
    {
        p_arr[0][2] = 0;
        p_arr[1][2] = 0;
        p_arr[2][2] = 0;
    }
    // Hier: Es wurden alle Pixelfelder ausgeschlossen, die nicht innerhalb des Bildes liegen

    struct rgbform vergleich;

    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            if ((p_arr[i][j] == 1) && ((i != (bezugspunkt_x - 1)) & (j != (bezugspunkt_y - 1)))) // Nur vergleichen was im Bild liegt und NICHT der Bezugspunkt ist
            {
                // Aktuellen Vergleichspunkt abrufen
                vergleich = getRGBbyCoordinate(x - (rows - bezugspunkt_x - i), y - (columns - bezugspunkt_y - j), data_array);

                if (vergleich.error == 1)
                    return 0; // Bei Fehler abbrechen (Punkt liegt nicht im Bild, etc...)
                if ((vergleich.r != bezugspunkt->r) && (vergleich.g != bezugspunkt->g) && (vergleich.b != bezugspunkt->b))
                    return 0; // && sorgt dafür, dass bei einer nicht erfüllten Bedingung, die ganze Schleife mit return 0 beendet wird.
            }
        }
    }

    // Wurde vorher nicht abgebrochen, kann an dieser Stelle davon ausgegangen werden, dass alle Punkte in der Menge liegen
    return 1;
}
// ********************************************************************

// ********************************************************************
// Erosion: Prüft für übergebene Koordinate ob strukturierendes Element
// vollständig in der Menge liegt (1) oder nicht (0)
void Erosion(uint8_t *data, uint8_t *errosed)
{
    // Iteriert durch gesammtes Bild
    for (uint64_t x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            // Bezugspunkt abrufen
            erosion bezugspunkt = getRGBbyCoordinate(x, y, data);

            // Prüfung:
            // Liegt strukturierendes Element um Bezugspunkt in der Menge, den Bezugspunkt in neues Bild aufnehmen
            if (menge(x, y, &bezugspunkt, data))
                setRGBbyCoordinate(x, y, &bezugspunkt, errosed);
        }
    }

    // Bitmap ausgeben
    CreateBitmap(errosed, "aufg2.bmp");
}
// *********************************************************************

// *********************************************************************
// Subtrahierung: Binärisiertes Bild und erosiertes Bild werden voneinander
// abzgezogen um Kreisumfang der Erde zu erhalten
void Subtrahieren(uint8_t *pic1, uint8_t *pic2, uint8_t *data_array)
{
    uint64_t calc;
    for (uint64_t i = 0; i < imageSize; i++)
    {
        calc = pic1[i] - pic2[i];

        switch (calc) // Aus Schwarz, weiß machen bzw. umgekehrt
        {
        case 0:
            calc = 255;
            break;

        case 255:
            calc = 0;
            break;
        }

        // Subtraktion in neues Bild aufnehmen
        data_array[i] = abs(calc);
    }

    // Bitmap ausgeben
    CreateBitmap(data_array, "aufg3.bmp");
}

uint32_t structVergleich(const struct rgbform *a, const struct rgbform *b)
{
    if (a->r == b->r && a->g == b->g && a->b == b->b)
        return 1; // true
    else
        return 0; // false
}

struct point
{
    uint64_t x;
    uint64_t y;
};

void DrawDot(const uint64_t x, const uint64_t y, uint8_t *data) // Zeichnet einen roten Punkt an den Kreismittelpunkt und gibt das Bild als bmp aus.
{
    /*for(int i = -4; i != 4; i++) {
        setRGBbyCoordinate(x + i, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
        setRGBbyCoordinate(x, y+i, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    }*/

    for(int i = -4; i < 5; i++)
        for(int j = -4; j < 5; j++)
            setRGBbyCoordinate(x + i, y + j, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);


    /*setRGBbyCoordinate(x - 2, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x - 1, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x + 1, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x + 2, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);

    setRGBbyCoordinate(x, y - 2, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x, y - 1, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x, y, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x, y + 1, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x, y + 2, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);

    setRGBbyCoordinate(x - 1, y - 1, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x - 1, y + 1, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);

    setRGBbyCoordinate(x + 1, y - 1, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);
    setRGBbyCoordinate(x + 1, y + 1, &(struct rgbform){.r = 0, .g = 0, .b = 255}, data);*/

    CreateBitmap(data, "aufg4.bmp");
}

// **********************************************************************
// Kreismitte mit der Methode der kleinsten Quadrate bestimmen
void Kreismitte(uint8_t *data_array)
{

    struct rgbform Koordinate;   // Schablone zum Farbvergleich einer Koordinate
    const uint8_t ANZAHL = 7;    // Anzahl zu speichernder Koordinaten für Methode kleinster Quadrate
    uint32_t counter = 0;        // Counter für Anzahl aufgenommener Koordinatentupel
    struct point points[ANZAHL]; // Array, dass Koordinaten enthält

    // Koordinatentupel mit 0 initialisieren (nicht initialisierte Variablen haben undefinierten Wert!)
    for (uint8_t i = 0; i < ANZAHL; i++)
        points[i] = (struct point){.x = 0, .y = 0};

    // Exemplarisch 7 Koordinaten aus Kreis aufnehmen und daraus den Mittelpunkt berechnen
    for (uint64_t i = 1; i < width; i+= 10)
    {
        for (uint64_t j = 1; j < height; j+= 10)
        {
            Koordinate = getRGBbyCoordinate(i, j, data_array);

            if ((counter < 7) && (Koordinate.error != 1) && (Koordinate.r == 0) & (Koordinate.g == 0) & (Koordinate.b == 0))
            {
                points[counter].x = i;
                points[counter].y = j;

                counter++;
                //break;
            }            
        }
        //if (counter == 7)
        //    break;
    }

    if (counter < 7)
    {
        printf("Es wurden nicht genügend Koordinatentupel gefunden um Interpoleration vorzunehmen!");
        return;
    }

    //printf("debug!\n");

    // Aus allen Koordinaten 7 rausnehmen, die gleichmäßig über den Umfang verteilt sind:
    for (uint8_t i = 0; i < ANZAHL; i++)
        printf("%u : (%lu, %lu)\n", i, points[i].x, points[i].y);

    uint64_t m1, m2;

    const double tolerance = 1e-6;

    double a, b, r;

    // Durchschnitt der Koordinatentupel berechnen
    int i, j;
    double xAvr = 0.0;
    double yAvr = 0.0;

    for (i = 0; i < ANZAHL; i++)
    {
        xAvr += points[i].x;
        yAvr += points[i].y;
    }

    xAvr /= ANZAHL;
    yAvr /= ANZAHL;

    // Startwerte festlegen
    a = xAvr;
    b = yAvr;

    for (j = 0; j < 255; j++) // Max. Iterations
    {
        // Updaten
        double a0 = a;
        double b0 = b;

        // Durchschnitt Berechnen
        double LAvr = 0.0;
        double LaAvr = 0.0;
        double LbAvr = 0.0;

        for (i = 0; i < ANZAHL; i++)
        {
            double dx = points[i].x - a;
            double dy = points[i].y - b;
            double L = sqrt(dx * dx + dy * dy);

            if (fabs(L) > tolerance)
            {
                LAvr += L;
                LaAvr -= dx / L;
                LbAvr -= dy / L;
            }
        }
        LAvr /= ANZAHL;
        LaAvr /= ANZAHL;
        LbAvr /= ANZAHL;

        a = xAvr + LAvr * LaAvr;
        b = yAvr + LAvr * LbAvr;
        r = LAvr;

        if (fabs(a - a0) <= tolerance && fabs(b - b0) <= tolerance)
            break;
    }

    m1 = a;
    m2 = b;

    DrawDot(m1, m2, data_array);

    printf("m1 = %lu, m2 = %lu, r = %f\n", m1, m2, r);
}

/*
BMP Dateien: |BITMAPFILEHEADER| - |BITMAPINFOHEADER| - |PIXEL VALUES|
             |    14 Bytes    |   |    40 Bytes    |   | ...        |
*/

// ********************************************************************
// ## MAIN ##
int main(void)
{
    // ****
    // ERDE.BMP ÖFFNEN
    FILE *file = fopen(imagepath, "rb");

    if (!file) // FEHLER: Datei konnte nicht geöffnet werden
    {
        printf("-- BMP konnte nicht geladen werden! -- \n");
        return 0;
    }
    // ****

    // ****
    // HIER: Datei wurde erfolgreich gelesen; ALS NÄCHSTES: Header einlesen
    if (fread(header, 1, 54, file) != 54) // FEHLER: wenn weniger als 54 Bytes gelesen werden können (kein vollständiger Header, leere Datei), abbrechen
    {
        printf("-- Keine korrekte BMP Datei! (1) -- \n");
        return 0;
    }

    if (header[0] != 'B' || header[1] != 'M') // Jedes Bitmap-file beginnt mit "BM", dies prüfen!
    {
        printf("-- Keine korrekte BMP Datei! (2) -- \n");
        return 0;
    }
    // ****

    // ****
    // VERSCHIEDENE WERTE AUS HEADER EINLESEN
    dataPos = *(int *)&(header[0x0A]);   // 10 : 4 Bytes : Position in der Datei, ab der die Pixeldaten anfangen
    imageSize = *(int *)&(header[0x22]); // 34 : 4 Bytes : Größte der Bilddaten in Byte
    width = *(int *)&(header[0x12]);     // 18 : 4 Bytes : Breite des Bildes in Pixel
    height = *(int *)&(header[0x16]);    // 22 : 4 Bytes : Höhe des Bildes in Pixel

    // Sicherheitshalber prüfen ob fundamentale Werte korrekt abgerufen wurden und falls nicht, korrigieren!
    if (imageSize == 0)
        imageSize = width * height * 3; // RBG besitzt drei Hexadezimalwerte für den rot-grün-blau-Anteil, deshalb *3!
    if (dataPos == 0)
        dataPos = 54; // Standartmäßig ist ein BMP Header 54 Bytes lang (BITMAPFILEHEADER & BITMAPINFOHEADER)
    // ****

    // ****
    // SPEICHERPLATZ RESERVIEREN
    data = (uint8_t *)malloc(imageSize * sizeof(uint8_t));    // Original-Farbwerte
    errdata = (uint8_t *)malloc(imageSize * sizeof(uint8_t)); // erodierte Menge
    subdata = (uint8_t *)malloc(imageSize * sizeof(uint8_t)); // subtrahierte Menge
    // ****

    // ****
    // RGB-PIXEL-DATEN AUS BITMAP LESEN & FILE SCHLIEßEN
    fread(data, 1, imageSize, file); // !!WICHTIG!!: fread(...) bewegt den File-Pointer! (quasi Cursor) Deswegen wird hier (durch die 1) nicht von Dateianfang, sondern ab Byte 55 gelesen! Enthält rein die Pixelwerte!!
    fclose(file);
    // ****

    /*
    Diese beiden For-Schleifen iterieren Spalte für Spalte durch die Pixel des Bitmap
    */
    RGBform pixel;                    // Struct speichert jeweils Farbwerte
    for (int x = 0; x < (width); x++) // iteriert Zeilenweise
    {
        for (int y = 0; y < (height); y++) // iteriert Spaltenweise
        {
            // Farbwerte des aktuellen Pixels abrufen um Vergleiche durchzuführen
            pixel = getRGBbyCoordinate(x, y, data);

            if (pixel.error == 1) // Wurde ein Fehler ausgelöst, hier abbrechen
            {
                printf("\n-- Koordinatenfehler! (%i, %i) --\n", x, y);
                return 0;
            }

            // Hier ist Platz um aktuelle Koordinate bzw. Pixel zu untersuchen (Filter!)
            // ...

            if (((pixel.r == 0x00) && (pixel.g == 0x00) && (pixel.b == 0x00)))
            {
                pixel.r = 255;
                pixel.g = 255;
                pixel.b = 255;

                setRGBbyCoordinate(x, y, &pixel, data); // struct wird "by-Reference" übergeben, das spart Speicherplatz und Rechenleistung!
            }
            else
            {
                pixel.r = 0x00;
                pixel.g = 0x00;
                pixel.b = 0x00;

                setRGBbyCoordinate(x, y, &pixel, data);
            }
        }
    }

    // ****
    // ZWISCHENERGEBNIS: binärisiertes Bitmap
    CreateBitmap(data, "aufg1.bmp");
    // ****

    // ****
    // ZWISCHENERGEBNIS: Erosierung
    Erosion(data, errdata);
    // ****

    // ****
    // ZWISCHENERGEBNIS: Subtrahierung
    Subtrahieren(data, errdata, data);
    // ****
    CreateBitmap(data, "test.bmp");
    // ****
    // ZWISCHENERGEBNIS: Kreismitte
    Kreismitte(data);

    // ****
    // BEREINIGUNG & SPEICHER FREIGEBEN
    // Speicher freigeben
    free(data);
    free(errdata);
    free(subdata);
    // ****

    return 0;
}
// ********************************************************************

// Build: gcc main.cc -lm