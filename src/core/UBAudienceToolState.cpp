/*
 * Copyright (C) 2026 OpenBoard contributors
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#include "UBAudienceToolState.h"

#include "core/UB.h"

UBAudienceToolState::UBAudienceToolState(QObject* parent)
    : QObject(parent)
{
}

void UBAudienceToolState::setPenEnabled(bool enabled)
{
    if (mPenEnabled == enabled)
    {
        return;
    }

    mPenEnabled = enabled;
    emit changed();
}

void UBAudienceToolState::setMoveEnabled(bool enabled)
{
    if (mMoveEnabled == enabled)
    {
        return;
    }

    mMoveEnabled = enabled;
    emit changed();
}

void UBAudienceToolState::setShapeEnabled(bool enabled)
{
    if (mShapeEnabled == enabled)
    {
        return;
    }

    mShapeEnabled = enabled;
    emit changed();
}

void UBAudienceToolState::setZoomEnabled(bool enabled)
{
    if (mZoomEnabled == enabled)
    {
        return;
    }

    mZoomEnabled = enabled;
    emit changed();
}

void UBAudienceToolState::setToolbarVisible(bool visible)
{
    if (mToolbarVisible == visible)
    {
        return;
    }

    mToolbarVisible = visible;
    emit changed();
}

bool UBAudienceToolState::anyInteractiveToolEnabled() const
{
    return mToolbarVisible && (mPenEnabled || mMoveEnabled || mShapeEnabled || mZoomEnabled);
}

bool UBAudienceToolState::isStylusToolEnabled(int tool) const
{
    if (!mToolbarVisible)
    {
        return false;
    }

    switch (tool)
    {
        case UBStylusTool::Pen:
        case UBStylusTool::Marker:
        case UBStylusTool::Eraser:
        case UBStylusTool::Text:
            return mPenEnabled;

        case UBStylusTool::Selector:
        case UBStylusTool::Play:
            return mMoveEnabled;

        case UBStylusTool::Line:
            return mShapeEnabled;

        case UBStylusTool::Hand:
        case UBStylusTool::ZoomIn:
        case UBStylusTool::ZoomOut:
            return mZoomEnabled;

        case UBStylusTool::Pointer:
        case UBStylusTool::Capture:
            return false;

        default:
            return true;
    }
}
