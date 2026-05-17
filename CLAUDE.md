# BoardPresenter — Developer Notes for Claude

## Build target
- **Compiler**: MSVC 2022 (cl.exe), C++20, Qt 6.7.3
- **Platform**: Windows 11 x64
- **Build system**: qmake → nmake
- **Branch for new work**: `claude/dual-screen-presentation-mode-krZSW`

## Build script names — IMPORTANT
- The `.pro` file stays `OpenBoard.pro` (qmake reads it by filename — do NOT rename).
- The compiled executable is `BoardPresenter.exe` (set via `TARGET` in `OpenBoard.pro`).
- `release_scripts/windows/release.win7.vc9.bat` — `APPLICATION_NAME=BoardPresenter`
- `release_scripts/windows/create-setup.bat` — `APPLICATION_NAME=BoardPresenter`
- `OpenBoard-ThirdParty` — separate repo name, unchanged.
- **Rule**: whenever `TARGET` in `.pro` changes, update `APPLICATION_NAME` in both `.bat` files.

## MSVC-specific rules — check before every commit

### 1. `const` correctness
- A `const` method **cannot** call a non-`const` method on `*this` or a base class.
- `UBDocumentContainer::selectedDocument()` is **non-const** — any method calling it must be non-const.
- `UBGraphicsScene::nominalSize()` is **non-const** — same rule.
- Before marking any new method `const`, grep the body for calls to non-const methods.

### 2. `auto` with Qt containers
- `auto x = someQMap.value(key)` — fine.  
- `auto& x = someQMap[key]` — inserts a default entry; use `.value()` for read-only.

### 3. Range-for over brace-initializer
```cpp
// OK — consistent with existing code patterns in this project
for (auto* b : {ptrA, ptrB}) { ... }
```
MSVC C++20 handles this correctly as long as all elements have the same type.

### 4. Integer arithmetic
- `QSize::width()` / `QSize::height()` return `int`.
- `qRound()` returns `int`.
- Mixing with `qreal`/`double` in `qMax()` — annotate with explicit cast: `qMax(200, value)` where both are `int` is fine; if one is `qreal`, cast explicitly.

### 5. `enum class` in switch
- Always include `default:` or all enum values in `switch` on a scoped enum to silence MSVC C4062.

### 6. `QGraphicsView::mapFromScene(QPointF)` returns `QPoint` (integer), not `QPointF`.

## Architecture overview

### Dual-screen presentation mode
| Class | File | Role |
|---|---|---|
| `UBPresentationManager` | `src/core/UBPresentationManager.*` | Presenter dock panel + orchestration |
| `UBAudienceWindow` | `src/gui/UBAudienceWindow.*` | Fullscreen audience window (owns its own `UBBoardView`) |
| `UBAudienceToolState` | `src/core/UBAudienceToolState.*` | Per-tool enable flags, gated by presenter |
| `UBBoardView` | `src/board/UBBoardView.*` | Shared canvas view; `setAudienceMode(true)` for audience |

### Page naming
- Names stored in `toc.json` via `UBDocumentToc::pageName/setPageName`.
- `UBBoardController::pageName/setPageName` is the public entry point.
- `UBDocumentNavigator`: double-click thumbnail label → `QInputDialog` → `setPageName`.

### Background types (`UBPageBackground` enum, `src/core/UB.h`)
| Value | Description |
|---|---|
| `plain` | Blank white/black |
| `ruled` | Horizontal lines |
| `crossed` | Full grid |
| `dotted` | Dots at intersections |

Persisted via `UBSvgSubsetAdaptor` as boolean XML attributes:  
`crossed-background`, `ruled-background`, `dotted-background`.

### Page resize via border drag
- `UBBoardView::detectPageResizeEdge()` — 12 px detection zone on right/bottom border.
- `UBBoardView::applyPageResizeCursor()` — shows SizeHor/SizeVer/SizeFDiag.
- On release: calls `UBBoardController::setPageSize()` (undoable).
- Only active in **control view** (`bIsControl == true`), never in audience mode.

## Key non-obvious behaviors
- `UBAudienceWindow` has `Qt::WA_ShowWithoutActivating` — it never steals keyboard focus.
- `mDisplayView` (the old second-screen view) is always hidden; only `UBAudienceWindow` uses screen 2.
- Arrow key page navigation is blocked in audience mode via `if (mAudienceMode) { event->ignore(); return; }`.
- Background buttons and page controls are only in the presenter dock — audience screen has no such controls.
