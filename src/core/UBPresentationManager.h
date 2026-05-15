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
class QPushButton;

class UBPresentationManager : public QObject
{
    Q_OBJECT

public:
    UBPresentationManager(UBApplicationController* appController,
                          UBBoardController* boardController,
                          UBDisplayManager* displayManager,
                          UBMainWindow* presenterWindow,
                          UBBoardView* audienceView,
                          QObject* parent = nullptr);
    ~UBPresentationManager() override;

    bool isRunning() const { return mRunning; }
    bool followMode() const { return mFollowMode; }
    bool shouldSyncAudienceViewport() const;
    UBAudienceToolState* audienceToolState() const { return mAudienceToolState; }
    UBAudienceWindow* audienceWindow() const { return mAudienceWindow; }

public slots:
    void startPresentation();
    void stopPresentation();
    void setRunning(bool enabled);
    void setFollowMode(bool follow);
    void resetAudienceFocus();

private:
    void createPresenterControls();
    void connectPresenterControls();
    void applyRunningState();
    void applyAudienceToolState();
    void updateAudienceViewFrame();

    QPointer<UBApplicationController> mAppController;
    QPointer<UBBoardController> mBoardController;
    QPointer<UBDisplayManager> mDisplayManager;
    QPointer<UBMainWindow> mPresenterWindow;
    QPointer<UBBoardView> mAudienceView;

    UBAudienceToolState* mAudienceToolState{nullptr};
    UBAudienceWindow* mAudienceWindow{nullptr};
    QDockWidget* mPresenterPanel{nullptr};
    QCheckBox* mStartStop{nullptr};
    QCheckBox* mFollowModeToggle{nullptr};
    QCheckBox* mAudienceToolbarToggle{nullptr};
    QCheckBox* mPenToggle{nullptr};
    QCheckBox* mMoveToggle{nullptr};
    QCheckBox* mShapeToggle{nullptr};
    QCheckBox* mZoomToggle{nullptr};
    QPushButton* mResetFocusButton{nullptr};

    bool mRunning{false};
    bool mFollowMode{true};
};

#endif
