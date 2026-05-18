/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#ifndef UBAUDIENCETOOLSTATE_H_
#define UBAUDIENCETOOLSTATE_H_

#include <QObject>

class UBAudienceToolState : public QObject
{
    Q_OBJECT

public:
    explicit UBAudienceToolState(QObject* parent = nullptr);

    bool penEnabled() const { return mPenEnabled; }
    bool moveEnabled() const { return mMoveEnabled; }
    bool shapeEnabled() const { return mShapeEnabled; }
    bool zoomEnabled() const { return mZoomEnabled; }
    bool toolbarVisible() const { return mToolbarVisible; }

    void setPenEnabled(bool enabled);
    void setMoveEnabled(bool enabled);
    void setShapeEnabled(bool enabled);
    void setZoomEnabled(bool enabled);
    void setToolbarVisible(bool visible);

    bool anyInteractiveToolEnabled() const;
    bool isStylusToolEnabled(int tool) const;

signals:
    void changed();

private:
    bool mPenEnabled{true};
    bool mMoveEnabled{true};
    bool mShapeEnabled{true};
    bool mZoomEnabled{true};
    bool mToolbarVisible{true};
};

#endif
