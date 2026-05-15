# Tracking: Dual-Screen Interactive Presentation Mode

Regel: Ein Punkt wird nur dann als abgeschlossen markiert, wenn er zu 100% umgesetzt, integriert und verifiziert ist.

Stand 2026-05-15 (Branch `copilot/add-dual-screen-presentation-mode`): Kernsystem ist implementiert und integriert; vollständige Laufzeitverifikation auf realen Multi-Screen-Setups steht noch aus.

## 1) Architektur & Kernzustand
- [x] Presentation Manager als zentrale Steuerung spezifizieren und integrieren (Start/Stop, Screen-Zuweisung, Sync)
- [x] Shared Application State für Presenter- und Audience-Window klar definieren und anbinden
- [x] Audience Tool State als zentrale Struktur einführen (Tool-Verfügbarkeit für Audience)
- [x] Synchronisationsmodi für Viewport-Logik definieren und implementieren (Follow Mode, Free Mode)

## 2) Fenster & Bildschirmzuweisung
- [x] Startlogik umsetzen: aktuelles Hauptfenster = Presenter-Screen, zweiter Screen = Audience-Screen
- [x] Fallback auf Single-Screen-Modus implementieren
- [ ] Optionale Bildschirmauswahl vorbereiten (Dialog/Wechselmechanismus)
- [x] Audience-Window als eigenes Qt-Fenster aufbauen (Fullscreen standardmäßig)

## 3) Rendering
- [x] Presenter-Renderer unverändert für kompletten Workspace nutzbar halten
- [x] Audience-Renderer-Modus implementieren: ausschließlich Seitenbereich rendern
- [x] Hartes Clipping auf Seitenrechteck sicherstellen (kein Backstage-Leak)
- [x] Sicherstellen, dass kein separater Dokument- oder Canvas-State für Audience entsteht

## 4) Interaktion & Werkzeuge
- [x] Audience-Toolbar als separates UI-Konzept implementieren
- [x] Dynamische Tool-Freigabe per Presenter-Toggles umsetzen (kein Rollen/Rechtesystem)
- [x] Tool-Deaktivierung strikt durchsetzen (deaktivierte Tools funktional gesperrt)
- [x] Audience-Interaktionen direkt auf Shared Canvas-State anwenden (live sichtbar beim Presenter)
- [x] Seitenwechsel und Präsentationsfluss exklusiv durch Presenter kontrollierbar halten

## 5) Zoom, Pan & Fokussteuerung
- [x] Presenter: freies Zoomen/Pannen innerhalb des bestehenden OpenBoard-Workflows
- [x] Audience: Zoom/Pan nur innerhalb der Seite und nur wenn freigegeben
- [x] Follow Mode als Standard aktivieren (Audience folgt Presenter-Viewport)
- [x] Free Mode implementieren (Audience-Viewport unabhängig)
- [x] Reset/Focus-Funktion umsetzen (Audience auf Fit-to-Page/Seitenfokus zurücksetzen)

## 6) Presenter Control Panel
- [ ] Neues Presenter-Control-Panel integrieren (Start/Stop, Tool-Toggles, Seitennavigation)
- [ ] Optionale Controls vorbereiten (Audience-Freeze, Bildschirmzuweisung, Audience-Vorschau)
- [x] Live-Aktualisierung aller Presenter-Änderungen in Audience-Ansicht sicherstellen

## 7) Qualität, Stabilität, Sicherheit
- [x] Keine Netzwerkarchitektur einführen (kein WebSocket, kein Server, kein Multi-Client)
- [ ] Regressionstestplan für bestehende OpenBoard-Workflows durchführen
- [ ] Funktionale Verifikation der Dual-Screen-Flows auf unterstützten Plattformen durchführen
- [ ] Sicherheitsprüfung: kein unbeabsichtigtes Sichtbarwerden von Backstage-Inhalten

## 8) MVP-Abgrenzung
- [ ] MVP-Umfang final bestätigen (ohne Langzeit-Optionen)
- [ ] Nicht-MVP-Funktionen klar als „später“ markieren (Freeze, Push-to-Stage, Multi-Audience, Preview, Szenen)

## Offene Fragen (vor Umsetzung klären)
- [ ] Soll die optionale Bildschirmauswahl bereits im MVP enthalten sein oder erst in einer Folgeiteration?
- [ ] Soll die Audience-Toolbar im MVP standardmäßig sichtbar sein oder initial ausgeblendet starten?
- [ ] Soll Audience-Zoom/Pan im MVP defaultmäßig erlaubt oder gesperrt sein?

## Umgesetzte Dateien (Auszug)
- `src/core/UBPresentationManager.{h,cpp}`
- `src/core/UBAudienceToolState.{h,cpp}`
- `src/gui/UBAudienceWindow.{h,cpp}`
- `src/board/UBBoardView.{h,cpp}`
- `src/core/UBApplicationController.{h,cpp}`
- `src/core/UBApplication.cpp`
- `resources/forms/mainWindow.ui`
