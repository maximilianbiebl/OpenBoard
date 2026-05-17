/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#include "UBAudienceWindow.h"

#include <QActionGroup>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolBar>

#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "core/UBAudienceToolState.h"
#include "core/UB.h"
#include "domain/UBGraphicsScene.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

UBAudienceWindow::UBAudienceWindow(UBBoardController* boardController,
                                   UBAudienceToolState* toolState,
                                   QWidget* parent)
    : QMainWindow(parent)
    , mBoardController(boardController)
    , mToolState(toolState)
{
    // Frameless fullscreen window — the OS chrome must not appear on the
    // audience screen.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    // Never steal keyboard focus from the presenter window.
    setAttribute(Qt::WA_ShowWithoutActivating, true);

    // Black background so any gap between page and window edge is invisible.
    setStyleSheet("QMainWindow { background: black; }");

    // Create a dedicated board view for the audience.
    // Crucially this does NOT touch the display manager's view.
    mOwnView = new UBBoardView(boardController,
                               UBItemLayerType::FixedBackground,
                               UBItemLayerType::Tool,
                               this,
                               /*isControl=*/false,
                               /*isDesktop=*/false);

    // Audience mode: clips rendering to the page rect and enforces tool gating.
    mOwnView->setAudienceMode(true);
    mOwnView->setAudienceToolState(toolState);
    mOwnView->setInteractive(false);
    // Audience view may not take keyboard focus — all keystrokes belong to the presenter.
    mOwnView->setFocusPolicy(Qt::NoFocus);

    // Remove all scroll bars — the view is always fitted to the window.
    mOwnView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mOwnView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mOwnView->setFrameShape(QFrame::NoFrame);
    mOwnView->setStyleSheet("background: black;");

    setCentralWidget(mOwnView);

    buildToolbar();
    connectSignals();
    syncFromToolState();

    // Load the current scene.  fitPage() is intentionally NOT called here
    // because the window has no size yet; showEvent() will call it once the
    // window is actually visible and sized.
    onActiveSceneChanged();
}

UBAudienceWindow::~UBAudienceWindow() = default;

// ---------------------------------------------------------------------------
// Qt event overrides
// ---------------------------------------------------------------------------

void UBAudienceWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // The window just became visible with its real geometry — now fit the page.
    fitPage();
}

void UBAudienceWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    // Window was resized (e.g. fullscreen → new resolution) — refit the page.
    fitPage();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UBAudienceWindow::fitPage()
{
    if (!mOwnView)
        return;

    const QRectF page = pageRectInScene();
    if (page.isEmpty() || mOwnView->size().isEmpty())
        return;

    // KeepAspectRatio ensures the entire slide is always visible.
    // Black letterbox bars may appear when aspect ratios differ — that is
    // correct; the audience must ALWAYS see the full page content.
    mOwnView->fitInView(page, Qt::KeepAspectRatio);
}

void UBAudienceWindow::syncViewport(UBBoardView* controlView)
{
    if (!mOwnView || !controlView)
        return;

    // Sync scene reference (page changes are also handled by onActiveSceneChanged,
    // but be safe in case the signal arrives slightly out of order).
    auto scene = controlView->scene();
    if (scene && mOwnView->scene() != scene)
        mOwnView->setScene(scene.get());

    if (mOwnView->size().isEmpty())
        return;

    const QRectF pageRect = pageRectInScene();
    if (pageRect.isEmpty())
        return;

    // Compute which part of the scene the presenter is currently looking at.
    const QRectF presenterView =
        controlView->mapToScene(controlView->viewport()->rect()).boundingRect();

    // Clamp to the page — the audience must never see backstage content.
    QRectF targetRect = presenterView.intersected(pageRect);

    // If the presenter is entirely outside the page (editing backstage),
    // fall back to showing the full page.
    if (targetRect.isEmpty())
        targetRect = pageRect;

    // Show that page region; KeepAspectRatio keeps full content visible.
    mOwnView->fitInView(targetRect, Qt::KeepAspectRatio);
}

void UBAudienceWindow::zoomIn()
{
    if (!mOwnView) return;
    const QRectF page = pageRectInScene();
    mOwnView->scale(1.25, 1.25);
    if (!page.isEmpty())
    {
        QPointF c = mOwnView->mapToScene(mOwnView->viewport()->rect().center());
        c.setX(qBound(page.left(), c.x(), page.right()));
        c.setY(qBound(page.top(),  c.y(), page.bottom()));
        mOwnView->centerOn(c);
    }
}

void UBAudienceWindow::zoomOut()
{
    if (!mOwnView) return;
    const QRectF page = pageRectInScene();
    if (page.isEmpty()) return;

    // From the normal (fit-to-page) view the audience may only zoom IN.
    // If already showing the full page or more, zoom-out does nothing.
    const QRectF visible = mOwnView->mapToScene(mOwnView->viewport()->rect()).boundingRect();
    if (visible.width() >= page.width() * 0.99 || visible.height() >= page.height() * 0.99)
        return;

    mOwnView->scale(0.8, 0.8);

    const QRectF newVisible = mOwnView->mapToScene(mOwnView->viewport()->rect()).boundingRect();
    if (newVisible.width() >= page.width() || newVisible.height() >= page.height())
    {
        fitPage();
        return;
    }
    QPointF c = mOwnView->mapToScene(mOwnView->viewport()->rect().center());
    c.setX(qBound(page.left(), c.x(), page.right()));
    c.setY(qBound(page.top(),  c.y(), page.bottom()));
    mOwnView->centerOn(c);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QRectF UBAudienceWindow::pageRectInScene() const
{
    if (!mOwnView)
        return {};

    auto scene = mOwnView->scene();
    if (!scene)
        return {};

    const QSize sz = scene->nominalSize();
    if (sz.isEmpty())
        return {};

    // OpenBoard pages are centred at the scene origin.
    return QRectF(sz.width()  / -2.0,
                  sz.height() / -2.0,
                  sz.width(),
                  sz.height());
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void UBAudienceWindow::buildToolbar()
{
    mToolbar = new QToolBar(tr("Audience Tools"), this);
    mToolbar->setMovable(false);
    mToolbar->setFloatable(false);
    mToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mToolbar->setIconSize(QSize(36, 36));

    // Style: translucent dark band that sits at the bottom of the screen
    // without distracting from the slide content.
    mToolbar->setStyleSheet(
        "QToolBar {"
        "  background: rgba(30,30,30,210);"
        "  border: none;"
        "  spacing: 12px;"
        "  padding: 4px 8px;"
        "}"
        "QToolButton {"
        "  color: white;"
        "  font-size: 13px;"
        "  min-width: 72px;"
        "  padding: 6px 10px;"
        "  border-radius: 6px;"
        "}"
        "QToolButton:hover  { background: rgba(255,255,255,30); }"
        "QToolButton:pressed{ background: rgba(255,255,255,60); }"
        "QToolButton:disabled{ color: rgba(255,255,255,80); }");

    // Tool buttons — icons match the presenter's stylus palette for consistency.
    auto makeToolAction = [this](const QString& off, const QString& on, const QString& label) -> QAction* {
        QIcon icon;
        icon.addFile(off, QSize(), QIcon::Normal, QIcon::Off);
        icon.addFile(on,  QSize(), QIcon::Normal, QIcon::On);
        return mToolbar->addAction(icon, label);
    };

    mPenAction    = makeToolAction(":images/stylusPalette/pen.png",    ":images/stylusPalette/penOn.png",    tr("Pen"));
    mMarkerAction = makeToolAction(":images/stylusPalette/marker.png", ":images/stylusPalette/markerOn.png", tr("Marker"));
    mEraserAction = makeToolAction(":images/stylusPalette/eraser.png", ":images/stylusPalette/eraserOn.png", tr("Eraser"));
    mMoveAction   = makeToolAction(":images/stylusPalette/arrow.png",  ":images/stylusPalette/arrowOn.png",  tr("Select"));
    mShapeAction  = makeToolAction(":images/stylusPalette/line.png",   ":images/stylusPalette/lineOn.png",   tr("Shape"));
    mZoomAction   = makeToolAction(":images/stylusPalette/hand.png",   ":images/stylusPalette/handOn.png",   tr("Pan/Zoom"));
    mLaserAction  = makeToolAction(":images/stylusPalette/laser.png",  ":images/stylusPalette/laserOn.png",  tr("Laser"));

    // Checkable + exclusive so exactly one tool appears active.
    auto* toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    for (auto* a : {mPenAction, mMarkerAction, mEraserAction, mMoveAction, mShapeAction, mZoomAction, mLaserAction})
    {
        a->setCheckable(true);
        toolGroup->addAction(a);
    }
    mPenAction->setChecked(true);

    connect(mPenAction,    &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);      });
    connect(mMarkerAction, &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Marker);    });
    connect(mEraserAction, &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Eraser);    });
    connect(mMoveAction,   &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);  });
    connect(mShapeAction,  &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Line);      });
    connect(mZoomAction,   &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Hand);      });
    connect(mLaserAction,  &QAction::triggered, this, [] { UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pointer);    });

    // Keep toolbar in sync when the global stylus tool changes (e.g. hardware stylus or presenter).
    connect(UBDrawingController::drawingController(), &UBDrawingController::stylusToolChanged,
            this, [this](int tool) {
                struct { QAction* action; std::initializer_list<int> tools; } map[] = {
                    { mPenAction,    { UBStylusTool::Pen } },
                    { mMarkerAction, { UBStylusTool::Marker } },
                    { mEraserAction, { UBStylusTool::Eraser } },
                    { mMoveAction,   { UBStylusTool::Selector, UBStylusTool::Play } },
                    { mShapeAction,  { UBStylusTool::Line } },
                    { mZoomAction,   { UBStylusTool::Hand, UBStylusTool::ZoomIn, UBStylusTool::ZoomOut } },
                    { mLaserAction,  { UBStylusTool::Pointer } },
                };
                for (auto& entry : map)
                    for (int t : entry.tools)
                        if (tool == t) { QSignalBlocker b(entry.action); entry.action->setChecked(true); return; }
            });

    // Zoom buttons — always available regardless of tool state.
    mToolbar->addSeparator();
    mZoomOutAction = mToolbar->addAction(tr("−  Zoom"), this, &UBAudienceWindow::zoomOut);
    mFitPageAction = mToolbar->addAction(tr("⊡  Fit"),  this, &UBAudienceWindow::fitPage);
    mZoomInAction  = mToolbar->addAction(tr("+  Zoom"), this, &UBAudienceWindow::zoomIn);

    // Bottom edge — less intrusive during the presentation.
    addToolBar(Qt::BottomToolBarArea, mToolbar);

    // Prevent toolbar buttons from stealing keyboard focus so the presenter
    // can keep using keyboard shortcuts on their own screen.
    mToolbar->setFocusPolicy(Qt::NoFocus);
    for (QWidget* w : mToolbar->findChildren<QWidget*>())
        w->setFocusPolicy(Qt::NoFocus);
}

void UBAudienceWindow::connectSignals()
{
    if (mToolState)
        connect(mToolState, &UBAudienceToolState::changed,
                this, &UBAudienceWindow::syncFromToolState);

    if (mBoardController)
        connect(mBoardController, &UBBoardController::activeSceneChanged,
                this, &UBAudienceWindow::onActiveSceneChanged);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void UBAudienceWindow::onActiveSceneChanged()
{
    if (!mBoardController || !mBoardController->activeScene() || !mOwnView)
        return;

    mOwnView->setScene(mBoardController->activeScene().get());

    // Re-fit immediately so the new page fills the window.
    fitPage();
}

void UBAudienceWindow::syncFromToolState()
{
    if (!mToolState)
        return;

    if (mToolbar)
        mToolbar->setVisible(mToolState->toolbarVisible());

    if (mPenAction)    mPenAction->setEnabled(mToolState->penEnabled());
    if (mMarkerAction) mMarkerAction->setEnabled(mToolState->penEnabled());
    if (mEraserAction) mEraserAction->setEnabled(mToolState->penEnabled());
    if (mMoveAction)   mMoveAction->setEnabled(mToolState->moveEnabled());
    if (mShapeAction)  mShapeAction->setEnabled(mToolState->shapeEnabled());
    if (mZoomAction)   mZoomAction->setEnabled(mToolState->zoomEnabled());
    if (mLaserAction)  mLaserAction->setEnabled(mToolState->zoomEnabled());

    if (mOwnView)
        mOwnView->setInteractive(mToolState->anyInteractiveToolEnabled());
}
