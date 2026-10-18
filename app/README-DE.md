# Taktile als Offline-App

Taktile läuft hier mit seinem echten Code im Browser: die Oberfläche (`ui.js`) und der Klang (`pad_studio.c`, nach WebAssembly übersetzt). Das ist ein Test- und Spielgerät, nicht die Move.

## Auf dem iPhone installieren (offline)

Die App muss einmal von einer https-Adresse geladen werden. Danach läuft sie ohne Netz.

1. Inhalt des Ordners `dist/` auf eine https-Seite legen, z. B. GitHub Pages:
   - im Repo `taktonmusic/Taktile` einen Ordner `app/` anlegen und den Inhalt von `dist/` hineinkopieren (ohne `taktile-app-single.html`)
   - Settings → Pages → Branch `main`, Ordner `/ (root)`
   - Adresse: `https://taktonmusic.github.io/Taktile/app/`
2. Adresse in **Safari** öffnen, einmal **Starten** tippen.
3. Teilen-Symbol → **Zum Home-Bildschirm**. Ab jetzt startet sie wie eine App, auch ohne Netz.

Hoch- und Querformat passen auf einen Bildschirm, ohne Scrollen (getestet mit den Maßen des iPhone 13, auch mit Notch und Safari-Leisten).
Um das Layout liegt ein freier Rand, damit das Schließen-Symbol nichts verdeckt. Unter **⋯** kannst du ihn mit **− / +** ändern (wird gespeichert).

## Bedienung

- **Jog-Rad:** mit dem Finger im Kreis drehen, die Mitte antippen = Klick.
- **Display:** nach oben/unten ziehen blättert, auf eine Zeile tippen wählt sie, auf die gewählte Zeile tippen öffnet sie, nach links/rechts wischen = Pfeil, lange drücken = Back.

## Neue Taktile-Version testen (Tarball laden)

1. Links unten auf **⋯** tippen, dann **Modul laden**. Im Menü stehen auch Demo-Beat, Halten und Zurücksetzen.
2. `taktile-module.tar.gz` aus Dateien/iCloud wählen. Auch `ui.js` oder `dsp.wasm` einzeln gehen.
3. Die App startet neu und nutzt jetzt deine Oberfläche. Sie bleibt gespeichert und wird bei jedem Start benutzt, bis du ein neues Modul lädst. Oben steht, was geladen ist. **Original** setzt alles zurück.

Was dabei geht und was nicht:
- Alles in `ui.js` (Display, Bedienung, Menüs, Sequencer-Logik der Oberfläche) wird sofort getestet.
- `dsp.so` im Tarball ist das Programm für die Move und läuft im Browser nicht. Änderungen am Klang (`pad_studio.c`) brauchen einen neuen Build der App (`build.py`). Danach lädst du die neue `dsp.wasm` über **Modul laden** oder legst die App neu auf die Seite.

## Dateien

| Datei | Zweck |
| --- | --- |
| `index.html` | die App (Oberfläche + Taktile `ui.js`) |
| `dsp.wasm` | der Taktile-Klang (2 MB) |
| `worklet.js` | gibt den Ton aus |
| `sw.js`, `manifest.webmanifest`, `icon-*.png` | Offline-Modus und Installation |
| `taktile-app-single.html` | alles in einer Datei (nur zum schnellen Testen, kein Offline-Modus) |

## Neu bauen (bei einer neuen Taktile-Version)

```
pip install ziglang pillow
ZIG="python3 -m ziglang" python3 build.py /pfad/zu/Taktile/taktile
```

`build.py` ändert keine Dateien von Taktile. Der Klang wird aus einer Kopie von `src/pad_studio.c` gebaut, mit zwei Anpassungen (siehe `build_wasm.sh`): die Dateiinfo-Funktion nutzt `fstatat()` statt eines Systemaufrufs, und eine Größenprüfung der Schwung-Header-Datei entfällt für die 32-Bit-WebAssembly-Version.

## Was anders ist als auf der Move

- Pads haben keine Anschlagstärke (immer 100).
- Pad- und LED-Farben kommen aus der Farbtabelle von Schwung (Index 0 bis 127), nicht von der Hardware gemessen.
- Samples und Dateibrowser sind leer (nur die eingebauten Klänge).
- Der Ton kommt etwa 25 bis 50 Millisekunden verzögert (Puffer).
- Dein Stand wird im Browser gespeichert.
