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

#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "core/UBAudienceToolState.h"
#include "core/UB.h"

UBAudienceWindow::UBAudienceWindow(UBBoardView* audienceView, UBAudienceToolState* toolState, QWidget* parent)
    : QMainWindow(parent)
    , mAudienceView(audienceView)
    , mToolState(toolState)
{
    setWindowFlags(Qt::FramelessWindowHint);
    setContentsMargins(0, 0, 0, 0);

    if (mAudienceView)
    {
        setCentralWidget(mAudienceView);
    }

    buildToolbar();
    connectSignals();
    syncFromToolState();
}

UBAudienceWindow::~UBAudienceWindow()
{
    if (mAudienceView)
    {
        mAudienceView->setParent(nullptr);
    }
}

void UBAudienceWindow::buildToolbar()
{
    mToolbar = addToolBar(tr("Audience Tools"));
    mToolbar->setMovable(false);
    mToolbar->setFloatable(false);
    mToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    mPenAction = mToolbar->addAction(tr("Pen"));
    mMoveAction = mToolbar->addAction(tr("Move"));
    mShapeAction = mToolbar->addAction(tr("Shape"));
    mZoomAction = mToolbar->addAction(tr("Zoom"));

    connect(mPenAction, &QAction::triggered, this, []() {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
    });
    connect(mMoveAction, &QAction::triggered, this, []() {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
    });
    connect(mShapeAction, &QAction::triggered, this, []() {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Line);
    });
    connect(mZoomAction, &QAction::triggered, this, []() {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Hand);
    });
}

void UBAudienceWindow::connectSignals()
{
    if (!mToolState)
    {
        return;
    }

    connect(mToolState, &UBAudienceToolState::changed, this, &UBAudienceWindow::syncFromToolState);
}

void UBAudienceWindow::syncFromToolState()
{
    if (!mToolState)
    {
        return;
    }

    if (mToolbar)
    {
        mToolbar->setVisible(mToolState->toolbarVisible());
    }

    if (mPenAction)
    {
        mPenAction->setEnabled(mToolState->penEnabled());
    }
    if (mMoveAction)
    {
        mMoveAction->setEnabled(mToolState->moveEnabled());
    }
    if (mShapeAction)
    {
        mShapeAction->setEnabled(mToolState->shapeEnabled());
    }
    if (mZoomAction)
    {
        mZoomAction->setEnabled(mToolState->zoomEnabled());
    }
}
