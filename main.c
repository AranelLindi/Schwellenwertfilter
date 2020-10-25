/*
BMP Dateien: |BITMAPFILEHEADER| - |BITMAPINFOHEADER| - |PIXEL VALUES|
             |    14 Bytes    |   |    40 Bytes    |   | ...        |
*/

/// Standardbibliotheken
#include <math.h>   // Wurzelfunktion
#include <stdlib.h> // Speicherallokation
#include <stdio.h>  // Konsolenausgabe
#include <stdint.h> // int-Typen

/// Makros
#define Eingabe "Erde.bmp"              // Bild mit Erde, das bearbeitet werden soll
#define Schritt1 "Binärisierung.bmp"    // Binärisierung
#define Schritt2 "Erosion.bmp"          // Erosion
#define Schritt3 "Subtraktion.bmp"      // Subtraktion
#define Schritt4 "Kreismittelpunkt.bmp" // Kreismittelpunkt

/// Strukturen
typedef struct rgbform // Wichtig: Farbdaten werden nicht in RGB Reinfolge sondern in BGR in Bitmap gespeichert !!
{
    uint8_t r;      // rot-Anteil
    uint8_t g;      // grün-Anteil
    uint8_t b;      // blau-Anteil
    char error : 1; // Enthält 0, falls kein Fehler aufgetreten ist, andernfalls 1 (Bitfeld!)
} RGBform, erosion;

struct point // Stellt eine Koordinate innerhalb des Bildes dar
{
    uint64_t x;
    uint64_t y;
};

/// Globale Variablen
uint8_t header[54] = {0};       // 14 Bytes (header) + 40 Bytes (info header) = 54 Bytes
uint32_t dataPos = 0;           // Position wo in der Datei die Pixeldaten anfangen
uint64_t width = 0, height = 0; // Breite/Höhe des Bildes (können wohl auch negativ sein, MÖGLICHE FEHLERQUELLE!) (hat sich bisher aber nicht bewahrheitet)
uint64_t imageSize = 0;         // Größe des Bildes in Byte

uint8_t *data;    // Enthält sämtliche Hex-Werte aller Pixel (darf nach einlesen, nicht bearbeitet werden!)
uint8_t *errdata; // Enthält die erodierte Menge die mit dem strukturierenden Element abgeblichen wurde

//const RGBform strElement[3][3] = {0, 0, 0}; // 3x3 großes Element, dass als strukturierendes Element dient (siehe Funktion '') (schwarzes Rechteck)

const float d = 0.001808905f; // Durchmesser Sensor: 1.8089 mm
const float f = 0.005110703f; // Brennweite: 5.110703 mm
const float r_E = 6.371e3f;   // Erdradius in km
// Formeln: D/L = d/f => f = d*L/(2*r_E)

/// Funktionen
RGBform getRGBbyCoordinate(uint64_t x, uint64_t y, const uint8_t *const data_array) // Gibt die Farbwerte in Bezug auf eine Koordinate des Bildes zurück
{
    // Koordinatensystem beginnt links oben, Richtung: rechts unten
    RGBform rgb;

    if ((x > width) || (y > height)) // prüfen ob übergebene Koordinaten im Rahmen liegen
    {
        rgb.error = 1; // Leeres struct zurückgeben, mit Fehlerflag!
        return rgb;
    }

    // Die Berechnung, wieviele Felder für eine bestimmte Koordinate übersprungen werden müssen, folgt der Gleichung:
    // pixel = ((x_1 - x_0) + (y_1 - y_0) * [Zeilenbreite]) * 3
    // x_1, y_1 : Zielkoordinaten
    // x_0, y_0 : Koordinaten Ursprung (0,0)
    // [Zeilenbreite] : Bildbreite, hier: width
    // * 3 : Ein Pixel, drei Farbwerte (RGB)
    const uint32_t pixel = (x + (y * width)) * 3; // x-Koordinate einfach addieren, Zeilensprünge (y-Koordianten) mit Anzahl Pixel pro "Zeile" multiplizieren. Ergebnis *3 (rgb)

    // Hex-Werte von rgb zuweisen (insgesamt drei Array-Felder) (Achtung! Reinfolge ist umgekehrt: RGB -> BGR !)
    rgb.r = data_array[pixel + 2];
    rgb.g = data_array[pixel + 1];
    rgb.b = data_array[pixel];
    rgb.error = 0;

    return rgb;
}

void setRGBbyCoordinate(uint64_t x, uint64_t y, RGBform *const rgb, uint8_t *const data_array) // Setzt die Farbwerte einer Koordinate auf die übergeben Farbwerte
{
    // Bei einem Fehler wird im RGB Element der Error-Member auf 1 gesetzt!
    if ((x > width) || (y > height)) // prüfen ob übergebene Koordiaten im Rahmen liegen
    {
        rgb->error = 1;
        return;
    }

    const uint64_t pixel = ((x) + ((y)*width)) * 3;

    // Achtung! Reinfolge in Bitmap ist nicht RGB sondern BGR !!
    data_array[pixel + 2] = (*rgb).r;
    data_array[pixel + 1] = (*rgb).g;
    data_array[pixel] = (*rgb).b;
}

void BitmapErstellen(const uint8_t *const arr, const char *const name) // Erstellt ein neues Bitmap-File mit Header des Ausgangsbitmaps und mit Farbwerten in 'data'
{
    // arr enthält die Pixelwerte; Header ist immer der Gleiche!

    // Ab hier wird davon ausgegangen, dass sämtliche Farbwerte der Pixel bereits in das Array arr geschrieben wurden!

    // Sämtliche Array-felder aus data_plus in Hexa-dezimalform in die Datei schreiben
    FILE *file = fopen(name, "w");

    fwrite(header, 1, 54, file); // Den Header in das File schreiben

    fwrite(arr, 1, imageSize, file); // Pixeldaten in das File schreiben

    fclose(file);

    printf("\nBitmap erstellt! - Name: %s\n", name);
}

uint8_t mengeErosieren(uint32_t x, uint32_t y, const RGBform *const bezugspunkt, const uint8_t *const data_array) // Prüft für die angegebene Koordinate, ob die Menge erosiert werden muss
{
    const uint8_t rows = 3, columns = 3;                 // Zeile & Spalten
    const uint64_t bezugspunkt_x = 2, bezugspunkt_y = 2; // Position des Bezugspunktes im strukturierenden Element (relativ zum Array)

    uint32_t p_arr[rows][columns]; // Array mit Größe stukturierendes Element, das speichert, welcher Pixel geprüft werden soll (falls Element teilweise aus Bild ragt)

    // Initialisierung des Arrays mit Standartwert 1
    for (uint64_t i = 0; i < rows; i++)
        for (uint64_t j = 0; j < columns; j++)
            p_arr[i][j] = 1;

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

    RGBform vergleich;

    for (uint8_t i = 0; i < rows; i++)
        for (uint8_t j = 0; j < columns; j++)
            if ((p_arr[i][j] == 1) && ((i != (bezugspunkt_x - 1)) & (j != (bezugspunkt_y - 1)))) // Nur vergleichen was im Bild liegt und NICHT der Bezugspunkt ist
            {
                // Aktuellen Vergleichspunkt abrufen
                vergleich = getRGBbyCoordinate(x - (rows - bezugspunkt_x - i), y - (columns - bezugspunkt_y - j), data_array);

                if (vergleich.error == 1)
                    return 1; // Bei Fehler abbrechen (Punkt liegt nicht im Bild, etc...)
                if ((vergleich.r != bezugspunkt->r) && (vergleich.g != bezugspunkt->g) && (vergleich.b != bezugspunkt->b))
                    return 1; // && sorgt dafür, dass bei einer nicht erfüllten Bedingung, die ganze Schleife mit return 0 beendet wird.
            }

    // Wurde vorher nicht abgebrochen, kann an dieser Stelle davon ausgegangen werden, dass alle Punkte in der Menge liegen!
    return 0;
}

void Erosion(const uint8_t *const data, uint8_t *const errosed) // Erosion: Prüft für übergebene Koordinate ob strukturierendes Element vollständig in der Menge liegt (0) oder nicht (1)
{
    // Iteriert durch gesammtes Bild
    for (uint64_t x = 0; x < width; x++)
        for (uint64_t y = 0; y < height; y++)
        {
            // Bezugspunkt abrufen
            erosion bezugspunkt = getRGBbyCoordinate(x, y, data);

            // Prüfung:
            // Liegt strukturierendes Element um Bezugspunkt in der Menge, den Bezugspunkt in neues Bild aufnehmen
            if (!mengeErosieren(x, y, &bezugspunkt, data))
                setRGBbyCoordinate(x, y, &bezugspunkt, errosed);
        }

    // Bitmap ausgeben:
    BitmapErstellen(errosed, Schritt2);
}

void Subtrahieren(const uint8_t *const pic1, const uint8_t *const pic2, uint8_t *const data_array) // Subtrahierung: Binärisiertes Bild und erosiertes Bild werden voneinander abzgezogen um Kreisumfang der Erde zu erhalten
{
    uint8_t calc;
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

        data_array[i] = abs(calc); // Subtraktion in neues Bild aufnehmen
    }

    // Bitmap ausgeben:
    BitmapErstellen(data_array, Schritt3);
}

void RechteckZeichnen(uint64_t x, uint64_t y, uint8_t *const data) // Zeichnet ein rotes Rechteck an den mitgeteilten Punkt
{
    for (int i = -4; i < 5; i++)
        for (int j = -4; j < 5; j++)
            setRGBbyCoordinate(x + i, y + j, &(RGBform){.r = 255, .g = 0, .b = 0}, data);
}

void NormiertenErdvektorBerechnen(double x, double y, double z) // Berechnet den Erdvektor, der von Bildmitte zur Erdmitte (Kreismitte) zeigt
{
    // Koordinatensystem Bitmap:
    // x-Achse positiv nach rechts
    // y-Achse positiv nach unten

    const double x0 = width / 2;
    const double y0 = height / 2;

    const double vektor[3] = {x - x0, -(y - y0), z - 0}; // y-Achse ist gegenläufig (positiv nach unten), deswegen hier Minus davor setzen! 

    const double betrag = sqrt(vektor[0] * vektor[0] + vektor[1] * vektor[1] + vektor[2] * vektor[2]);

    if (betrag == 0)
    {
        printf("Division durch Null entdeckt! Kein Erdvektor vorhanden!");
        return;
    }

    // Vektor ausgeben und normieren:
    printf("\nNormierter Erdvektor bei:\n\t[ x = %f8 ]\n\t[ y = %f8 ]\n\t[ z = %f8 ]\n(Koordinatensystem: Ursprung links oben. x-Achse positiv nach rechts, y-Achse positiv nach unten)", vektor[0] / betrag, vektor[1] / betrag, vektor[2] / betrag);
}

void Kreismitte(uint8_t *const data_array) // Kreismitte mit der Methode der kleinsten Quadrate bestimmen
{
    RGBform Koordinate;          // Schablone zum Farbvergleich einer Koordinate
    const uint8_t ANZAHL = 7;    // Anzahl zu speichernder Koordinaten für Methode kleinster Quadrate
    uint32_t counter = 0;        // Counter für Anzahl aufgenommener Koordinatentupel
    struct point points[ANZAHL]; // Array, dass Koordinaten enthält

    // Koordinatentupel mit 0 initialisieren (nicht initialisierte Variablen haben undefinierten Wert!)
    for (uint8_t i = 0; i < ANZAHL; i++)
        points[i] = (struct point){.x = 0, .y = 0};

    // Exemplarisch 7 Koordinaten aus Kreis aufnehmen und daraus den Mittelpunkt berechnen
    for (uint64_t i = 1; i < width; i += 10)
    {
        for (uint64_t j = 1; j < height; j += 10)
        {
            Koordinate = getRGBbyCoordinate(i, j, data_array);

            if ((counter < 7) && (Koordinate.error != 1) && (Koordinate.r == 0) & (Koordinate.g == 0) & (Koordinate.b == 0))
            {
                points[counter].x = i;
                points[counter].y = j;

                counter++;
            }
            else if (counter == 7)
                break;
        }
        if (counter == 7)
            break;
    }

    if (counter < 7)
    {
        printf("Es wurden nicht genügend Koordinatentupel gefunden um Interpoleration vorzunehmen!");
        return;
    }

    /* ENTKOMMENTIEREN FALLS KOORDINATENTUPELN FÜR GAUSS-NEWTON-VERFAHREN AUSGEGEBEN WERDEN SOLLEN
    //
    // Aus allen Koordinaten 7 rausnehmen, die über den Umfang verteilt sind:
    for (uint8_t i = 0; i < ANZAHL; i++)
        printf("%u : (%lu, %lu)\n", i, points[i].x, points[i].y);
    */

    // Bestimmung des Kreismittelpunkts der Erde mit der Methode der kleinsten Quadrate (Gauss-Newton-Verfahren)
    // *********************************************************************************************************
    // Dazu wird über Punkte auf der Kreisbahn mittels eines iterativen Verfahrens ein Kreis
    // angenähert und die versucht, die Abstände der Punkte zur Kreisbahn zu minimieren.
    // Die Kreisgleichung
    //   f(x) :=   r^2 = (x - m_1)^2 + (y - m_2)^2
    // soll damit gelöst werden, bei der m_1 und m_2 die Koordinaten des Kreismittelpunkts
    // darstellen.
    uint64_t m1, m2; // Koordinaten des Kreismittelpunkts

    const double tolerance = 1e-6;

    double a, b, r; // Variablen für Gauss-Newton-Verfahren (a = m_1, b = m_2, r = r)

    // Durchschnitt der Koordinatentupel berechnen
    uint64_t i, j;
    double xAvr = 0.0; // arithmetisches Mittel der Koordinatentupel zum Abstand auf die Abszisse
    double yAvr = 0.0; // arithmetisches Mittel der Koordinatentupel zum Abstand auf die Ordinate

    for (i = 0; i < ANZAHL; i++)
    {
        xAvr += points[i].x;
        yAvr += points[i].y;
    }

    // siehe Def.
    xAvr /= ANZAHL;
    yAvr /= ANZAHL;

    // Startwerte für Iteration festlegen
    a = xAvr;
    b = yAvr;

    // Zur Lösung des Verfahrens, wird die Jacobi Matrix gebildet, welche aus den partiellen Ableitungen
    // der Kreisgleichung bestehen. Diese muss zur Lösung des überbestimmten Gleichungssystems 0 werden.
    // Sei f(x)_i die Kreisgleichung nach dem Koordinatentupel i und x_1 = m_1 sowie x_3 = c
    // Aufbau Jacobi Matrix:
    // [df(x)_1/dx_1, ..., df(x)_1/dx_3]
    // [   .        , ...,      .      ]
    // [   .        , ...,      .      ]
    // [df(x)_n/dx_1, ..., df(x)_n/dx_3]
    // Quelle: http://people.inf.ethz.ch/arbenz/MatlabKurs/node79.html

    for (j = 0; j < 255; j++) // Anzahl Iterationen (255); je mehr desto besser. Ab einer bestimmten Größe erfolgt jedoch nur sehr geringe Anpassung
    {
        // Neuen Wert für Iteration bestimmten (Zähler im Bruch; Jacobi Matrix):
        const double a0 = a;
        const double b0 = b;

        // Arithmetisches Mittel der Distanz, die minimiert werden soll um Kreis anzunähern:
        double LAvr = 0.0;
        double LaAvr = 0.0; // Distanz von a
        double LbAvr = 0.0; // Distanz von b

        for (i = 0; i < ANZAHL; i++)
        {
            const double dx = points[i].x - a;
            const double dy = points[i].y - b;
            const double L = sqrt(dx * dx + dy * dy);

            if (fabs(L) > tolerance) // prüfen ob Länge ausreichend minimiert wurde
            {
                LAvr += L;
                LaAvr -= dx / L;
                LbAvr -= dy / L;
            }
        }
        // siehe Def.
        LAvr /= ANZAHL;
        LaAvr /= ANZAHL;
        LbAvr /= ANZAHL;

        // Aktuelle Position des "Kreismittelpunkts" und Radius updaten, für nächste Iteration:
        a = xAvr + LAvr * LaAvr;
        b = yAvr + LAvr * LbAvr;
        r = LAvr;

        // Ist Differenz von m_1/m_2 und neuem Koordinatentupel ausreichend klein, kann davon
        // ausgegangen werden, dass Kreis sehr genau auf Punkten liegt, und damit auch Kreis-
        // mittelpunkt bzw. Radius ausreichend genau bestimmt wurden:
        if (fabs(a - a0) <= tolerance && fabs(b - b0) <= tolerance)
            break;
    }

    // Berechnete Werte an Variablen der Kreisgleichung übergeben:
    m1 = a;
    m2 = b;

    // Kreismittelpunkt markieren:
    RechteckZeichnen(m1, m2, data_array);

    // Bitmap zeichnen:
    BitmapErstellen(data_array, Schritt4);

    // Koordinaten Kreismittelpunkt inkl. Radius auf Konsole ausgeben:
    printf("\nKreismitte bei:\n\t[ x = %lu ]\n\t[ y = %lu ]\n\t[ r = %f ]\n", m1, m2, r);

    // Erdvektor berechnen und ausgeben:
    NormiertenErdvektorBerechnen(m1, m2, f * 2 * r_E / d);
}

int main(void)
{
    // ERDE.BMP ÖFFNEN:
    FILE *file = fopen(Eingabe, "rb");

    if (!file) // FEHLER: Datei konnte nicht geöffnet werden
    {
        printf("-- BMP konnte nicht geladen werden! -- \n");
        return 0;
    }

    // HIER: Datei wurde erfolgreich gelesen; ALS NÄCHSTES: Header einlesen:
    if (fread(header, 1, 54, file) != 54) // FEHLER: wenn weniger als 54 Bytes gelesen werden können (kein vollständiger Header, leere Datei), abbrechen
    {
        printf("-- Keine korrekte BMP Datei! (1) -- \n");
        return 0;
    }

    if ((header[0] != 'B') || (header[1] != 'M')) // Jedes Bitmap-file beginnt mit "BM", dies prüfen! (Magische Zahl)
    {
        printf("-- Keine korrekte BMP Datei! (2) -- \n");
        return 0;
    }

    // VERSCHIEDENE WERTE AUS HEADER EINLESEN:
    dataPos = *(int *)&(header[0x0A]);   // 10 : 4 Bytes : Position in der Datei, ab der die Pixeldaten anfangen
    imageSize = *(int *)&(header[0x22]); // 34 : 4 Bytes : Größte der Bilddaten in Byte
    width = *(int *)&(header[0x12]);     // 18 : 4 Bytes : Breite des Bildes in Pixel
    height = *(int *)&(header[0x16]);    // 22 : 4 Bytes : Höhe des Bildes in Pixel

    // Sicherheitshalber prüfen ob fundamentale Werte korrekt abgerufen wurden und falls nicht, korrigieren!
    if (imageSize == 0)
        imageSize = width * height * 3; // RBG besitzt drei Hexadezimalwerte für den rot-grün-blau-Anteil, deshalb *3!
    if (dataPos == 0)
        dataPos = 54; // Standardmäßig ist ein BMP Header 54 Bytes lang (BITMAPFILEHEADER & BITMAPINFOHEADER)

    // Ausgabe der Grundlegenden Bildinformationen:
    printf("Daten des Eingabebildes:\n\t[ Breite: %lu [px] ]\n\t[ Höhe: %lu [px] ]\n\t[ Bildgröße: %lu [px] ]\n", width, height, imageSize);

    // SPEICHERPLATZ RESERVIEREN:
    data = (uint8_t *)malloc(imageSize * sizeof(uint8_t));    // Original-Farbwerte
    errdata = (uint8_t *)malloc(imageSize * sizeof(uint8_t)); // erodierte Menge

    // RGB-PIXEL-DATEN AUS BITMAP LESEN & FILE SCHLIEßEN:
    fread(data, 1, imageSize, file); // !!WICHTIG!!: fread(...) bewegt den File-Pointer! (quasi Cursor) Deswegen wird hier (durch die 1) nicht von Dateianfang, sondern ab Byte 55 gelesen! (Cursor wurde vorher schon verschoben!) Enthält rein die Pixelwerte!!
    fclose(file);

    // Diese beiden For-Schleifen iterieren Spalte für Spalte durch die Pixel des Bitmap:
    RGBform pixel;                            // Struct speichert jeweils Farbwerte
    for (uint64_t x = 0; x < width; x++)      // iteriert Zeilenweise
        for (uint64_t y = 0; y < height; y++) // iteriert Spaltenweise
        {
            // Farbwerte des aktuellen Pixels abrufen um Vergleiche durchzuführen
            pixel = getRGBbyCoordinate(x, y, data);

            if (pixel.error == (uint8_t)(1)) // Wurde ein Fehler ausgelöst, hier abbrechen
            {
                printf("\n-- Koordinatenfehler! (%li, %li) --\n", x, y);
                return 1;
            }

            // Hier ist Platz um aktuelle Koordinate bzw. Pixel zu untersuchen (Filter!)
            // ...

            if (((pixel.r == 0x00) && (pixel.g == 0x00) && (pixel.b == 0x00)))
            {
                pixel.r = (uint8_t)(0);
                pixel.g = (uint8_t)(0);
                pixel.b = (uint8_t)(0);

                setRGBbyCoordinate(x, y, &pixel, data); // struct wird "by-Reference" übergeben, das spart Speicherplatz und Rechenleistung!
            }
            else
            {
                pixel.r = (uint8_t)(255);
                pixel.g = (uint8_t)(255);
                pixel.b = (uint8_t)(255);

                setRGBbyCoordinate(x, y, &pixel, data);
            }
        }

    // ZWISCHENERGEBNIS: binärisiertes Bitmap:
    BitmapErstellen(data, Schritt1);

    // ZWISCHENERGEBNIS: Erosierung:
    Erosion(data, errdata);

    // ZWISCHENERGEBNIS: Subtrahierung:
    Subtrahieren(data, errdata, data);

    // ZWISCHENERGEBNIS: Kreismitte:
    Kreismitte(data);

    // BEREINIGUNG & SPEICHER FREIGEBEN:
    free(data);
    free(errdata);
    //free(subdata);

    return 0;
}

// Buildbefehl: gcc main.cc -lm