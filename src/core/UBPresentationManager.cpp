/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#include "UBPresentationManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWindow>

#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "core/UBApplicationController.h"
#include "core/UBAudienceToolState.h"
#include "core/UBDisplayManager.h"
#include "frameworks/UBPlatformUtils.h"
#include "gui/UBAudienceWindow.h"
#include "gui/UBMainWindow.h"

UBPresentationManager::UBPresentationManager(UBApplicationController* appController,
                                             UBBoardController* boardController,
                                             UBDisplayManager* displayManager,
                                             UBMainWindow* presenterWindow,
                                             UBBoardView* displayView,
                                             QObject* parent)
    : QObject(parent)
    , mAppController(appController)
    , mBoardController(boardController)
    , mDisplayManager(displayManager)
    , mPresenterWindow(presenterWindow)
    , mDisplayView(displayView)
{
    mAudienceToolState = new UBAudienceToolState(this);

    // UBAudienceWindow creates its OWN UBBoardView internally.
    // mDisplayView is NOT touched here — it stays with the display manager.
    mAudienceWindow = new UBAudienceWindow(mBoardController, mAudienceToolState, nullptr);

    createPresenterControls();
    connectPresenterControls();
    refreshAudienceScreenSelector();
    applyAudienceToolState();
}

UBPresentationManager::~UBPresentationManager()
{
    if (mAudienceWindow)
        mAudienceWindow->hide();
}

bool UBPresentationManager::shouldSyncAudienceViewport() const
{
    return mRunning && mFollowMode;
}

void UBPresentationManager::startPresentation()
{
    setRunning(true);
}

void UBPresentationManager::stopPresentation()
{
    setRunning(false);
}

void UBPresentationManager::setRunning(bool enabled)
{
    if (mRunning == enabled)
        return;

    mRunning = enabled;

    if (mStartStop)
    {
        QSignalBlocker blocker(mStartStop);
        mStartStop->setChecked(enabled);
    }

    applyRunningState();
}

void UBPresentationManager::setFollowMode(bool follow)
{
    mFollowMode = follow;

    if (mFollowMode)
        updateAudienceViewFrame();
}

void UBPresentationManager::resetAudienceFocus()
{
    if (!mRunning || !mAudienceWindow)
        return;

    mAudienceWindow->fitPage();
}

void UBPresentationManager::createPresenterControls()
{
    if (!mPresenterWindow)
        return;

    mPresenterPanel = new QDockWidget(tr("Presentation Control"), mPresenterWindow);
    mPresenterPanel->setObjectName("presentationControlPanel");

    QWidget* panel = new QWidget(mPresenterPanel);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);

    mStartStop            = new QCheckBox(tr("Presentation running"));
    mFollowModeToggle     = new QCheckBox(tr("Follow mode"));
    mAudienceToolbarToggle = new QCheckBox(tr("Audience toolbar visible"));
    mPenToggle            = new QCheckBox(tr("Pen enabled"));
    mMoveToggle           = new QCheckBox(tr("Move enabled"));
    mShapeToggle          = new QCheckBox(tr("Shape enabled"));
    mZoomToggle           = new QCheckBox(tr("Zoom/Pan enabled"));
    mFreezeAudienceToggle = new QCheckBox(tr("Freeze audience view"));
    mAudienceScreenSelector = new QComboBox();
    mPreviousPageButton   = new QPushButton(tr("◀  Previous page"));
    mNextPageButton       = new QPushButton(tr("Next page  ▶"));
    mAudiencePreviewButton = new QPushButton(tr("Bring audience window to front"));
    mResetFocusButton     = new QPushButton(tr("Reset audience focus"));

    mFollowModeToggle->setChecked(true);
    mAudienceToolbarToggle->setChecked(true);
    mPenToggle->setChecked(true);
    mMoveToggle->setChecked(true);
    mShapeToggle->setChecked(true);
    mZoomToggle->setChecked(true);
    mAudiencePreviewButton->setEnabled(false);

    layout->addWidget(mStartStop);
    layout->addWidget(mFollowModeToggle);
    layout->addWidget(mAudienceToolbarToggle);
    layout->addSpacing(8);
    layout->addWidget(new QLabel(tr("Audience tools")));
    layout->addWidget(mPenToggle);
    layout->addWidget(mMoveToggle);
    layout->addWidget(mShapeToggle);
    layout->addWidget(mZoomToggle);
    layout->addWidget(mFreezeAudienceToggle);
    layout->addSpacing(8);
    layout->addWidget(new QLabel(tr("Page navigation")));
    auto* navLayout = new QHBoxLayout();
    navLayout->addWidget(mPreviousPageButton);
    navLayout->addWidget(mNextPageButton);
    layout->addLayout(navLayout);
    layout->addSpacing(8);
    layout->addWidget(new QLabel(tr("Audience screen")));
    layout->addWidget(mAudienceScreenSelector);
    layout->addWidget(mAudiencePreviewButton);
    layout->addWidget(mResetFocusButton);
    layout->addStretch();

    mPresenterPanel->setWidget(panel);
    mPresenterWindow->addDockWidget(Qt::RightDockWidgetArea, mPresenterPanel);
}

void UBPresentationManager::connectPresenterControls()
{
    if (!mPresenterPanel)
        return;

    connect(mStartStop, &QCheckBox::toggled, this, &UBPresentationManager::setRunning);
    connect(mFollowModeToggle, &QCheckBox::toggled, this, &UBPresentationManager::setFollowMode);

    connect(mAudienceToolbarToggle, &QCheckBox::toggled, this, [this](bool checked) {
        mAudienceToolState->setToolbarVisible(checked);
        applyAudienceToolState();
    });
    connect(mPenToggle, &QCheckBox::toggled, this, [this](bool checked) {
        mAudienceToolState->setPenEnabled(checked);
        applyAudienceToolState();
    });
    connect(mMoveToggle, &QCheckBox::toggled, this, [this](bool checked) {
        mAudienceToolState->setMoveEnabled(checked);
        applyAudienceToolState();
    });
    connect(mShapeToggle, &QCheckBox::toggled, this, [this](bool checked) {
        mAudienceToolState->setShapeEnabled(checked);
        applyAudienceToolState();
    });
    connect(mZoomToggle, &QCheckBox::toggled, this, [this](bool checked) {
        mAudienceToolState->setZoomEnabled(checked);
        applyAudienceToolState();
    });
    connect(mFreezeAudienceToggle, &QCheckBox::toggled, this, [this](bool checked) {
        mAudienceFrozen = checked;
        applyAudienceToolState();
    });

    connect(mPreviousPageButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->previousScene();
    });
    connect(mNextPageButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->nextScene();
    });

    connect(mAudienceScreenSelector,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { applyAudienceScreenSelection(); });

    connect(mAudiencePreviewButton, &QPushButton::clicked, this, [this] {
        if (!mAudienceWindow || !mRunning)
            return;
        applyAudienceScreenSelection();
        mAudienceWindow->raise();
        mAudienceWindow->activateWindow();
    });

    connect(mResetFocusButton, &QPushButton::clicked,
            this, &UBPresentationManager::resetAudienceFocus);

    if (mDisplayManager)
    {
        connect(mDisplayManager,
                &UBDisplayManager::availableScreenCountChanged,
                this,
                [this](int) { refreshAudienceScreenSelector(); });
    }

    // Keep audience viewport in sync when the presenter pans/zooms (follow mode).
    if (mBoardController)
    {
        connect(mBoardController, &UBBoardController::controlViewportChanged,
                this, [this] {
                    if (mRunning && mFollowMode)
                        updateAudienceViewFrame();
                });
    }
}

void UBPresentationManager::refreshAudienceScreenSelector()
{
    if (!mAudienceScreenSelector || !mDisplayManager)
        return;

    const QList<QScreen*> screens = mDisplayManager->availableScreens();

    QSignalBlocker blocker(mAudienceScreenSelector);
    mAudienceScreenSelector->clear();

    for (int i = 0; i < screens.size(); ++i)
    {
        const QScreen* s = screens.at(i);
        mAudienceScreenSelector->addItem(
            tr("Screen %1 (%2×%3)")
                .arg(i + 1)
                .arg(s ? s->geometry().width()  : 0)
                .arg(s ? s->geometry().height() : 0),
            i);
    }

    mAudienceScreenSelector->setCurrentIndex(screens.size() > 1 ? 1 : 0);
    mAudienceScreenSelector->setEnabled(!screens.isEmpty());
}

void UBPresentationManager::applyAudienceScreenSelection()
{
    if (!mAudienceWindow || !mDisplayManager || !mRunning)
        return;

    const QList<QScreen*> screens = mDisplayManager->availableScreens();
    if (screens.isEmpty())
        return;

    int index = 0;
    if (mAudienceScreenSelector)
        index = qBound(0, mAudienceScreenSelector->currentIndex(), screens.size() - 1);

    QScreen* target = screens.at(index);
    if (!target)
        return;

    // On Windows, moving to a different screen requires hiding first,
    // then reassigning the screen handle, then calling platform showFullScreen.
    mAudienceWindow->hide();

    // Force native window handle creation so setScreen works.
    mAudienceWindow->winId();

    if (QWindow* handle = mAudienceWindow->windowHandle())
        handle->setScreen(target);

    mAudienceWindow->setGeometry(target->geometry());
    UBPlatformUtils::showFullScreen(mAudienceWindow);
}

void UBPresentationManager::applyRunningState()
{
    if (!mAudienceWindow)
        return;

    if (mRunning)
    {
        // Hide the display manager's view so the audience window is the only
        // thing visible on the second screen.
        if (mDisplayView)
            mDisplayView->hide();

        if (mAudiencePreviewButton)
            mAudiencePreviewButton->setEnabled(true);

        applyAudienceToolState();
        applyAudienceScreenSelection();
        updateAudienceViewFrame();
    }
    else
    {
        mAudienceWindow->hide();

        // Restore the display manager's normal view.
        if (mDisplayView)
            mDisplayView->show();

        if (mAudiencePreviewButton)
            mAudiencePreviewButton->setEnabled(false);
    }
}

void UBPresentationManager::applyAudienceToolState()
{
    if (!mAudienceWindow)
        return;

    mAudienceWindow->syncFromToolState();

    if (UBBoardView* v = mAudienceWindow->boardView())
    {
        const bool canInteract = mRunning && !mAudienceFrozen
                                 && mAudienceToolState->anyInteractiveToolEnabled();
        v->setInteractive(canInteract);
        v->setEnabled(!mAudienceFrozen);
        mAudienceWindow->setUpdatesEnabled(!mAudienceFrozen);
    }
}

void UBPresentationManager::updateAudienceViewFrame()
{
    if (!mRunning || !mFollowMode || !mBoardController || !mAudienceWindow)
        return;

    if (UBBoardView* controlView = mBoardController->controlView())
        mAudienceWindow->syncViewport(controlView);
}
