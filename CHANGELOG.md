# Changelog

## 5.175.0 (18 October 2026)
- Browser: Regler scrollen deutlich langsamer (6 Klicks pro Schritt, hoechstens ein Schritt alle 150 ms)

## 5.174.0 (18 October 2026)
- Parametername oben rechts beim Drehen ausgeschrieben (z.B. PROBABILITY statt PROB, GRAIN DENSITY), lange Namen laufen durch

## 5.173.0 (18 October 2026)
- Browser: das Display zeigt immer nur die Ebene des zuletzt gedrehten Reglers (volle Breite); Regler 1 = Hauptordner, jeder weitere Regler oeffnet die naechste Ebene bis zu den Sounds

## 5.172.0 (18 October 2026)
- Browser: geteilte Ansicht entfernt, Liste wieder ueber die ganze Breite mit den Kategorie-Kaestchen oben; Bedienung der Ebenen nur ueber die Regler 1-5

## 5.171.0 (18 October 2026)
- Browser: Regler 1-5 steuern die Ebenen des Baums von oben (Regler 1 Hauptordner, 2 naechste Ebene ..., der letzte die Sounds), Regler 8 = Vorschau OFF/STOP/ALL (Shift+Regler 8 = Lautstaerke)

## 5.170.0 (18 October 2026)
- Browser: Regler 1 und 2 scrollen langsamer (Regler 2: 3 Klicks = 1 Eintrag, Regler 1: 4 Klicks = 1 Kategorie, kein schnelles Ueberspringen mehr)

## 5.169.0 (18 October 2026)
- Browser: Regler 1 = linke Spalte (Kategorien), Regler 2 = rechte Spalte (Sounds), Regler 3 = Vorschau AUS / STOP (Standard, nur im Stop) / ALL; Shift+Regler 3 = Vorschau-Lautstaerke

## 5.168.0 (18 October 2026)
- Browser: Pfeiltasten links/rechts wechseln die Kategorie (in Ordnerlisten: links = zurueck, rechts = oeffnen); lange Kategorienamen laufen in der linken Spalte durch

## 5.167.0 (18 October 2026)
- Browser: geteilte Ansicht (Option B3): links die Kategorien, rechts die Sounds; Regler 3 schaltet zwischen SPLIT und TABS

## 5.166.0 (18 October 2026)
- Granulator-Seite wie im Bild: grosse Puffer-Anzeige (Wellenform, Lese-Rahmen, Grain-Striche) und die 8 Werte als Text mit Balken darunter statt der Kacheln

## 5.165.0 (18 October 2026)
- Browser: Regler 2 = Vorschau-Lautstaerke (10-100 %, Standard 70 %, Anzeige oben rechts, leuchtet mit der Staerke); Design-Themes entfernt: es gibt nur noch das bisherige Standard-Design

## 5.164.0 (18 October 2026)
- Browser: Pad druecken laedt und spielt den markierten Sound sofort, auch bei PRV aus und ohne Jog-Klick (Back macht es rueckgaengig)

## 5.163.0 (18 October 2026)
- Browser: schnelles Scrollen auf Regler 2 entfernt, nur Regler 1 (Vorschau) bleibt und leuchtet

## 5.162.0 (18 October 2026)
- Browser: bei offenem Sound-Browser spielen die Pads nur den Ton, auch bei Record wird nichts in den Clip aufgenommen

## 5.161.0 (18 October 2026)
- Browser: Regler 1 (Vorschau, hell = an, gedimmt = aus) und Regler 2 (schnelles Scrollen) leuchten unter dem Display

## 5.160.0 (18 October 2026)
- Browser: Sounds, Kits, Samples, Presets und Effekte nach Namen sortiert (A-Z, INIT zuerst); Regler 2 = schnelles Scrollen

## 5.159.0 (18 October 2026)
- Browser: Regler 1 schaltet die Vorschau (PRV/OFF) ein und aus: nach rechts drehen = ein, nach links = aus (ersetzt das schnelle Scrollen auf Regler 1)

## 5.158.0 (18 October 2026)
- Scope: alle Ansichten waehlbar (Jog-Klick): 6 WAVE+16 Baender mit Peak-Kappen, 7 SPECTRUM, 8 STEREO mit Phasenanzeige, 9 WATERFALL, 10 BEAT; DSP liefert 16 Baender, Stereo-Punkte, Phase

## 5.157.0 (18 October 2026)
- Granulator-Anzeige neu: Puffer-Ansicht mit Wellenform der letzten Sekunden, Lesebereich (Spread) als Klammer, jeder Grain als Strich mit Laengenbalken

## 5.156.0 (18 October 2026)
- Browser redesign: breadcrumb path in the header (DRUMS>KITS>TECHNO) with PRV/OFF top right, 4-row list with scroll bar, bottom line with position and what the click does
- Category tabs: boxes above the list show the categories next to the open one; Shift+jog switches to the next/previous category without going back
- Kit preview: in KITS the 16 pad sounds of the highlighted kit are shown as a small grid (laid out like the pads)

## 5.155.0 (18 October 2026)
- Browser: preview on/off in the top right corner (PRV / OFF), toggled with the Up or Down button; with PRV on, every sound, kit, synth, sample and effect you scroll to is heard while playing, recording and stopped (before: stopped only)
- Tape: Wow much stronger (about +-2.5 % pitch at full, clearly audible wobble), Hiss louder and more linear (about -45 dB at full), Tape colour keeps the loudness (no more volume jump), more saturation and top roll-off

## 5.154.0 (18 October 2026)
- KITS folder of the sound browser is now always there on drum tracks (was only for a brand-new track), so the kit can be changed later
- KITS sorted into 9 categories (Techno + Hard, House + Disco, Trance + Psy, Bass + Breaks, HipHop + Lofi, Electro + Synth, Acoustic, World + Organic, FX + Ambient) plus USER KITS
- 23 new factory kits (65 in total): Peaktime, Berlin, Warehouse, Detroit, EBM, Chicago, Afrohouse, Techhouse, Goa, Darkpsy, Progressive, Jungle, Dubstep, Drill, Synthwave, AC Brush, AC Punk, AC Funk, Latin, Afro Perc, Risers, Impacts, Glitch Lab

## 5.153.0 (18 October 2026)
- Undo button glows only while there is something to undo (with Shift: to redo), like on the Move; Undo / Shift+Undo = Redo unchanged

## 5.152.0 (18 October 2026)
- Step Grid like the Move: Shift+Step 3 -> Step Grid (per track): 1/16 (default), 1/32, 1/16T, 1/8T, 1/8
- 1/32 = 2 pages per bar; triplets show 12 steps per page (every 4th step button and pad is switched off); the page label shows the grid
- Switching the grid keeps every note at its exact time (the part between two steps becomes nudge) and the loop keeps its length in time
- Recording, quantize, fill, capture, automation, playhead and loop growth all follow the track's grid
- Old projects open unchanged on the 1/16 grid

## 5.151.0 (18 October 2026)
- Engine: the sequencer clock now has 6 ticks per 1/16, so 1/32 notes and triplets (1/16T, 1/8T) can be real steps (groundwork for the Move-style Step Grid; nothing changes yet for existing projects)
- Old projects load unchanged (all tracks stay on the 1/16 grid)
- MIDI export knows the finer grids

## 5.150.0 (18 October 2026)
- Quantize like on the Move: Shift + step 16 quantizes the whole clip (on drum tracks all 16 sounds in one undo step) by the QUANTIZE amount
- QUANTIZE amount is now adjustable in 5 % steps (5-100 %), old saves keep their value
- Shift + step 3 page now shows Quantize, Grid, Count-In, Record Snap, Launch (Layout, Key and Scale live on the track page / Shift + step 9)
- Manuals updated

## 5.149.0 (18 October 2026)
- Chord mode now has two pages: page 1 Mode, Chord, Level, Track Key, Track Scale; page 2 (jog click) Layer FX, Layer Amt, Layer Prm, Prob, VelBias. Chord mode works on synth tracks only
- Track Key / Track Scale tiles are labelled TKey / TScl so they cannot be mixed up with the master Key / Scale
- Fixed drawings outside the display (slider mark, long tab names)
- Removed unused code (old Shift + step 1 song jump, unused helper functions and variables)
- Manuals updated (chord page, track key/scale, 28 scales, tape, take start, song via menu)

## 5.148.0 (18 October 2026)
- KEY and SCALE are now per track (Chord page and MIDI FX page): each track can have its own key and scale; 'Mstr' (default) follows the master Key / Scale, so old projects sound the same; pads, chord pads, scale quantise, MIDI effects and the arp use the track's values

## 5.147.0 (18 October 2026)
- KEY now really moves the pads: changing the key (e.g. C to D) shifts the notes of the pads and of the chord pads, in the sound and in the sequencer; Scale Quantise follows the key too (default key F = unchanged)

## 5.146.0 (18 October 2026)
- Chord mode now opens its own CHORD page and returns to your previous screen when the mode ends; the page has Mode, Chord, Level, Key, Scale and the three Layer knobs
- 16 new scales: Pentatonic major/minor, Blues, Major blues, Hirajoshi, In-Sen, Iwato, Egyptian, Whole tone, Diminished (both), Bebop dominant, Neapolitan minor, Lydian dominant, Altered, Persian (5 to 8 notes; pads, arp and MIDI effects follow the notes of the scale)

## 5.145.0 (18 October 2026)
- SONG mode now opens only from the menu (MORE > SONG); Shift + step 1 in the clips no longer jumps into it

## 5.144.0 (18 October 2026)
- Tape is now clearly audible: Tape adds real saturation with more head-bump warmth, Wow bends the pitch like a worn cassette (about 10 cents at full), Hiss unchanged

## 5.143.0 (18 October 2026)
- MIDI effects no longer crackle: OCTAVE, CHORD and DIATONIC CHORD now share the level between their notes, so stacked notes no longer overload the output

## 5.142.0 (18 October 2026)
- Recording into an empty clip like the Move: the clip begins at the bar where your take began (your first bar is step 1), grows reliably bar by bar even if the screen updates late, and each note lands in the bar you play it in
- Each track keeps its own clip start (new engine setting tph)
- Fixed a possible freeze while a take grows

## 5.141.0 (18 October 2026)
- Step page now reads BAR 2/4 (every bar = 16 steps = one page), so you always see which bar you are in
- Safer skip of a live-recorded note: an old skip can never stay behind

## 5.140.0 (18 October 2026)
- Recording into an empty clip: notes in bar 2, 3 ... are no longer lost when the sequencer was already running before you pressed Rec (the clip now grows from the bar where your take began, and every note keeps its real position)

## 5.139.0 (18 October 2026)
- Tape tiles (Tape, Hiss, Wow) as an extra block at the end of the FX BUS 2 page, so every track reaches them

## 5.138.0 (18 October 2026)
- GRAN insert effect removed (old projects load it as off); the GRAIN page stays

## 5.137.0 (18 October 2026)
- New TAPE page (MORE > TAPE): a soft cassette colour on the master output - Tape (warm saturation, head bump, softer highs), Hiss (very quiet noise, at most about -57 dB) and Wow (slow pitch wobble). All off by default

## 5.136.0 (18 October 2026)
- Granulator is back exactly as before (GRAIN page with all its tiles, GRAN insert effect)

## 5.135.0 (18 October 2026)
- Granulator removed again: the GRAIN page and its tiles are gone (old projects load with it switched off)

## 5.134.0 (18 October 2026)
- Pads: a note recorded live no longer sounds a second time right after you let go
- X (Delete) button: its release is never lost any more, so it does not stay stuck
- Chord mode stays on the track where you chose it (no longer switched off when you change track) and survives a restart

## 5.133.0 (18 October 2026)
- REC: clip length after recording into an empty clip works like on the Move - stop in the first half of a bar: that bar is not part of the clip (its notes are dropped); second half: it is

## 5.132.0 (18 October 2026)
- REC: a new clip recorded for the first time stays silent (no loop playback) until you stop the recording, then it plays in sync with the other clips

## 5.131.0 (18 October 2026)
- FX: DELAY and REVERB are now single insert effects (pick them in the FX list of any sound, also each drum pad); Amount = level, Rate = time / size, Depth = feedback / damping
- FX: drum sounds go back to their own insert effects, delay / reverb SENDS only on synth tracks
- REC: recording into a clip with notes no longer grows the clip from a note on the last step (undone)

## 5.130.0 (18 October 2026)
- SPEECH: much clearer voice (faster formant transitions, stress per word, slower pace, place cues for P/T/K, doubled letters fixed)
- SPEECH: S / Z / SH / CH are a clean hiss band instead of noise, voiced Z follows the voice
- SPEECH: new PLAY = LETTER - pick a letter with the Letter knob, then press a step: that step says the letter (EN + DE, 33 letters and sounds). Hold a step + knob 8 changes its letter
- REC: recording again into a clip that has notes - a note on the last step grows the clip by 16 steps
- KEY now only steers the MIDI effects (scale snap, diatonic chord); sounds keep their pitch
- FX: drum sounds got Delay and Reverb sends, the FX BUS tab and one FX page like synths (Insert 2 on page 2); Granulator removed from the insert list (own GRAIN page); SENDS / OUTPUT block renamed, tiles Delay / Reverb

## 5.129.0 (18 October 2026)
- SPEECH: Length (AUTO follows tempo, 1/16 to 2 BARS fits the text to the clip length)
- SPEECH: Play WHOLE/WORD (one word per note, restarts with the loop, starts on the beat)
- SPEECH: speech speed now scales with BPM

## 5.128.0 (18 October 2026)
- New SPEECH engine: a speech computer synth. Write a text (jog wheel, like naming a project), every note says it at the pad's pitch
- English and German (SpkEn / SpkDe, each in a deep and a high voice), found under SYNTHS > ENGINES > SPEECH
- SOUND tab: Text tile (turn to edit); ENG tab: Speed, Formant, Breath, Inflect
- The text is saved with the project

## 5.127.0 (18 October 2026)
- Drum synth ENGINE tab has its own four knobs (Sweep, Sweep time, Noise, Click), no longer shared with the filter page; all start at 50 = the sound as designed
- Sweep, Noise and Click are much more audible on kicks and toms

## 5.126.0 (18 October 2026)
- New drum tracks start with the INIT drum kit: pure synth drums (808 kick, 909 snare, clap, hats, toms, rim, cowbell), no preset needed
- Drum synth sounds get an ENGINE tab: Sweep, Sweep time, Noise, Click + Drive and filter, edit them like a synth sound
- The ENGINE tab shows only on drum sounds that have an engine; other drum sounds keep their tabs

## 5.125.0 (18 October 2026)
- Attack bei ALLEN Synths identisch: nur der Knopf bestimmt die Zeit; kein Sonderwert pro Preset mehr (Pad und FM starten bei 0 gleich hart)
- Decay bei Pads/Strings/Lead mit derselben Abfall-Kurve wie FM (Zeitkonstante 0,8 s × Decay-Knopf), nicht nur kürzeres Sustain; ganz oben (≥ 0,97) bleibt der Ton gehalten
- Alte Projekte: Attack 0,12 → 0, gehaltene Sounds bekommen Decay oben, klingen also wie vorher

## 5.124.0 (18 October 2026)
- Attack: EIN gemeinsamer Mechanismus für alle Engines – der Knopf bestimmt allein die Zeit (0 = sofort, 1 = 2 s), bei FM, Pads, Strings, Plucks, Glocken usw. gleich; beim Wählen eines Sounds setzt sich der Knopf auf den Attack, mit dem das Preset gebaut wurde (Pad bleibt weich, FM sofort); alte Projekte werden entsprechend umgerechnet
- Decay: auch bei gehaltenen Sounds (Pads, Strings, Lead): unter 0,2 stirbt der gehaltene Ton ab, bei 0 nur noch ein Klick – wie bei FM

## 5.123.0 (18 October 2026)
- Attack-Knopf wirkt jetzt auf allen Synth-Sounds: auch bei 2OP-FM (FmInit), Zupf-/Schlag-Sounds, Glocken usw. (vorher nur bei Pads/gehaltenen Sounds); bei 0,12 (Standard) und darunter bleibt alles wie bisher, darüber wird der Einsatz langsamer bis ca. 1,5 s

## 5.122.0 (18 October 2026)
- Aufnahme in leeren Clip: Bei Drum-Tracks wuchs die Loop-Länge nicht mit (jeder Drum-Sound blieb bei 16 Steps) – jetzt wachsen Track und alle Drum-Sounds gemeinsam in 16er-Schritten und bleiben synchron
- Beim zweiten Druck auf Record wird die Länge auf volle 16er-Takte aufgerundet (16, 32, 48 …) für alle Sounds des Tracks

## 5.121.0 (18 October 2026)
- Drum-Tracks haben keinen Zugriff mehr auf den gemeinsamen Delay/Reverb: keine Sends, kein FX-BUS-Tab; stattdessen zwei eigene Insert-Effekte pro Drum-Sound (INSERT 1 + INSERT 2)
- Synth-Tracks behalten Sends (DEL/REV) und die FX-BUS-Seiten mit allen Delay-/Reverb-Einstellungen
- Alte Projekte: Drum-Sends werden beim Laden auf 0 gesetzt; Mix-Seite sperrt Sends für Drum-Tracks

## 5.120.0 (18 October 2026)
- Tonleitern: 12 statt 3 (Minor, Phryg, Major, Dorian, Lydian, Mixolydian, Locrian, Harm.Moll, Mel.Moll, Phryg.Dominante, Doppelt-Harmonisch, Ungar.Moll) – überall (Pads, Arp, MIDI-FX, Akkorde, SCQ)
- ScQ-Knopf: reagiert in beide Drehrichtungen, zeigt Tonleiter + Anzahl verschobener Noten (bei Drum-Tracks Hinweis)
- Attack 0 = echter harter Attack bei Pad/Lead/Sub/Reese/Engines (vorher blieb der weiche Preset-Attack); alte Projekte bekommen einmalig den bisherigen weichen Wert, neue Sounds Standard 0.12
- Decay 0 = echter Snap/Klick (ca. 1/150 der natürlichen Länge statt 1/20)
- Grain-Größe bis 4,5 s (vorher 1,2 s), Puffer 11 s

## 5.119.0 (17 October 2026)
- Beim Drehen steht der Parameter-Name oben rechts (statt Track) – der Kachel-Name links bleibt

## 5.118.0 (17 October 2026)
- Beim Drehen eines Knopfs steht oben links sein voller Parameter-Name statt des Kachel-Namens (danach wieder der Kachel-Name)

## 5.117.0 (17 October 2026)
- Kacheln werden beim Drehen/Wechseln nicht mehr weiß hinterlegt (nur der Balken wird kräftiger)

## 5.116.0 (17 October 2026)
- Track FX: Send-Knöpfe heißen jetzt DEL und REV (statt DSND/RSND)

## 5.115.0 (17 October 2026)
- Bug: neues/geladenes Projekt löscht jetzt alle alten Noten in der Engine (Track 1 spielte einen Beat ohne Aufnahme)
- Song-Seite: Hilfetext überlappt nicht mehr mit den Szenen-Kästchen

## 5.114.0 (17 October 2026)
- Preset-Seite im Engine-Design: keine alten Kästen mehr, Preset- und Wave-Name als normale Kachel
- Alle Kacheln sehen in jedem Design gleich aus wie auf der Engine-Seite (kleiner Name, fetter Wert, Balken)

## 5.113.0 (17 October 2026)
- Synth-Tracks zeigen immer das normale Pad-Layout (Akkord-Pads schalten beim Track-Wechsel/Typ-Wechsel ab)
- Ein Step-Knopf startet im Stand sofort die Wiedergabe (jede Ansicht, auch mit gehaltenem Pad)
- Step-Fenster (Vel/Prob/Gate/Ratch/Nudge/Cond) ohne Überlappung mit dem Kopf
- EQ-Seite: größere Kurve, kleinere Parameterwerte

## 5.112.0 (17 October 2026)
- LIVE FX mode: the bottom two pad rows no longer play drum sounds; they are 16 new live effects (HALF SPEED, DOUBLE SPEED, WOBBLE, FLANGER, RING MOD, AUTO PAN, TREMOLO, WAH SWEEP, BASS CUT, TELEPHONE, UNDERWATER, DISTORT, KILL, REPEAT 3/16, BRAKE, PUMP)

## 5.111.0 (17 October 2026)
- CPU: sine oscillators/LFOs use a 2048-point table instead of sinf (82 places; output identical to 16 bit in the bench)

## 5.110.0 (17 October 2026)
- New standard track header: big tile name + small track on top, all tiles as a row (PRE ENG MOD FLT FX BUS EUC MID), no mode label; pages re-fitted below it (preset, filter, EQ, grain, sample, euclid, MIDI)
- Four new designs: NEON (white tiles), BLOCKS (thick bars), EMOJI (a face that gets happier), ROBOT (LED eyes); KNOBS design fits the new header

## 5.109.0 (17 October 2026)
- ENGINE 2 page removed (noise, fold, chop, tremolo, auto-pan ... ): Jog click on the engine page now opens the sound browser; old projects load with those knobs at neutral values

## 5.108.0 (17 October 2026)
- Hardware-style pages: named blocks of four knobs (ENGINE / FILTER + VOICES, TONE SHAPE / MOVEMENT, RING + PUMP / DUCK, INSERT / THIS TRACK + SENDS + OUTPUT, DELAY / ALL TRACKS + REVERB / ALL TRACKS)
- FX BUS pages regrouped: delay knobs together, reverb knobs together; type and width moved to page 2

## 5.107.0 (17 October 2026)
- MIDI FX page: Key and Scale now sit where Swing+ was (4th and 7th knob); VelBias moved to MIDI FX 2

## 5.106.0 (17 October 2026)
- Pad mode name PITCH is now NOTE (synth and drum tracks); synth tracks: NOTE <> CHORD

## 5.105.0 (17 October 2026)
- Track button no longer changes the pad layout / mode: only Shift + step 8 does
- A step switched on after playing a pad gets that pad's pitch (not only while the pad is held)
- Page 'SENDS' renamed TRK FX (it holds the track's own effect + send knobs), clearer where you are

## 5.104.0 (17 October 2026)
- Synth track: Shift + step 8 only switches note layout <> CHORD and no longer jumps to another page (no Favorites after leaving chord mode)
- SENDS page: the two send knobs are now named DlySend / RvbSend (DSND / RSND), so it is clear what they control

## 5.103.0 (17 October 2026)
- CHORD mode on the Move pads: left four columns play single notes, right four columns play the chords (same degrees), also when the pads reach the engine directly

## 5.102.0 (17 October 2026)
- New project: track 1 is now a Drum track (default kit) instead of a Synth
- Switching tracks no longer leaves the Favorites / chord page open on a drum or non-chord track; the track opens on its first page

## 5.101.0 (17 October 2026)
- Synth tracks no longer show the drum knob row in the sequencer view: they get TUN ATK DCY TON CUT RES PAN VOL (attack instead of drive)
- Drum tracks: Shift + step 8 walks note layout > note repeat > step sequencer; synth tracks: Shift + step 8 = note layout <> chord

## 5.100.0 (17 October 2026)
- Synth tracks: Shift + Step 8 now switches between your note layout and CHORD (steps stay on step 12 and on the Track button)
- The Track button never enters CHORD any more: on synth tracks it toggles note layout and STEPS

## 5.99.0 (17 October 2026)
- FX BUS pages are labelled as shared by all tracks (header no longer shows the track number)

## 5.98.0 (17 October 2026)
- GRAIN is called GRAIN again (BEADS name removed)
- MULTI and MIDI FX tiles: every parameter now has a readable name in the normal letters, also while the effect is OFF (no more dashes)
- Shared reverb/delay bus stops early when the tail has died away (saves CPU), cheaper plate modulation

## 5.97.0 (17 October 2026)
- Granulator replaced by a BEADS-style granular effect: grain size from 4 ms (short) to 1.2 s (long), 3 s history, up to 16 grains, random grain timing, dense clouds stay level
- GRAIN page renamed BEADS

## 5.96.0 (17 October 2026)
- One shared delay and reverb bus for all tracks: each track only sends into it (Dly / Rvb), the per-track delay and reverb are gone - far less CPU, no more crackling with 3-4 tracks
- New FX BUS tile: delay time, feedback, type, tone, return and reverb size, darkness, type, return, widths, low cut now apply to every track
- Reverb HALL is now a Dattorro plate reverb (ECHO and SHIM kept)
- Chorus and phaser removed
- PAD engine rebuilt as additive synthesis: 8 harmonics, the lowest four doubled into a drifting detuned pair, new Hollow knob instead of PWM
- Knob names shortened so every tile label uses the same text size

## 5.95.0 (17 October 2026)
- Less CPU, same sound: the engine now only visits the sounds, arps and effect buses that are really active instead of scanning all 80 lanes for every sample. Idle load is about 4x lower; the output of a sequenced song is bit-identical to 5.94.0
- PAD and ADDITIVE engines are 2.5x to 5x cheaper per voice (oscillator settings are updated every 16 samples, sine table instead of sinf/powf per partial and sample); sound difference is below audibility (additive -87 dB, pad -56 dB)
- Per-track effects stay as they are: measured, a track with delay, reverb, chorus, phaser and two insert effects costs only a few percent of the budget because idle effects are already skipped

## 5.94.0 (17 October 2026)
- SYNTH tile is back to its full 8 knobs (Wave, Detune, Sub, Drive, LPCut, Reso, FltEnv, Voices; engines accordingly) as before 5.89.0
- CHORD mode: a CHORD tile sits first and its 8 knobs control the chord options (Mode, Chord, Level, Layer FX + Amount + Param, Prob, VelBias); it opens by itself when the mode is switched on
- Undo / Redo show what was changed: the knob name (T03 LOWPASS CUTOFF, TEMPO, MASTER FILTER ...) in the toast and in the history

## 5.93.0 (17 October 2026)
- Pad modes by track type: drum tracks walk PITCH > REPEAT > STEPS, synth tracks walk CHORD > STEPS. Note Repeat is for drum tracks only; with no mode on, the synth pads play normally

## 5.92.0 (17 October 2026)
- Live FX is a full-screen mode on drum tracks now too: top two rows = the 16 effects, bottom two rows = fills (single hits) and rolls with the sounds of the drum track
- Rolls fit the sound: hats 1/32 (gently building), snare / clap 1/16 (building up strongly), kick 1/8 (steady), perc 1/16 triplets
- CHORD pad mode: the left 16 pads play single notes (without the MIDI effect), the right 16 pads (other colour, tonic white) play the same notes as chords

## 5.91.0 (17 October 2026)
- New pad mode CHORD on synth tracks: Shift + Step 8 (or the Track button) now walks PITCH > REPEAT > STEPS > CHORD (also in the menu: Chord pads on/off). The pads play chords through the track's MIDI effect
- While CHORD is on, the 8 favorite knobs become the chord options (Mode, Chord type, Level, Layer FX + Amount + Param, Prob, VelBias); outside the mode they are your favorites again
- Leaving CHORD brings your previous MIDI effect back; the mode survives a restart

## 5.90.0 (17 October 2026)
- Undo/Redo: a clip copy now also restores the sound the clip carried (before, only the steps came back)
- Saving: while playing, the project is written after a short pause (about a third of the former wait), so a Move restart or power-off loses much less. MIDI effects such as Chord mode are part of the saved project and stay after a restart

## 5.89.0 (17 October 2026)
- PRESET tile on synth tracks now reads Engine, Attack, Decay, Tone / Tune, Pan, Voices, Volume
- Streamlined synth tiles: no knob appears on two tiles any more. SYNTH keeps only its own controls (Wave/Detune/Sub/Drive, FM Init: Ratio/Index/Feedback/Drive, engines: their specific knobs + Drive); Voices lives on PRESET, cutoff/resonance/filter envelope on FILTER. Where an engine uses Filter Key/Decay as its own knobs, FILTER shows FltAtk/VelFilt instead

## 5.88.0 (17 October 2026)
- PRESET tile on synth tracks: Attack takes the place of Drive (Type, Tune, Decay, Tone, Attack, Pan, Voices, Vol) for instant sound shaping. Drive moves to the position Attack had on the SYNTH tile; Chop is back on SYNTH page 2; the wave stays on the SYNTH tile

## 5.87.0 (16 October 2026)
- Live FX mode: the pads only trigger effects and fill hits, the synth no longer sounds on the bottom pads
- Switching to another track ends the Live FX mode (switch it on again where you need it)

## 5.86.0 (16 October 2026)
- Synth tracks have no SAMPLE tile any more (own samples stay on drum tracks)
- Song mode: hold Delete + step pad deletes that song step (a short tap on Delete deletes the selected one); Copy + pad picks a step, Copy + another pad pastes a copy there; the display shows all 8 scenes (filled = has notes), the step LEDs 1-8 are bright for scenes with notes

## 5.85.1 (16 October 2026)
- Live FX fill pads always use the sounds of the first loaded drum track, whichever track is selected

## 5.85.0 (16 October 2026)
- INIT patches are dry: no delay or reverb send. New synth tracks start without delay and reverb (before, some tracks had a reverb send by default). Delay, reverb and all other effects stay adjustable per synth track and per drum pad on the FX tiles; master returns on the BUS tile

## 5.84.0 (16 October 2026)
- Shift + Step 11 opens the Note Repeat options (Rate, Repeat on/off, Full Velocity, RecSnap, ScaleQ, BPM, Swing), also when the Repeat mode is not open. Repeat on/off is an item of that page

## 5.83.0 (16 October 2026)
- Note Repeat on drum tracks: pick the sound on the left, every pad on the right repeats it right away at its speed while held (you hear it)
- Live FX, pad mode, Note Repeat and Step grid (Shift + Step 4 / 8 / 11 / 12) can be started from the clip view; it switches to the track's pads

## 5.82.0 (16 October 2026)
- Granulator: Tone is replaced by WINDOW (RECT, TRI, SINE, GAUSS, HANN, blended); the grain picture is redrawn with the real window shape of every grain
- Recording: when a clip grows or shrinks to the length of the take, the notes keep their place on the shared beat grid, so the clip stays in sync with the other clips

## 5.81.0 (16 October 2026)
- Undo / Redo now also covers: loop length changes (incl. drum sound lengths), recorded and cleared automation (knob motion), copied sounds / tracks, BPM and swing set with Shift + knob

## 5.80.1 (16 October 2026)
- Delete mode can no longer get stuck: releasing Delete always ends it (also with a menu open), and Back / Play / Rec / Menu / Undo clear it

## 5.80.0 (16 October 2026)
- SYNTH tile page 2: Chop is back for the sound engines; classic synths show Drive in its place (and the waveform window on the PRESET tile)

## 5.79.3 (16 October 2026)
- Sound engines: Drive is back on the PRESET tile (the waveform window stays on classic synths only)

## 5.79.2 (16 October 2026)
- The waveform window on the PRESET tile only shows for classic synths (where the wave has an effect); sound engines show the delay send there

## 5.79.1 (16 October 2026)
- Recording length now uses the DSP's exact play position when Rec is pressed the second time (pressing Rec within the first 16 steps keeps 16)

## 5.79.0 (16 October 2026)
- Recording: the clip length is set when Rec is pressed the second time (rounded up to 16, 32, 48 ... steps), on synth and drum tracks
- Clearing a clip sets its length back to 16 steps

## 5.78.0 (16 October 2026)
- PRESET tile on synth tracks: a small preset-style window to pick the waveform (Auto, Saw, Square, Tri, Sine ...) where Drive was. Drive is on SYNTH page 2
- Manuals, README and guides corrected accordingly

## 5.77.1 (16 October 2026)
- New synth track: the browser opens on the SYNTHS family list (Analog, Wavetable, FM 4-OP, FM 2-OP, Fold ...) instead of inside the first family

## 5.77.0 (16 October 2026)
- SYNTH tile, page 2: Drive replaces Chop (Chop removed). On synth tracks the SOUND tile shows the delay send where Drive was (drums keep Drive)

## 5.76.0 (16 October 2026)
- Synth tracks: no more click when the next pad is played (the old note now fades out in about 10 ms instead of being cut)
- Setting a step while stopped starts playback

## 5.75.0 (16 October 2026)
Desktop-tested (UI simulator, 60,000-event fuzzing, native builds, GLIBC 2.17 check); not yet confirmed on the device.

**Tiles and navigation**
- Track tiles reorganised: 1 PRESET · 2 SYNTH (synth tracks) · MOD · FILTER (+ EQ) · FX (+ GRAIN) · FX TYPE (+ FX TIME) · EUCLID · MIDI FX · SAMPLE (now also on synth tracks). Jog click switches the page of a tile
- Header: tile number and name top left, page (e.g. 2/2, EUCLID 1/7) top right, always
- Left / Right = previous / next track (Shift = step page); in lists and name entry they move the selection / cursor
- Menu always opens at the top on QUICK. Knob turns show the name top left and only the value top right
- MOD tile: Ring, RingFrq, Pump, Wobble / Duck, DuckRel, DuckSrc, PumpCrv (drums: Rumble / RumbleTail instead of Wobble / PumpCrv). Drum tracks show a delay send on the first page instead of voices
- FX types turn more slowly; Euclid and MIDI FX are off by default (older saves are migrated once)

**Sequencing and recording**
- Quantize page (Shift + Step 3): grid 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32, strength 25–100 %, pad **Layout** (KEY, SEQ, OCT, 4THS, 3RDS, ROW8). "Quantize now" in the menu (EDIT CLIP) and Shift + Step 16. Live recording snaps to the grid and keeps the remainder as nudge
- Live recording keeps the real hold length of pads; a held pad + a step button sets that step to the pad's pitch and velocity
- Recording into an empty clip grows it with you: 16 → 32 → 48 … steps (also drum tracks)
- Euclid: master switch OFF / ON / ADD on the first page, lane pages start with the lane number; Euclid no longer overwrites steps while it is off
- Swing is correct for odd loop lengths (the groove restarts on every pass)
- No stray notes while the Synth / Drum question of a new track is open; a fresh drum slot clears old patterns in the engine

**Sounds and browser**
- Preset categories are single names (no more "A + B"); new synth tracks start in SYNTHS → first family → INIT

**Project and menu**
- Project slots: LOAD, SAVE HERE, RENAME, NEW EMPTY, DELETE; Back deletes a character in name entry
- EXPERT: CPU meter / Load info screen (load, peak, history, number of sounds and effects), Scope with new animations (DANCE, STARS, RINGS)
- Exit dialog: BACKGROUND / CLOSE selectable again; the scene toast no longer pops up during song playback

**Engine**
- Denormal flush-to-zero around every audio block (less CPU spike and crackle on decaying tails); the UI polls the engine less often while stopped

**Docs and CI**
- Manuals (EN/DE), animated guide, README and display preview updated to 5.75.0
- CI runs a syntax check, a UI smoke + fuzz test, a sanitizer compile check and the aarch64 build; the older 5.4-line tests live in `taktile/tools/test/legacy-5.4/`

**Not carried over from the previous main branch** (still available on the branch `pre-5.74-upload`): queued live project switch, mono sample storage with resample buffer guard, `CORRUPT` project guard, Shift + arrow sub-pages. The 5.7x line uses jog click for second pages.

## Previous main branch (not part of 5.75.0)
- **Live project switch.** While the sequencer plays, PROJECT → Load no longer stops the music. The chosen project is queued and swaps in at the end of the current bar: all tracks are muted (delay and reverb tails keep ringing), the new project is sent to the DSP with a fast queue (48 messages per tick instead of 3; a full project of about 1000 messages takes roughly half a second), then the transport restarts at step 1 with the new project and the mutes of that project. Turn Load again for the same slot, or stop the transport, to cancel. A stopped sequencer loads immediately, as before. It is not gapless: expect a short silence at the switch
- **Jog click is consistent now.** It never cycles pages any more: click = open / confirm, or back to the last tab on a track tab, or HOME. The sub-pages of MIX (Volume / Delay / Reverb), EUCLID (2 pages), SAMPLE (Sample / Slice / Envelope) and SCOPE (3 views) switch with **Shift + Left / Right** instead (both directions, wraps around). In-device help updated
- **Samples use half the memory.** WAV files are stored as mono (L+R)/2, which is how the engine always played them. A stereo file now takes the same space as a mono one, and resample takes are stored as mono too. The 40 MB bank holds twice as much stereo material
- **Resample respects the buffer.** The take buffer holds 40 s. At slow tempi fewer bars are chosen automatically and a message says so (16 bars below ~96 BPM used to be cut off silently). If the take would not fit into the free sample bank, recording is refused with "SAMPLE BANK FULL" instead of failing at the end
- `tools/test/`: headless UI tests with a mocked Schwung host (`test_jog.js`, `test_resample.js`) and a native DSP test with sanitizers (`run_dsp_test.sh`), all run in CI
- Repository cleanup: one `taktile/` folder (current state, 5.4.1) instead of three copies
- Build scripts are executable after cloning
- `fetch-header.sh` pins Schwung's plugin API header to tag v1.7.3 (`SCHWUNG_REF` to override)
- GitHub Actions: syntax check, sanitizer compile check, aarch64 build, release on `v*` tags
- Loading a project from an unreadable file shows `CORRUPT` and no longer overwrites the current state
- README: install by uploading the release tarball at move.local:7700, compatibility, compatibility, limits, troubleshooting

## 5.4.1
- Current development state (manuals EN/DE updated to 5.4.1)
