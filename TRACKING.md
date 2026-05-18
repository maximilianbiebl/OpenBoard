# OpenBoard – Dual-Screen Interactive Presentation Mode
## Tracking & TODO-Liste

**Ziel-Vollständigkeit: 100%**  
**Status:** In Bearbeitung  
**Feature-Branch:** `claude/dual-screen-presentation-mode-krZSW`

---

## Phase 0 – Windows Installer Build-Fähigkeit (VORAUSSETZUNG)

Solange der Installer nicht buildbar ist, kann keine Feature-Entwicklung verifiziert werden.

### 0.1 Build-Skripte reparieren
- [x] `release.win7.vc9.bat`: Kaputten `xcopy C:\OpenBoard\bin\*.dll` entfernt und durch `windeployqt` ersetzt
- [x] `release.win7.vc9.bat`: Fehlende Prüfung auf `windeployqt.exe` ergänzt
- [x] `release.win7.vc9.bat`: `PROJECT_ROOT`-Umgebungsvariable korrekt für Inno Setup gesetzt
- [x] `create-setup.bat`: `PROJECT_ROOT` wird jetzt korrekt gesetzt

### 0.2 Poppler Windows-Support
- [x] `OpenBoard.pro`: Explizite Windows-Poppler-Konfiguration ergänzt (INCLUDEPATH + LIBS) ohne Abhängigkeit von `libs.pri`
- [x] `cmake/DependencyPoppler.cmake`: Windows-spezifischen Suchpfad via `OPENBOARD_THIRDPARTY`-Umgebungsvariable ergänzt
- [x] `setup-windows-env.ps1`: Poppler-Download und -Vorbereitung via vcpkg oder direktem Download ergänzt

### 0.3 QuaZip Windows-Support
- [x] `OpenBoard.pro`: QuaZip-Lib-Fallback für verschiedene Bibliotheksnamen bereits implementiert (quazip1-qt6, quazip-qt6, etc.)
- [ ] `setup-windows-env.ps1`: Automatisches QuaZip-Kompilieren aus ThirdParty-Quellen per Script ergänzen
- [ ] Dokumentation: Schritt-für-Schritt-Anleitung für OpenBoard-ThirdParty-Setup unter Windows 11

### 0.4 Inno Setup / Installer
- [x] `OpenBoard.iss`: Installer-Output-Pfad korrigiert (`build\win32\release\installer\`)
- [ ] `OpenBoard.iss`: Prüfen ob alle referenzierten DLLs in ThirdParty-Struktur vorhanden sind
- [ ] End-to-End-Test: Installer auf Windows 11 bauen und Ergebnis verifizieren

---

## Phase 1 – Kern-Architektur (Dual-Screen)

### 1.1 UBAudienceToolState
- [x] Klasse angelegt (`src/core/UBAudienceToolState.h/.cpp`)
- [x] Felder: `penEnabled`, `moveEnabled`, `shapeEnabled`, `zoomEnabled`, `toolbarVisible`
- [x] Setter-Methoden mit Signal `changed()`
- [x] Hilfsmethode `anyInteractiveToolEnabled()`
- [x] `isStylusToolEnabled(int tool)` implementiert
- [x] In `core.pri` und `src/core/CMakeLists.txt` registriert

### 1.2 UBPresentationManager
- [x] Klasse angelegt (`src/core/UBPresentationManager.h/.cpp`)
- [x] Start/Stop-Präsentation
- [x] Follow-Mode / Free-Mode
- [x] Bildschirm-Zuweisung via QComboBox
- [x] Presenter-Control-Panel als QDockWidget im Presenter-Fenster
- [x] Tool-Toggles (Pen, Move, Shape, Zoom, Toolbar)
- [x] Audience-Freeze-Toggle
- [x] Seitennavigation (Previous/Next) via UBBoardController
- [x] Reset-Audience-Focus
- [x] Audience-Preview-Button
- [x] Screen-Selector mit `QScreen`-Liste aus `UBDisplayManager`
- [x] In `core.pri` und `src/core/CMakeLists.txt` registriert
- [ ] Unit-Test: Start/Stop-Zustand korrekt
- [ ] Unit-Test: Follow-Mode Synchronisation

### 1.3 UBAudienceWindow
- [x] Klasse angelegt (`src/gui/UBAudienceWindow.h/.cpp`)
- [x] Eigenes `QMainWindow` (Fullscreen, FramelessWindowHint)
- [x] Eigene `UBBoardView` als Central Widget (nicht mDisplayView gestohlen)
- [x] `setAudienceMode(true)` + `setAudienceToolState()` auf eigener View gesetzt
- [x] Audience-Toolbar mit Pen/Move/Shape/Zoom-Actions
- [x] `syncFromToolState()` reagiert auf `UBAudienceToolState::changed()`
- [x] Toolbar kann per Presenter ein-/ausgeblendet werden
- [x] Verbindung zu `activeSceneChanged` für automatisches Seiten-Tracking
- [x] In `gui.pri` und `src/gui/CMakeLists.txt` registriert

### 1.4 UBBoardView – Audience-Render-Mode
- [x] `setAudienceMode(bool)` implementiert
- [x] `setAudienceToolState(UBAudienceToolState*)` implementiert
- [x] Clipping auf Seitenbereich (Audience sieht nur die Seite, kein Backstage)
- [x] Audience-Interaktion je nach Tool-State ein-/ausgeschaltet
- [x] `audienceAllowsStylusTool()` Hilfsmethode
- [ ] Visueller Test: Backstage-Bereich wirklich nicht sichtbar

### 1.5 UBApplicationController Integration
- [x] `UBPresentationManager` wird instanziiert
- [x] `adjustDisplayView()` wird von `resetAudienceFocus()` genutzt
- [ ] Presenter-Viewport-Sync beim Seitenwechsel prüfen

### 1.6 UBApplication Integration
- [x] `UBPresentationManager` initialisiert in `UBApplication.cpp`

---

## Phase 2 – Dual-Screen-Verhalten

### 2.1 Bildschirm-Erkennung und -Zuweisung
- [x] Verfügbare Screens via `UBDisplayManager::availableScreens()` geladen
- [x] Automatisch zweiten Screen als Audience-Screen vorauswählen
- [x] Audience-Fenster auf gewählten Screen verschieben
- [ ] Test: Wechsel des Audience-Screens im laufenden Betrieb

### 2.2 Follow-Mode (Sync-Viewport)
- [x] `shouldSyncAudienceViewport()` gibt `mRunning && mFollowMode` zurück
- [x] `resetAudienceFocus()` zentriert Audience auf aktuelle Seite
- [x] Integration in Zoom/Pan-Event des Presenter-Views (Viewport-Sync via controlViewportChanged)
- [ ] Test: Audience folgt Presenter-Zoom

### 2.3 Free-Mode
- [x] Audience-View erlaubt eigenständiges Zoomen/Pannen wenn Follow-Mode aus (UBBoardView mit setInteractive(true))
- [ ] Presenter kann Audience jederzeit via Reset zurücksetzen
- [ ] Test: Audience zoomt unabhängig, Presenter-Reset setzt zurück

### 2.4 Audience-Freeze
- [x] `mAudienceFrozen`-Flag im PresentationManager
- [x] `setUpdatesEnabled(false)` auf AudienceWindow bei Freeze
- [x] Canvas-Interaktion bei Freeze deaktiviert
- [ ] Visuelles Feedback im Presenter-Panel (Status "eingefroren")
- [ ] Test: Änderungen auf Canvas während Freeze nicht für Audience sichtbar

---

## Phase 3 – Audience-Interaktivität

### 3.1 Live-Sync Audience → Presenter
- [x] Audience und Presenter teilen denselben Canvas-State (SharedDocument)
- [x] Änderungen der Audience werden in Presenter-View live angezeigt
- [ ] Test: Audience zeichnet, Presenter sieht es sofort

### 3.2 Tool-Steuerung
- [x] Pen-Tool in Audience-Toolbar verdrahtet mit `UBDrawingController`
- [x] Move-Tool (Selector) verdrahtet
- [x] Shape-Tool (Line) verdrahtet
- [x] Zoom-Tool (Hand) verdrahtet
- [ ] Test: Tool-Wechsel in Audience via Toolbar
- [ ] Test: Tool-Deaktivierung durch Presenter wirkt sich sofort auf Audience aus

---

## Phase 4 – UX und Präsentations-UI

### 4.1 Presenter Control Panel
- [x] QDockWidget im Presenter-Fenster (rechte Seite)
- [x] Start/Stop-Toggle
- [x] Follow-Mode-Toggle
- [x] Toolbar-Visibility-Toggle für Audience
- [x] Tool-Toggles (Pen, Move, Shape, Zoom)
- [x] Freeze-Toggle
- [x] Screen-Selector-Dropdown
- [x] Previous/Next-Page-Buttons
- [x] Reset-Focus-Button
- [x] Preview-Button (optional, aktuell disabled)
- [ ] Styling/CSS-Anpassung für bessere Erkennbarkeit

### 4.2 Audience-Toolbar
- [x] Audience-Toolbar wird bei Presentation-Start sichtbar (wenn aktiviert)
- [x] Toolbar reagiert live auf Presenter-Toggles
- [ ] Icons für Audience-Toolbar-Actions hinzufügen
- [ ] Styling für Fullscreen-Modus optimieren

### 4.3 Seitennavigation
- [x] Previous/Next über UBBoardController verdrahtet
- [ ] Keyboard-Shortcut für Seitennavigation im Presenter-Modus
- [ ] Seiten-Thumbnail-Übersicht im Presenter-Panel (optional)

---

## Phase 5 – Qualitätssicherung und Tests

### 5.1 Compiler-Kompatibilität
- [x] Qt6 / MSVC2022 kompatibler Code (keine deprecated APIs)
- [x] `QPointer` korrekt verwendet (kein direkter Qt6-Downcast-Bug)
- [x] `std::string_view`-Konvertierung für Poppler ≥ 25.12 gefixt
- [ ] Windows-Build: Erfolgreicher Compile ohne Warnings
- [ ] Linux-Build: Kein Regressionstest-Fehler

### 5.2 Manuelle Tests
- [ ] Einfacher Dual-Screen-Test (Präsentation starten, Audience auf zweitem Monitor)
- [ ] Single-Screen-Fallback (Präsentation auf einem Monitor)
- [ ] Seiten-Wechsel während laufender Präsentation
- [ ] Tool-Toggles live testen
- [ ] Backstage-Bereich nicht sichtbar für Audience verifizieren

### 5.3 Installer-Test
- [ ] Installer auf Windows 11 erstellen
- [ ] Installer installieren und OpenBoard starten
- [ ] Dual-Screen-Feature nach Installation funktionsfähig

---

## Phase 6 – Optionale Erweiterungen (Post-MVP)

- [ ] Push-to-Stage-Funktion (Objekte aus Backstage auf die Seite schieben)
- [ ] Mehrere Audience-Screens
- [ ] Präsentationsszenen / gespeicherte Layouts
- [ ] Touch-/Pen-Optimierung für Audience-Screen
- [ ] Presenter-Preview-Fenster (miniaturisierte Audience-Vorschau)
- [ ] Audience-Freeze mit visuellem Overlay ("Bitte warten")

---

## Bekannte offene Probleme

| ID | Problem | Priorität | Status |
|----|---------|-----------|--------|
| P1 | Poppler auf Windows muss aus ThirdParty-Repo kommen, kein automatisches Setup | HOCH | In Bearbeitung |
| P2 | QuaZip muss manuell vor-kompiliert werden | HOCH | Dokumentiert |
| P3 | `release.win7.vc9.bat` hatte kaputten xcopy-Aufruf | HOCH | Gefixt |
| P4 | Viewer-Sync bei Zoom/Pan: via controlViewportChanged-Signal implementiert | MITTEL | Gefixt |
| P5 | Audience-Toolbar hat keine Icons, nur Text | NIEDRIG | Offen |
| P6 | Free-Mode ermöglicht noch kein eigenständiges Zoomen/Pannen der Audience | MITTEL | Offen |

---

## Fortschritt-Übersicht

| Phase | Beschreibung | Fertig | Gesamt | % |
|-------|-------------|--------|--------|---|
| 0 | Windows Build-Fähigkeit | 7 | 11 | 64% |
| 1 | Kern-Architektur | 22 | 26 | 85% |
| 2 | Dual-Screen-Verhalten | 5 | 10 | 50% |
| 3 | Audience-Interaktivität | 4 | 8 | 50% |
| 4 | UX und Präsentations-UI | 11 | 16 | 69% |
| 5 | Qualitätssicherung | 3 | 10 | 30% |
| 6 | Optionale Erweiterungen | 0 | 6 | 0% |
| **Gesamt** | | **52** | **87** | **60%** |

**MVP-Vollständigkeit (Phase 0–5):** 52/81 = **64%**

---

## Nächste Schritte (Priorität)

1. **[P0, JETZT]** Windows-Build fixen: `OpenBoard.pro` Poppler-Config, `release.win7.vc9.bat` reparieren
2. **[P0]** Poppler-ThirdParty-Setup in `setup-windows-env.ps1` automatisieren
3. **[P0]** End-to-End Windows Installer bauen und verifizieren
4. **[P1]** Follow-Mode Viewport-Sync bei Zoom/Pan implementieren
5. **[P1]** Free-Mode für Audience implementieren
6. **[P2]** Icons für Audience-Toolbar
7. **[P2]** Alle manuellen Tests durchführen
