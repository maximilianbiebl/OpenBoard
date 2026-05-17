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

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QComboBox>
#include <QDockWidget>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QWindow>

#include "board/UBBoardController.h"
#include "board/UBBoardView.h"
#include "core/UB.h"
#include "document/UBDocumentProxy.h"
#include "core/UBApplicationController.h"
#include "core/UBAudienceToolState.h"
#include "core/UBDisplayManager.h"
#include "frameworks/UBPlatformUtils.h"
#include "gui/UBAudienceWindow.h"
#include "gui/UBMainWindow.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

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

    // UBAudienceWindow owns its own UBBoardView — mDisplayView is untouched.
    mAudienceWindow = new UBAudienceWindow(mBoardController, mAudienceToolState, nullptr);

    createPresenterControls();
    connectPresenterControls();
    refreshAudienceScreenSelector();

    // Apply the initial tool state so UBAudienceWindow and its toolbar are
    // already in the correct state before the first presentation starts.
    applyAudienceToolState();

    // Always hide the display manager's view at startup — the second screen
    // must be blank until the user explicitly starts a presentation.
    if (mDisplayView)
        mDisplayView->hide();
}

UBPresentationManager::~UBPresentationManager()
{
    if (mAudienceWindow)
        mAudienceWindow->hide();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

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
        updateStartStopStyle();
    }

    applyRunningState();
}

void UBPresentationManager::setFollowMode(bool follow)
{
    mFollowMode = follow;

    if (mFollowMode)
        updateAudienceViewFrame();
    else
        mAudienceWindow->fitPage(); // entering free mode: show full page first
}

void UBPresentationManager::swapPresenterAndAudienceScreens()
{
    if (!mDisplayManager || !mPresenterWindow)
        return;

    const QList<QScreen*> screens = mDisplayManager->availableScreens();
    if (screens.size() < 2)
        return;

    // Current audience screen (from the dropdown).
    const int audienceIdx = mAudienceScreenSelector
        ? qBound(0, mAudienceScreenSelector->currentIndex(), screens.size() - 1)
        : 0;

    // Find which screen the main presenter window is currently on.
    QScreen* presenterScreen = mPresenterWindow->screen();
    const int presenterIdx   = screens.indexOf(presenterScreen);

    if (presenterIdx < 0 || presenterIdx == audienceIdx)
        return; // nothing to swap

    // ── Move the presenter (main) window to the old audience screen ──
    QScreen* newPresenterScreen = screens.at(audienceIdx);
    mPresenterWindow->winId(); // ensure native handle exists
    if (QWindow* h = mPresenterWindow->windowHandle())
    {
        if (mPresenterWindow->isFullScreen() || mPresenterWindow->isMaximized())
            mPresenterWindow->showNormal();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        h->setScreen(newPresenterScreen);
    }
    mPresenterWindow->setGeometry(newPresenterScreen->availableGeometry());
    mPresenterWindow->showMaximized();
    mPresenterWindow->activateWindow();
    mPresenterWindow->raise();

    // ── Move the audience selector to the old presenter screen ──────
    if (mAudienceScreenSelector)
    {
        QSignalBlocker blocker(mAudienceScreenSelector);
        mAudienceScreenSelector->setCurrentIndex(presenterIdx);
    }

    // ── Reposition the audience window if a presentation is running ─
    if (mRunning)
        applyAudienceScreenSelection();
}

void UBPresentationManager::resetAudienceFocus()
{
    if (!mRunning || !mAudienceWindow)
        return;

    mAudienceWindow->fitPage();
}

// ---------------------------------------------------------------------------
// Presenter control panel
// ---------------------------------------------------------------------------

static QGroupBox* makeGroup(const QString& title)
{
    auto* g = new QGroupBox(title);
    g->setStyleSheet(
        "QGroupBox {"
        "  font-weight: bold;"
        "  border: 1px solid #cccccc;"
        "  border-radius: 6px;"
        "  margin-top: 8px;"
        "  padding-top: 4px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 8px;"
        "  color: #555555;"
        "}");
    return g;
}

void UBPresentationManager::createPresenterControls()
{
    if (!mPresenterWindow)
        return;

    mPresenterPanel = new QDockWidget(tr("Presentation"), mPresenterWindow);
    mPresenterPanel->setObjectName("presentationControlPanel");
    mPresenterPanel->setFeatures(QDockWidget::DockWidgetMovable |
                                 QDockWidget::DockWidgetFloatable);

    QWidget* root = new QWidget(mPresenterPanel);
    root->setMinimumWidth(220);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // ── Start / Stop ──────────────────────────────────────────────────────
    mStartStop = new QPushButton(tr("▶  Start Presentation"), root);
    mStartStop->setCheckable(true);
    mStartStop->setMinimumHeight(40);
    mStartStop->setStyleSheet(
        "QPushButton {"
        "  background: #27ae60; color: white;"
        "  border: none; border-radius: 8px;"
        "  font-size: 14px; font-weight: bold;"
        "}"
        "QPushButton:hover  { background: #2ecc71; }"
        "QPushButton:checked {"
        "  background: #c0392b;"
        "}"
        "QPushButton:checked:hover { background: #e74c3c; }");
    rootLayout->addWidget(mStartStop);

    // Document name — shows which document is being presented.
    mDocumentNameLabel = new QLabel(tr("—"), root);
    mDocumentNameLabel->setAlignment(Qt::AlignCenter);
    mDocumentNameLabel->setStyleSheet(
        "QLabel { color: #555; font-size: 11px; font-style: italic; padding: 2px 4px; }");
    mDocumentNameLabel->setWordWrap(true);
    rootLayout->addWidget(mDocumentNameLabel);

    // ── Screen selection ──────────────────────────────────────────────────
    {
        auto* g = makeGroup(tr("Audience Screen"));
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(4);

        mAudienceScreenSelector = new QComboBox();
        gl->addWidget(mAudienceScreenSelector);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(4);

        mSwapScreensButton = new QPushButton(tr("⇄ Swap"));
        mSwapScreensButton->setToolTip(tr("Swap presenter and audience screens"));

        mAudiencePreviewButton = new QPushButton(tr("↗ Front"));
        mAudiencePreviewButton->setEnabled(false);
        mAudiencePreviewButton->setToolTip(tr("Bring audience window to the front"));

        btnRow->addWidget(mSwapScreensButton);
        btnRow->addWidget(mAudiencePreviewButton);
        gl->addLayout(btnRow);

        rootLayout->addWidget(g);
    }

    // ── Viewport / Follow ─────────────────────────────────────────────────
    {
        auto* g = makeGroup(tr("Viewport"));
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(4);

        mFollowModeToggle = new QCheckBox(tr("Follow Mode"));
        mFollowModeToggle->setToolTip(
            tr("When enabled, the audience view follows the presenter's zoom and pan,\n"
               "clamped to the page (the audience never sees backstage content)."));
        mFollowModeToggle->setChecked(false); // off by default
        gl->addWidget(mFollowModeToggle);

        mResetFocusButton = new QPushButton(tr("↺ Reset to Full Page"));
        mResetFocusButton->setToolTip(tr("Snap the audience view back to the full page"));
        gl->addWidget(mResetFocusButton);

        // Presenter-side zoom controls for the audience screen
        auto* zoomRow = new QHBoxLayout();
        zoomRow->setSpacing(4);
        mZoomOutButton = new QPushButton(tr("−  Zoom Out"));
        mZoomInButton  = new QPushButton(tr("+  Zoom In"));
        for (auto* b : {mZoomOutButton, mZoomInButton})
            b->setMinimumHeight(28);
        zoomRow->addWidget(mZoomOutButton);
        zoomRow->addWidget(mZoomInButton);
        gl->addLayout(zoomRow);

        rootLayout->addWidget(g);
    }

    // ── Page navigation ───────────────────────────────────────────────────
    {
        auto* g = makeGroup(tr("Page Navigation"));
        auto* gl = new QHBoxLayout(g);
        gl->setSpacing(6);

        mPreviousPageButton = new QPushButton(tr("◀ Previous"));
        mNextPageButton     = new QPushButton(tr("Next ▶"));

        for (auto* b : {mPreviousPageButton, mNextPageButton})
        {
            b->setMinimumHeight(32);
            b->setStyleSheet(
                "QPushButton { border: 1px solid #aaa; border-radius: 6px; padding: 4px; }"
                "QPushButton:hover { background: #e8e8e8; }"
                "QPushButton:pressed { background: #d0d0d0; }");
        }

        gl->addWidget(mPreviousPageButton);
        gl->addWidget(mNextPageButton);
        rootLayout->addWidget(g);
    }

    // ── Audience tools ────────────────────────────────────────────────────
    {
        auto* g = makeGroup(tr("Audience Tools"));
        auto* gl = new QVBoxLayout(g);
        gl->setSpacing(4);

        mAudienceToolbarToggle = new QCheckBox(tr("Toolbar visible"));
        mAudienceToolbarToggle->setChecked(true);
        gl->addWidget(mAudienceToolbarToggle);

        auto* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        gl->addWidget(line);

        // 2-column grid for tool toggles
        auto* toolGrid = new QHBoxLayout();
        auto* col1 = new QVBoxLayout();
        auto* col2 = new QVBoxLayout();
        toolGrid->addLayout(col1);
        toolGrid->addLayout(col2);

        mPenToggle   = new QCheckBox(tr("Pen"));
        mMoveToggle  = new QCheckBox(tr("Move"));
        mShapeToggle = new QCheckBox(tr("Shape"));
        mZoomToggle  = new QCheckBox(tr("Zoom/Pan"));

        for (auto* cb : {mPenToggle, mMoveToggle, mShapeToggle, mZoomToggle})
            cb->setChecked(true);

        col1->addWidget(mPenToggle);
        col1->addWidget(mShapeToggle);
        col2->addWidget(mMoveToggle);
        col2->addWidget(mZoomToggle);
        gl->addLayout(toolGrid);

        auto* line2 = new QFrame();
        line2->setFrameShape(QFrame::HLine);
        line2->setFrameShadow(QFrame::Sunken);
        gl->addWidget(line2);

        mFreezeAudienceToggle = new QCheckBox(tr("Freeze audience view"));
        mFreezeAudienceToggle->setToolTip(
            tr("Pauses all updates on the audience screen without stopping the presentation"));
        gl->addWidget(mFreezeAudienceToggle);

        rootLayout->addWidget(g);
    }

    // ── Page background ───────────────────────────────────────────────────
    {
        auto* g = makeGroup(tr("Page Background"));
        auto* gl = new QHBoxLayout(g);
        gl->setSpacing(4);

        mBgPlainButton   = new QPushButton(tr("Blank"));
        mBgRuledButton   = new QPushButton(tr("Lines"));
        mBgCrossedButton = new QPushButton(tr("Grid"));

        for (auto* b : {mBgPlainButton, mBgRuledButton, mBgCrossedButton})
            b->setMinimumHeight(28);

        gl->addWidget(mBgPlainButton);
        gl->addWidget(mBgRuledButton);
        gl->addWidget(mBgCrossedButton);
        rootLayout->addWidget(g);
    }

    rootLayout->addStretch();

    // ── Quit ─────────────────────────────────────────────────────────────
    mQuitButton = new QPushButton(tr("✕  Quit BoardPresenter"), root);
    mQuitButton->setStyleSheet(
        "QPushButton { color: #6B7280; border: 1px solid #D1D5DB;"
        "  border-radius: 5px; padding: 4px 8px; font-size: 11px; }"
        "QPushButton:hover { background: #FEE2E2; color: #DC2626; border-color: #FCA5A5; }");
    rootLayout->addWidget(mQuitButton);

    mPresenterPanel->setWidget(root);
    mPresenterWindow->addDockWidget(Qt::RightDockWidgetArea, mPresenterPanel);
}

void UBPresentationManager::updateStartStopStyle()
{
    if (!mStartStop)
        return;

    if (mRunning)
        mStartStop->setText(tr("⏹  Stop Presentation"));
    else
        mStartStop->setText(tr("▶  Start Presentation"));
}

void UBPresentationManager::connectPresenterControls()
{
    if (!mPresenterPanel)
        return;

    connect(mStartStop, &QPushButton::toggled,
            this, &UBPresentationManager::setRunning);

    connect(mFollowModeToggle, &QCheckBox::toggled,
            this, &UBPresentationManager::setFollowMode);

    connect(mAudienceToolbarToggle, &QCheckBox::toggled, this, [this](bool v) {
        mAudienceToolState->setToolbarVisible(v);
        applyAudienceToolState();
    });
    connect(mPenToggle, &QCheckBox::toggled, this, [this](bool v) {
        mAudienceToolState->setPenEnabled(v);
        applyAudienceToolState();
    });
    connect(mMoveToggle, &QCheckBox::toggled, this, [this](bool v) {
        mAudienceToolState->setMoveEnabled(v);
        applyAudienceToolState();
    });
    connect(mShapeToggle, &QCheckBox::toggled, this, [this](bool v) {
        mAudienceToolState->setShapeEnabled(v);
        applyAudienceToolState();
    });
    connect(mZoomToggle, &QCheckBox::toggled, this, [this](bool v) {
        mAudienceToolState->setZoomEnabled(v);
        applyAudienceToolState();
    });
    connect(mFreezeAudienceToggle, &QCheckBox::toggled, this, [this](bool v) {
        mAudienceFrozen = v;
        applyAudienceToolState();
    });

    connect(mPreviousPageButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->previousScene();
    });
    connect(mNextPageButton, &QPushButton::clicked, this, [this] {
        if (!mBoardController) return;
        // If already on the last page, create a new one automatically.
        if (mBoardController->currentPage() >= mBoardController->selectedDocument()->pageCount())
            mBoardController->addScene();
        else
            mBoardController->nextScene();
    });

    connect(mAudienceScreenSelector,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                // Only act if a presentation is already running.
                if (mRunning)
                    applyAudienceScreenSelection();
            });

    connect(mSwapScreensButton, &QPushButton::clicked,
            this, &UBPresentationManager::swapPresenterAndAudienceScreens);

    connect(mZoomInButton, &QPushButton::clicked, this, [this] {
        if (mAudienceWindow && mRunning) mAudienceWindow->zoomIn();
    });
    connect(mZoomOutButton, &QPushButton::clicked, this, [this] {
        if (mAudienceWindow && mRunning) mAudienceWindow->zoomOut();
    });

    connect(mAudiencePreviewButton, &QPushButton::clicked, this, [this] {
        if (!mAudienceWindow || !mRunning)
            return;
        applyAudienceScreenSelection();
        mAudienceWindow->raise();
        mAudienceWindow->activateWindow();
    });

    connect(mResetFocusButton, &QPushButton::clicked,
            this, &UBPresentationManager::resetAudienceFocus);

    // Background buttons — only affect the active scene; audience sees the change live.
    connect(mBgPlainButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->changeBackground(false, UBPageBackground::plain);
    });
    connect(mBgRuledButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->changeBackground(false, UBPageBackground::ruled);
    });
    connect(mBgCrossedButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->changeBackground(false, UBPageBackground::crossed);
    });

    connect(mQuitButton, &QPushButton::clicked, this, [] {
        QApplication::quit();
    });

    if (mDisplayManager)
    {
        connect(mDisplayManager, &UBDisplayManager::availableScreenCountChanged,
                this, [this](int) { refreshAudienceScreenSelector(); });
    }

    // Document name label — update whenever the active scene changes.
    if (mBoardController)
    {
        auto updateDocName = [this] {
            if (!mDocumentNameLabel || !mBoardController) return;
            auto doc = mBoardController->selectedDocument();
            mDocumentNameLabel->setText(doc ? doc->name() : tr("—"));
        };
        connect(mBoardController, &UBBoardController::activeSceneChanged, this, updateDocName);
        updateDocName();
    }

    // Sync audience viewport whenever the presenter pans or zooms.
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
            tr("Screen %1  (%2 × %3)")
                .arg(i + 1)
                .arg(s ? s->geometry().width()  : 0)
                .arg(s ? s->geometry().height() : 0),
            i);
    }

    // Default: second screen for audience (if available).
    mAudienceScreenSelector->setCurrentIndex(screens.size() > 1 ? 1 : 0);
    mAudienceScreenSelector->setEnabled(!screens.isEmpty());

    const bool multiScreen = screens.size() > 1;
    if (mSwapScreensButton)
        mSwapScreensButton->setEnabled(multiScreen);
}

// ---------------------------------------------------------------------------
// Private: screen placement
// ---------------------------------------------------------------------------

void UBPresentationManager::applyAudienceScreenSelection()
{
    if (!mAudienceWindow || !mDisplayManager || !mRunning)
        return;

    const QList<QScreen*> screens = mDisplayManager->availableScreens();
    if (screens.isEmpty())
        return;

    const int index = mAudienceScreenSelector
        ? qBound(0, mAudienceScreenSelector->currentIndex(), screens.size() - 1)
        : 0;

    QScreen* target = screens.at(index);
    if (!target)
        return;

    // On Windows, the reliable way to show fullscreen on a specific screen is:
    // 1. Exit fullscreen so the window becomes moveable.
    // 2. Force native handle creation.
    // 3. Set the target screen on the native window.
    // 4. Move window geometry to the target screen.
    // 5. Process pending events so the window moves before going fullscreen.
    // 6. Show fullscreen — Qt will fullscreen on the screen matching the geometry.
    if (mAudienceWindow->isFullScreen())
        mAudienceWindow->showNormal();

    mAudienceWindow->winId(); // ensure native handle exists

    if (QWindow* handle = mAudienceWindow->windowHandle())
        handle->setScreen(target);

    mAudienceWindow->setGeometry(target->geometry());
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    UBPlatformUtils::showFullScreen(mAudienceWindow);
    mAudienceWindow->raise();
}

// ---------------------------------------------------------------------------
// Private: start / stop
// ---------------------------------------------------------------------------

void UBPresentationManager::applyRunningState()
{
    if (!mAudienceWindow)
        return;

    if (mRunning)
    {
#if defined(Q_OS_WIN)
        // If only one screen is visible the displays are likely mirrored or
        // the second monitor is off.  Ask Windows to switch to Extended mode
        // (same as Win+P → Extend), then re-apply the screen placement once
        // Windows has finished reconfiguring (typically within 3 s).
        if (mDisplayManager && mDisplayManager->availableScreens().size() < 2)
        {
            QProcess::startDetached(QStringLiteral("DisplaySwitch.exe"),
                                    {QStringLiteral("/extend")});
            QTimer::singleShot(3000, this, [this] {
                refreshAudienceScreenSelector();
                if (mRunning) applyAudienceScreenSelection();
            });
        }
#endif
        // The display manager's existing display view must be hidden so the
        // audience window is the only thing on the second screen.
        if (mDisplayView)
            mDisplayView->hide();

        if (mAudiencePreviewButton)
            mAudiencePreviewButton->setEnabled(true);

        // Tool state must be applied before the window is shown so the toolbar
        // is already in the correct state when it becomes visible.
        applyAudienceToolState();

        // Show the audience window fullscreen on the selected screen.
        // applyAudienceScreenSelection() calls UBPlatformUtils::showFullScreen()
        // which triggers showEvent() → fitPage() on the audience window.
        applyAudienceScreenSelection();

        // If follow-mode is active, sync the audience viewport to wherever
        // the presenter is currently looking.
        updateAudienceViewFrame();
    }
    else
    {
        mAudienceWindow->hide();

        // Do NOT restore mDisplayView — the second screen stays blank when
        // the presentation is not running (the audience must not see anything).

        if (mAudiencePreviewButton)
            mAudiencePreviewButton->setEnabled(false);
    }
}

// ---------------------------------------------------------------------------
// Private: tool state
// ---------------------------------------------------------------------------

void UBPresentationManager::applyAudienceToolState()
{
    if (!mAudienceWindow)
        return;

    // Push the full tool state to the window (toolbar visibility + enabled tools).
    mAudienceWindow->syncFromToolState();

    // Additionally override interactivity based on running + frozen state.
    if (UBBoardView* v = mAudienceWindow->boardView())
    {
        const bool canInteract = mRunning && !mAudienceFrozen
                                 && mAudienceToolState->anyInteractiveToolEnabled();
        v->setInteractive(canInteract);
        v->setEnabled(!mAudienceFrozen);
        mAudienceWindow->setUpdatesEnabled(!mAudienceFrozen);
    }
}

// ---------------------------------------------------------------------------
// Private: viewport sync
// ---------------------------------------------------------------------------

void UBPresentationManager::updateAudienceViewFrame()
{
    if (!mRunning || !mFollowMode || !mBoardController || !mAudienceWindow)
        return;

    if (UBBoardView* controlView = mBoardController->controlView())
        mAudienceWindow->syncViewport(controlView);
}
