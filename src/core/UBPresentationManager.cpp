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
#include <QMessageBox>
#include <QPainter>
#include <QStackedWidget>
#include <QTimer>
#include <QComboBox>
#include <QDockWidget>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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
// Helper: vertical-text label for the collapsed panel tab strip
// ---------------------------------------------------------------------------

class UBVerticalLabel : public QWidget
{
public:
    explicit UBVerticalLabel(const QString& text, QWidget* parent = nullptr)
        : QWidget(parent), mText(text)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setFixedWidth(20);
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(Qt::white);
        QFont f(p.font());
        f.setPixelSize(11);
        p.setFont(f);
        p.translate(0, height());
        p.rotate(-90.0);
        p.drawText(QRect(0, 0, height(), width()), Qt::AlignCenter, mText);
    }
private:
    QString mText;
};

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

    // Warn when starting in duplicate / single-screen mode.
    if (enabled && mDisplayManager)
    {
        const QList<QScreen*> screens = mDisplayManager->availableScreens();
        bool isDuplicate = screens.size() < 2;
        if (!isDuplicate && screens.size() >= 2)
        {
            const QRect r0 = screens.at(0) ? screens.at(0)->geometry() : QRect();
            isDuplicate = true;
            for (int i = 1; i < screens.size(); ++i)
                if (screens.at(i) && screens.at(i)->geometry() != r0)
                { isDuplicate = false; break; }
        }
        if (isDuplicate)
        {
            // Always block — the presentation must not start in duplicate/mirror mode.
            QMessageBox::warning(
                mPresenterWindow,
                tr("Kein erweiterter Bildschirm"),
                tr("Es wurde kein zweiter Bildschirm im erweiterten Modus erkannt.\n\n"
                   "Bitte wechseln Sie zuerst in den erweiterten Anzeigemodus "
                   "(Win + P → Erweitern) und versuchen Sie es dann erneut."));
            // Revert the Start button.
            if (mStartStop)
            {
                QSignalBlocker blocker(mStartStop);
                mStartStop->setChecked(false);
                updateStartStopStyle();
            }
            return;
        }
    }

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
    mPresenterWindow->setGeometry(newPresenterScreen->geometry());
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    UBPlatformUtils::showFullScreen(mPresenterWindow);
    mPresenterWindow->activateWindow();
    mPresenterWindow->raise();

    // ── Move the audience selector to the old presenter screen ──────
    if (mAudienceScreenSelector)
    {
        QSignalBlocker blocker(mAudienceScreenSelector);
        mAudienceScreenSelector->setCurrentIndex(presenterIdx);
    }

    // ── Reposition the audience window (always, mRunning guard is inside) ──
    applyAudienceScreenSelection();

    // Always bring the presenter window back to front after the swap.
    if (mPresenterWindow)
        mPresenterWindow->activateWindow();
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

    mPresenterPanel = new QDockWidget(mPresenterWindow);
    mPresenterPanel->setObjectName("presentationControlPanel");
    // No default title bar — we provide our own tab-style collapse strip.
    mPresenterPanel->setFeatures(QDockWidget::DockWidgetMovable |
                                 QDockWidget::DockWidgetFloatable);

    // ── Custom title bar (matches UBDockPalette tab visual) ──────────────
    {
        const QString tabStyle =
            "QWidget {"
            "  background: rgba(80,80,80,220);"
            "  border-radius: 4px;"
            "}"
            "QPushButton {"
            "  background: transparent; color: white;"
            "  border: none; font-size: 14px; font-weight: bold;"
            "  min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,40); border-radius: 3px; }";

        auto* titleBar = new QWidget(mPresenterPanel);
        titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        titleBar->setStyleSheet(tabStyle);

        auto* outerVBox = new QVBoxLayout(titleBar);
        outerVBox->setContentsMargins(2, 2, 2, 2);
        outerVBox->setSpacing(0);

        // QStackedWidget — index 0 = expanded, index 1 = collapsed strip
        mPresenterPanelTitleStack = new QStackedWidget(titleBar);
        mPresenterPanelTitleStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        outerVBox->addWidget(mPresenterPanelTitleStack);

        // ── Page 0: expanded horizontal row ─────────────────────────────
        auto* expandedPage = new QWidget(mPresenterPanelTitleStack);
        auto* hb = new QHBoxLayout(expandedPage);
        hb->setContentsMargins(2, 0, 2, 0);
        hb->setSpacing(4);

        mPanelCollapseBtn = new QPushButton(tr("◀"), expandedPage);
        mPanelCollapseBtn->setToolTip(tr("Collapse panel"));

        mPresenterPanelTitleLbl = new QLabel(tr("Presentation"), expandedPage);
        mPresenterPanelTitleLbl->setStyleSheet(
            "font-size: 12px; font-weight: bold; color: white; background: transparent;");

        hb->addWidget(mPanelCollapseBtn);
        hb->addWidget(mPresenterPanelTitleLbl);
        hb->addStretch();
        mPresenterPanelTitleStack->addWidget(expandedPage);   // index 0

        // ── Page 1: collapsed vertical strip ────────────────────────────
        auto* collapsedPage = new QWidget(mPresenterPanelTitleStack);
        collapsedPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* vb = new QVBoxLayout(collapsedPage);
        vb->setContentsMargins(2, 4, 2, 4);
        vb->setSpacing(4);
        vb->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

        mPanelExpandBtn = new QPushButton(tr("▶"), collapsedPage);
        mPanelExpandBtn->setToolTip(tr("Expand panel"));

        auto* vertLbl = new UBVerticalLabel(tr("Presentation"), collapsedPage);

        vb->addWidget(mPanelExpandBtn, 0, Qt::AlignHCenter);
        vb->addWidget(vertLbl, 1);   // stretch=1 so it fills remaining height
        mPresenterPanelTitleStack->addWidget(collapsedPage);  // index 1

        mPresenterPanelTitleStack->setCurrentIndex(0);
        mPresenterPanel->setTitleBarWidget(titleBar);
    }

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
        mBgDottedButton  = new QPushButton(tr("Dots"));

        auto* bgRow1 = new QHBoxLayout();
        auto* bgRow2 = new QHBoxLayout();
        for (auto* b : {mBgPlainButton, mBgRuledButton})
        { b->setMinimumHeight(28); bgRow1->addWidget(b); }
        for (auto* b : {mBgCrossedButton, mBgDottedButton})
        { b->setMinimumHeight(28); bgRow2->addWidget(b); }
        gl->addLayout(bgRow1);
        gl->addLayout(bgRow2);
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
    mPresenterWindow->resizeDocks({mPresenterPanel}, {260}, Qt::Horizontal);
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
            [this](int newAudienceIdx) {
                // Move presenter to whichever screen is NOT the audience screen
                movePresenterToNonAudienceScreen(newAudienceIdx);
                // Apply audience window placement
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
    connect(mBgDottedButton, &QPushButton::clicked, this, [this] {
        if (mBoardController) mBoardController->changeBackground(false, UBPageBackground::dotted);
    });

    connect(mQuitButton, &QPushButton::clicked, this, [] {
        QApplication::quit();
    });

    // ── Panel collapse / expand ───────────────────────────────────────────
    auto togglePanel = [this] {
        if (!mPresenterPanel || !mPresenterWindow)
            return;
        QWidget* content = mPresenterPanel->widget();
        if (!content)
            return;
        if (!mPresenterPanelCollapsed)
        {
            // Collapse: hide content, switch title bar to collapsed strip.
            mPresenterPanelLastWidth = mPresenterPanel->width();
            content->setMinimumWidth(0);
            content->hide();
            if (mPresenterPanelTitleStack)
                mPresenterPanelTitleStack->setCurrentIndex(1);
            mPresenterPanel->setMaximumWidth(32);
            mPresenterPanel->setMinimumWidth(0);
            mPresenterWindow->resizeDocks({mPresenterPanel}, {32}, Qt::Horizontal);
        }
        else
        {
            // Expand: restore title bar, then show content.
            mPresenterPanel->setMaximumWidth(QWIDGETSIZE_MAX);
            mPresenterWindow->resizeDocks({mPresenterPanel}, {mPresenterPanelLastWidth}, Qt::Horizontal);
            if (mPresenterPanelTitleStack)
                mPresenterPanelTitleStack->setCurrentIndex(0);
            content->setMinimumWidth(220);
            content->show();
        }
        mPresenterPanelCollapsed = !mPresenterPanelCollapsed;
    };
    if (mPanelCollapseBtn)
        connect(mPanelCollapseBtn, &QPushButton::clicked, this, togglePanel);
    if (mPanelExpandBtn)
        connect(mPanelExpandBtn,   &QPushButton::clicked, this, togglePanel);

    if (mDisplayManager)
    {
        connect(mDisplayManager, &UBDisplayManager::availableScreenCountChanged,
                this, [this](int) { refreshAudienceScreenSelector(); });
    }

    // Document title — centered label in the main toolbar (fullscreen hides the window title bar)
    if (mPresenterWindow)
    {
        // Flexible spacer so the label centres itself
        auto* spacerL = new QWidget();
        spacerL->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        mPresenterWindow->boardToolBar->addWidget(spacerL);

        mDocTitleLabel = new QLabel(tr("BoardPresenter"), mPresenterWindow->boardToolBar);
        mDocTitleLabel->setAlignment(Qt::AlignCenter);
        mDocTitleLabel->setStyleSheet(
            "QLabel { font-size: 14px; font-weight: bold; padding: 0 12px; }");
        mPresenterWindow->boardToolBar->addWidget(mDocTitleLabel);

        auto* spacerR = new QWidget();
        spacerR->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        mPresenterWindow->boardToolBar->addWidget(spacerR);
    }

    // Document name — update the main window title bar whenever the active scene changes.
    if (mBoardController)
    {
        auto updateDocName = [this] {
            if (!mBoardController || !mPresenterWindow) return;
            auto doc = mBoardController->selectedDocument();
            QString docName = doc ? doc->name() : QString();
            mPresenterWindow->setWindowTitle(docName.isEmpty() ? "BoardPresenter" : docName + " \xe2\x80\x94 BoardPresenter");
            if (mDocTitleLabel)
                mDocTitleLabel->setText(docName.isEmpty() ? tr("BoardPresenter") : docName);
        };
        connect(mBoardController, &UBBoardController::activeSceneChanged, this, updateDocName);
        connect(mBoardController, &UBDocumentContainer::documentSet, this, [updateDocName](std::shared_ptr<UBDocumentProxy>){ updateDocName(); });
        // Defer the initial call so the document has time to load.
        QTimer::singleShot(500, this, updateDocName);
        // Safety-net: re-check every second in case signals were missed.
        auto* titleTimer = new QTimer(this);
        titleTimer->setInterval(1000);
        connect(titleTimer, &QTimer::timeout, this, updateDocName);
        titleTimer->start();
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
    // But make sure the audience screen is NOT the same as the presenter window's screen.
    int defaultAudienceIdx = (screens.size() > 1) ? 1 : 0;
    if (mPresenterWindow && screens.size() > 1)
    {
        QScreen* presenterScreen = mPresenterWindow->screen();
        int presenterIdx = screens.indexOf(presenterScreen);
        // If the default audience index == presenter screen, pick the other one
        if (presenterIdx >= 0 && defaultAudienceIdx == presenterIdx)
            defaultAudienceIdx = (presenterIdx == 0) ? 1 : 0;
    }
    mAudienceScreenSelector->setCurrentIndex(defaultAudienceIdx);
    mAudienceScreenSelector->setEnabled(!screens.isEmpty());

    // Swap only makes sense when screens have distinct geometries.
    bool extendedMode = screens.size() > 1;
    if (extendedMode && screens.size() >= 2)
    {
        QRect r0 = screens.at(0) ? screens.at(0)->geometry() : QRect();
        for (int i = 1; i < screens.size(); ++i)
            if (screens.at(i) && screens.at(i)->geometry() == r0)
            { extendedMode = false; break; }
    }
    if (mSwapScreensButton)
        mSwapScreensButton->setEnabled(extendedMode);
}

// ---------------------------------------------------------------------------
// Private: screen placement
// ---------------------------------------------------------------------------

void UBPresentationManager::applyAudienceScreenSelection()
{
    if (!mAudienceWindow || !mDisplayManager)
        return;

    // Always hide the legacy display view to prevent it competing with audience window.
    if (mDisplayView)
        mDisplayView->hide();

    const QList<QScreen*> screens = mDisplayManager->availableScreens();
    if (screens.isEmpty())
        return;

    const int index = mAudienceScreenSelector
        ? qBound(0, mAudienceScreenSelector->currentIndex(), screens.size() - 1)
        : 0;

    QScreen* target = screens.at(index);
    if (!target)
        return;

    // Keep audience window hidden while repositioning to avoid any flash
    // on the wrong screen. Move the native window to the target screen via
    // the window handle (does not require the window to be visible), then
    // set the geometry, process events so the OS registers the move, and
    // only then show fullscreen if the presentation is running.
    mAudienceWindow->hide();
    mAudienceWindow->winId(); // ensure native handle exists

    if (QWindow* handle = mAudienceWindow->windowHandle())
    {
        handle->setScreen(target);
        handle->setGeometry(target->geometry().x(), target->geometry().y(),
                            target->geometry().width(), target->geometry().height());
    }
    mAudienceWindow->setGeometry(target->geometry());
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (mRunning)
    {
        UBPlatformUtils::showFullScreen(mAudienceWindow);
        mAudienceWindow->raise();
        // In duplicate / single-screen mode the audience window and the presenter
        // window share the same physical display.  Bring the presenter back to
        // front so it stays accessible.
        if (mPresenterWindow)
        {
            mPresenterWindow->activateWindow();
            mPresenterWindow->raise();
        }
    }
    // else: window stays hidden; it will be shown correctly when presentation starts
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
        // Detect duplicate / mirror mode: screens share the same geometry.
        // In this mode we skip screen-separation logic and just show the
        // audience window on the selected screen without attempting to move
        // the presenter window to a different display.
        bool isDuplicateMode = false;
        if (mDisplayManager)
        {
            const QList<QScreen*> screens = mDisplayManager->availableScreens();
            if (screens.size() >= 2)
            {
                QRect r0 = screens.at(0) ? screens.at(0)->geometry() : QRect();
                isDuplicateMode = true;
                for (int i = 1; i < screens.size(); ++i)
                    if (screens.at(i) && screens.at(i)->geometry() != r0)
                    { isDuplicateMode = false; break; }
            }
            else if (screens.size() < 2)
            {
                isDuplicateMode = true;
            }
        }
        // The display manager's existing display view must be hidden so the
        // audience window is the only thing on the second screen.
        // processEvents() forces the hide to take visual effect immediately,
        // preventing any flash of the legacy display on the audience screen.
        if (mDisplayView)
            mDisplayView->hide();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        if (mAudiencePreviewButton)
            mAudiencePreviewButton->setEnabled(true);

        // Tool state must be applied before the window is shown so the toolbar
        // is already in the correct state when it becomes visible.
        applyAudienceToolState();

        // In extended mode only: ensure audience screen ≠ presenter screen.
        // Skip this entire block when screens are duplicated/mirrored because
        // moving windows between identical geometries causes layout chaos.
        if (!isDuplicateMode && mPresenterWindow && mAudienceScreenSelector && mDisplayManager)
        {
            const QList<QScreen*> screens = mDisplayManager->availableScreens();
            if (screens.size() > 1)
            {
                QScreen* presenterScreen = mPresenterWindow->screen();
                const int presenterIdx = screens.indexOf(presenterScreen);
                const int audienceIdx  = mAudienceScreenSelector->currentIndex();
                if (presenterIdx >= 0 && presenterIdx == audienceIdx)
                {
                    for (int i = 0; i < screens.size(); ++i)
                    {
                        if (i != audienceIdx)
                        {
                            movePresenterToNonAudienceScreen(audienceIdx);
                            break;
                        }
                    }
                }
            }
        }

        // Show the audience window fullscreen on the selected screen.
        applyAudienceScreenSelection();

        // Re-hide the legacy display view to ensure it doesn't compete with the audience window.
        // Some internal events (screenLayoutChanged) may re-show it; we guard against that here.
        if (mDisplayView)
            mDisplayView->hide();
        QTimer::singleShot(100, this, [this] { if (mDisplayView && mRunning) mDisplayView->hide(); });
        QTimer::singleShot(500, this, [this] { if (mDisplayView && mRunning) mDisplayView->hide(); });

        // Delayed fitPage as a safety net — showEvent may fire before the window
        // has settled into its final geometry on the target screen.
        QTimer::singleShot(200, mAudienceWindow, &UBAudienceWindow::fitPage);

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
        // Use setUpdatesEnabled to freeze visuals; avoid setEnabled(false) because
        // it breaks QGraphicsView re-painting after the widget is re-enabled.
        mAudienceWindow->setUpdatesEnabled(!mAudienceFrozen);
        if (!mAudienceFrozen)
        {
            // After unfreeze: force the view to re-render all scene changes that
            // occurred during the frozen period.
            if (auto scene = v->scene())
                scene->update();          // mark whole scene as dirty
            v->update(v->rect());         // queue repaint of the view
            v->viewport()->repaint();     // synchronous repaint so it happens immediately
            mAudienceWindow->fitPage();
        }
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

// ---------------------------------------------------------------------------
// Private: move presenter window to non-audience screen
// ---------------------------------------------------------------------------

void UBPresentationManager::movePresenterToNonAudienceScreen(int audienceScreenIdx)
{
    if (!mDisplayManager || !mPresenterWindow)
        return;

    const QList<QScreen*> screens = mDisplayManager->availableScreens();
    if (screens.size() < 2)
        return;

    // Skip if screens are duplicated/mirrored — moving is pointless and disruptive.
    const QRect r0 = screens.at(0) ? screens.at(0)->geometry() : QRect();
    bool allSame = true;
    for (int i = 1; i < screens.size(); ++i)
        if (screens.at(i) && screens.at(i)->geometry() != r0)
        { allSame = false; break; }
    if (allSame)
        return;

    // Find first screen that is NOT the audience screen
    int presenterIdx = -1;
    for (int i = 0; i < screens.size(); ++i)
    {
        if (i != audienceScreenIdx)
        {
            presenterIdx = i;
            break;
        }
    }
    if (presenterIdx < 0)
        return;

    QScreen* presenterScreen = screens.at(presenterIdx);
    mPresenterWindow->winId();
    if (QWindow* h = mPresenterWindow->windowHandle())
    {
        if (mPresenterWindow->isFullScreen() || mPresenterWindow->isMaximized())
            mPresenterWindow->showNormal();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        h->setScreen(presenterScreen);
    }
    mPresenterWindow->setGeometry(presenterScreen->geometry());
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    UBPlatformUtils::showFullScreen(mPresenterWindow);
    mPresenterWindow->activateWindow();
    mPresenterWindow->raise();
}
