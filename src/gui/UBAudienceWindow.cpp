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

#include <QToolBar>

#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "core/UBAudienceToolState.h"
#include "core/UB.h"
#include "domain/UBGraphicsScene.h"

UBAudienceWindow::UBAudienceWindow(UBBoardController* boardController,
                                   UBAudienceToolState* toolState,
                                   QWidget* parent)
    : QMainWindow(parent)
    , mBoardController(boardController)
    , mToolState(toolState)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setContentsMargins(0, 0, 0, 0);

    // Create a dedicated display-only view. This does NOT steal mDisplayView
    // from the display manager, so normal presenter operation is unaffected.
    mOwnView = new UBBoardView(boardController,
                               UBItemLayerType::FixedBackground,
                               UBItemLayerType::Tool,
                               this,
                               /*isControl=*/false,
                               /*isDesktop=*/false);
    mOwnView->setAudienceMode(true);
    mOwnView->setAudienceToolState(toolState);
    mOwnView->setInteractive(false);
    setCentralWidget(mOwnView);

    // Show the current scene immediately.
    onActiveSceneChanged();

    buildToolbar();
    connectSignals();
    syncFromToolState();
}

UBAudienceWindow::~UBAudienceWindow() = default;

void UBAudienceWindow::onActiveSceneChanged()
{
    if (mBoardController && mBoardController->activeScene() && mOwnView)
    {
        mOwnView->setScene(mBoardController->activeScene().get());
    }
}

void UBAudienceWindow::syncViewport(UBBoardView* controlView)
{
    if (!mOwnView || !controlView)
        return;

    // Mirror the scene (already set on scene-change, but be safe).
    if (controlView->scene() && mOwnView->scene() != controlView->scene())
        mOwnView->setScene(controlView->scene().get());

    // Scale so the audience fills the window with the same content as the
    // control view.
    const QSize mySize   = mOwnView->size();
    const QSize ctrlSize = controlView->size();
    if (mySize.isEmpty() || ctrlSize.isEmpty())
        return;

    const qreal hFactor = static_cast<qreal>(mySize.height()) / ctrlSize.height();
    const qreal wFactor = static_cast<qreal>(mySize.width())  / ctrlSize.width();
    const qreal factor  = qMin(hFactor, wFactor);

    QTransform tr;
    tr.scale(factor, factor);
    mOwnView->setTransform(tr);

    // Centre on the same scene point the presenter is looking at.
    mOwnView->centerOn(controlView->mapToScene(controlView->rect().center()));
}

void UBAudienceWindow::buildToolbar()
{
    mToolbar = addToolBar(tr("Audience Tools"));
    mToolbar->setMovable(false);
    mToolbar->setFloatable(false);
    mToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

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

    // Mirror interactivity on the view.
    if (mOwnView)
        mOwnView->setInteractive(mToolState->anyInteractiveToolEnabled());
}
