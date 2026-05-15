# Tracking: Dual-Screen Interactive Presentation Mode

Regel: Ein Punkt wird nur dann als abgeschlossen markiert, wenn er zu 100% umgesetzt, integriert und verifiziert ist.

## 1) Architektur & Kernzustand
- [ ] Presentation Manager als zentrale Steuerung spezifizieren und integrieren (Start/Stop, Screen-Zuweisung, Sync)
- [ ] Shared Application State für Presenter- und Audience-Window klar definieren und anbinden
- [ ] Audience Tool State als zentrale Struktur einführen (Tool-Verfügbarkeit für Audience)
- [ ] Synchronisationsmodi für Viewport-Logik definieren und implementieren (Follow Mode, Free Mode)

## 2) Fenster & Bildschirmzuweisung
- [ ] Startlogik umsetzen: aktuelles Hauptfenster = Presenter-Screen, zweiter Screen = Audience-Screen
- [ ] Fallback auf Single-Screen-Modus implementieren
- [ ] Optionale Bildschirmauswahl vorbereiten (Dialog/Wechselmechanismus)
- [ ] Audience-Window als eigenes Qt-Fenster aufbauen (Fullscreen standardmäßig)

## 3) Rendering
- [ ] Presenter-Renderer unverändert für kompletten Workspace nutzbar halten
- [ ] Audience-Renderer-Modus implementieren: ausschließlich Seitenbereich rendern
- [ ] Hartes Clipping auf Seitenrechteck sicherstellen (kein Backstage-Leak)
- [ ] Sicherstellen, dass kein separater Dokument- oder Canvas-State für Audience entsteht

## 4) Interaktion & Werkzeuge
- [ ] Audience-Toolbar als separates UI-Konzept implementieren
- [ ] Dynamische Tool-Freigabe per Presenter-Toggles umsetzen (kein Rollen/Rechtesystem)
- [ ] Tool-Deaktivierung strikt durchsetzen (deaktivierte Tools funktional gesperrt)
- [ ] Audience-Interaktionen direkt auf Shared Canvas-State anwenden (live sichtbar beim Presenter)
- [ ] Seitenwechsel und Präsentationsfluss exklusiv durch Presenter kontrollierbar halten

## 5) Zoom, Pan & Fokussteuerung
- [ ] Presenter: freies Zoomen/Pannen innerhalb des bestehenden OpenBoard-Workflows
- [ ] Audience: Zoom/Pan nur innerhalb der Seite und nur wenn freigegeben
- [ ] Follow Mode als Standard aktivieren (Audience folgt Presenter-Viewport)
- [ ] Free Mode implementieren (Audience-Viewport unabhängig)
- [ ] Reset/Focus-Funktion umsetzen (Audience auf Fit-to-Page/Seitenfokus zurücksetzen)

## 6) Presenter Control Panel
- [ ] Neues Presenter-Control-Panel integrieren (Start/Stop, Tool-Toggles, Seitennavigation)
- [ ] Optionale Controls vorbereiten (Audience-Freeze, Bildschirmzuweisung, Audience-Vorschau)
- [ ] Live-Aktualisierung aller Presenter-Änderungen in Audience-Ansicht sicherstellen

## 7) Qualität, Stabilität, Sicherheit
- [ ] Keine Netzwerkarchitektur einführen (kein WebSocket, kein Server, kein Multi-Client)
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
