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

    // KeepAspectRatioByExpanding: page fills the entire window, no black
    // bars.  A tiny sliver may be cropped when the aspect ratios differ, but
    // the audience never sees a letterbox border.
    mOwnView->fitInView(page, Qt::KeepAspectRatioByExpanding);
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

    // Fill the audience window with exactly that page region, edge to edge.
    mOwnView->fitInView(targetRect, Qt::KeepAspectRatioByExpanding);
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

    mPenAction   = mToolbar->addAction(tr("Pen"));
    mMoveAction  = mToolbar->addAction(tr("Move"));
    mShapeAction = mToolbar->addAction(tr("Shape"));
    mZoomAction  = mToolbar->addAction(tr("Zoom"));

    connect(mPenAction,   &QAction::triggered, this, [] {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
    });
    connect(mMoveAction,  &QAction::triggered, this, [] {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
    });
    connect(mShapeAction, &QAction::triggered, this, [] {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Line);
    });
    connect(mZoomAction,  &QAction::triggered, this, [] {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Hand);
    });

    // Bottom edge — less intrusive during the presentation.
    addToolBar(Qt::BottomToolBarArea, mToolbar);
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

    if (mPenAction)   mPenAction->setEnabled(mToolState->penEnabled());
    if (mMoveAction)  mMoveAction->setEnabled(mToolState->moveEnabled());
    if (mShapeAction) mShapeAction->setEnabled(mToolState->shapeEnabled());
    if (mZoomAction)  mZoomAction->setEnabled(mToolState->zoomEnabled());

    if (mOwnView)
        mOwnView->setInteractive(mToolState->anyInteractiveToolEnabled());
}
