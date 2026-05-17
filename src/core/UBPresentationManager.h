/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#ifndef UBPRESENTATIONMANAGER_H_
#define UBPRESENTATIONMANAGER_H_

#include <QObject>
#include <QPointer>

class UBApplicationController;
class UBBoardController;
class UBDisplayManager;
class UBMainWindow;
class UBBoardView;
class UBAudienceToolState;
class UBAudienceWindow;
class QDockWidget;
class QCheckBox;
class QLabel;
class QPushButton;
class QComboBox;

class UBPresentationManager : public QObject
{
    Q_OBJECT

public:
    UBPresentationManager(UBApplicationController* appController,
                          UBBoardController* boardController,
                          UBDisplayManager* displayManager,
                          UBMainWindow* presenterWindow,
                          UBBoardView* displayView,
                          QObject* parent = nullptr);
    ~UBPresentationManager() override;

    bool isRunning() const { return mRunning; }
    bool followMode() const { return mFollowMode; }
    bool shouldSyncAudienceViewport() const;
    UBAudienceToolState* audienceToolState() const { return mAudienceToolState; }
    UBAudienceWindow*    audienceWindow()    const { return mAudienceWindow; }

public slots:
    void startPresentation();
    void stopPresentation();
    void setRunning(bool enabled);
    void setFollowMode(bool follow);
    void resetAudienceFocus();
    void swapPresenterAndAudienceScreens();

private:
    void createPresenterControls();
    void connectPresenterControls();
    void refreshAudienceScreenSelector();
    void applyAudienceScreenSelection();
    void applyRunningState();
    void applyAudienceToolState();
    void updateAudienceViewFrame();
    void updateStartStopStyle();

    QPointer<UBApplicationController> mAppController;
    QPointer<UBBoardController>       mBoardController;
    QPointer<UBDisplayManager>        mDisplayManager;
    QPointer<UBMainWindow>            mPresenterWindow;
    QPointer<UBBoardView>             mDisplayView;

    UBAudienceToolState* mAudienceToolState{nullptr};
    UBAudienceWindow*    mAudienceWindow{nullptr};
    QDockWidget*         mPresenterPanel{nullptr};

    // Start/Stop is a checkable QPushButton so it can be styled per state.
    QPushButton* mStartStop{nullptr};

    QCheckBox*   mFollowModeToggle{nullptr};
    QCheckBox*   mAudienceToolbarToggle{nullptr};
    QCheckBox*   mPenToggle{nullptr};
    QCheckBox*   mMoveToggle{nullptr};
    QCheckBox*   mShapeToggle{nullptr};
    QCheckBox*   mZoomToggle{nullptr};
    QCheckBox*   mFreezeAudienceToggle{nullptr};
    QComboBox*   mAudienceScreenSelector{nullptr};
    QPushButton* mSwapScreensButton{nullptr};
    QPushButton* mPreviousPageButton{nullptr};
    QPushButton* mNextPageButton{nullptr};
    QPushButton* mAudiencePreviewButton{nullptr};
    QPushButton* mResetFocusButton{nullptr};
    QPushButton* mZoomInButton{nullptr};
    QPushButton* mZoomOutButton{nullptr};
    QPushButton* mQuitButton{nullptr};

    QLabel*      mDocumentNameLabel{nullptr};

    // Background buttons (plain / ruled / crossed / dotted)
    QPushButton* mBgPlainButton{nullptr};
    QPushButton* mBgRuledButton{nullptr};
    QPushButton* mBgCrossedButton{nullptr};
    QPushButton* mBgDottedButton{nullptr};

    bool mRunning{false};
    bool mFollowMode{false};   // off by default — presenter decides when to lock
    bool mAudienceFrozen{false};
};

#endif
