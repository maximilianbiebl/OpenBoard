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
#include <QRectF>

class UBBoardController;
class UBBoardView;
class UBAudienceToolState;
class QToolBar;
class QAction;
class QResizeEvent;
class QShowEvent;

class UBAudienceWindow : public QMainWindow
{
    Q_OBJECT

public:
    UBAudienceWindow(UBBoardController* boardController,
                     UBAudienceToolState* toolState,
                     QWidget* parent = nullptr);
    ~UBAudienceWindow() override;

    UBBoardView* boardView() const { return mOwnView; }

    // Show full page fitted to the window (always all content visible).
    void fitPage();

    // Follow-mode: show the portion of the page the presenter is viewing.
    // Clamped to page rect — audience never sees backstage.
    void syncViewport(UBBoardView* controlView);

    // Zoom in/out on the audience view (clamped to page).
    void zoomIn();
    void zoomOut();

public slots:
    void syncFromToolState();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onActiveSceneChanged();

private:
    void buildToolbar();
    void connectSignals();

    // Page rectangle in scene coordinates (centred at origin).
    QRectF pageRectInScene() const;

    UBBoardView*                  mOwnView{nullptr};
    QPointer<UBBoardController>   mBoardController;
    QPointer<UBAudienceToolState> mToolState;

    QToolBar* mToolbar{nullptr};
    QAction*  mPenAction{nullptr};
    QAction*  mEraserAction{nullptr};
    QAction*  mMarkerAction{nullptr};
    QAction*  mMoveAction{nullptr};
    QAction*  mShapeAction{nullptr};
    QAction*  mZoomAction{nullptr};
    QAction*  mLaserAction{nullptr};
    QAction*  mZoomInAction{nullptr};
    QAction*  mZoomOutAction{nullptr};
    QAction*  mFitPageAction{nullptr};
};

#endif
