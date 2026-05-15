/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#ifndef UBAUDIENCEWINDOW_H_
#define UBAUDIENCEWINDOW_H_

#include <QMainWindow>
#include <QPointer>

class UBBoardView;
class UBAudienceToolState;
class QToolBar;
class QAction;

class UBAudienceWindow : public QMainWindow
{
    Q_OBJECT

public:
    UBAudienceWindow(UBBoardView* audienceView, UBAudienceToolState* toolState, QWidget* parent = nullptr);
    ~UBAudienceWindow() override;

    UBBoardView* audienceView() const { return mAudienceView; }
    void syncFromToolState();

private:
    void buildToolbar();
    void connectSignals();

    QPointer<UBBoardView> mAudienceView;
    QPointer<UBAudienceToolState> mToolState;
    QToolBar* mToolbar{nullptr};
    QAction* mPenAction{nullptr};
    QAction* mMoveAction{nullptr};
    QAction* mShapeAction{nullptr};
    QAction* mZoomAction{nullptr};
};

#endif
