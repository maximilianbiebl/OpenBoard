/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */




#include "UBLeftPalette.h"
#include "core/UBSettings.h"

#include <QMouseEvent>

#include "core/memcheck.h"

/**
 * \brief The constructor
 */
UBLeftPalette::UBLeftPalette(QWidget *parent, const char *name):
    UBDockPalette(eUBDockPaletteType_LEFT, parent)
{
    setObjectName(name);
    setOrientation(eUBDockOrientation_Left);
    mCollapseWidth = 80;

    bool isCollapsed = false;
    if(mCurrentMode == eUBDockPaletteWidget_BOARD){
        mLastWidth = UBSettings::settings()->leftLibPaletteBoardModeWidth->get().toInt();
        isCollapsed = UBSettings::settings()->leftLibPaletteBoardModeIsCollapsed->get().toBool();
    }
    else{
        mLastWidth = UBSettings::settings()->leftLibPaletteDesktopModeWidth->get().toInt();
        isCollapsed = UBSettings::settings()->leftLibPaletteDesktopModeIsCollapsed->get().toBool();
    }

    if(isCollapsed)
        resize(0,parentWidget()->height());
    else
        resize(mLastWidth, parentWidget()->height());
}

/**
 * \brief The destructor
 */
UBLeftPalette::~UBLeftPalette()
{

}


void UBLeftPalette::onDocumentSet(std::shared_ptr<UBDocumentProxy> documentProxy)
{
    Q_UNUSED(documentProxy)
    // the tab zero is forced
    mLastOpenedTabForMode.insert(eUBDockPaletteWidget_BOARD, 0);
}

/**
 * \brief Update the maximum width
 */
void UBLeftPalette::updateMaxWidth()
{
    setMaximumWidth((int)(parentWidget()->width() * 0.45));
}

/**
 * \brief Handle the resize event
 * @param event as the resize event
 */
void UBLeftPalette::resizeEvent(QResizeEvent *event)
{
    int newWidth = width();
    if(mCurrentMode == eUBDockPaletteWidget_BOARD){
        if(newWidth > mCollapseWidth)
            UBSettings::settings()->leftLibPaletteBoardModeWidth->set(newWidth);
        UBSettings::settings()->leftLibPaletteBoardModeIsCollapsed->set(newWidth == 0);
    }
    else{
        if(newWidth > mCollapseWidth)
            UBSettings::settings()->leftLibPaletteDesktopModeWidth->set(newWidth);
        UBSettings::settings()->leftLibPaletteDesktopModeIsCollapsed->set(newWidth == 0);
    }
    UBDockPalette::resizeEvent(event);
}


void UBLeftPalette::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && isOnRightEdge(event->pos()))
    {
        mEdgeDragging = true;
        mEdgeDragStartX = event->globalPosition().toPoint().x();
        mEdgeDragStartWidth = width();
        event->accept();
        return;
    }
    UBDockPalette::mousePressEvent(event);
}

void UBLeftPalette::mouseReleaseEvent(QMouseEvent* event)
{
    if (mEdgeDragging)
    {
        mEdgeDragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    UBDockPalette::mouseReleaseEvent(event);
}

void UBLeftPalette::mouseMoveEvent(QMouseEvent* event)
{
    if (mEdgeDragging)
    {
        int delta = event->globalPosition().toPoint().x() - mEdgeDragStartX;
        int newWidth = qBound(mCollapseWidth, mEdgeDragStartWidth + delta, maximumWidth());
        resize(newWidth, height());
        event->accept();
        return;
    }
    if (isOnRightEdge(event->pos()))
        setCursor(Qt::SizeHorCursor);
    else if (!mCanResize)
        unsetCursor();
    UBDockPalette::mouseMoveEvent(event);
}

bool UBLeftPalette::switchMode(eUBDockPaletteWidgetMode mode)
{
    int newModeWidth;
    if(mode == eUBDockPaletteWidget_BOARD){
        mLastWidth = UBSettings::settings()->leftLibPaletteBoardModeWidth->get().toInt();
        newModeWidth = mLastWidth;
        if(UBSettings::settings()->leftLibPaletteBoardModeIsCollapsed->get().toBool())
            newModeWidth = 0;
    }
    else{
        mLastWidth = UBSettings::settings()->leftLibPaletteDesktopModeWidth->get().toInt();
        newModeWidth = mLastWidth;
        if(UBSettings::settings()->leftLibPaletteDesktopModeIsCollapsed->get().toBool())
            newModeWidth = 0;
    }
    resize(newModeWidth,height());
    return UBDockPalette::switchMode(mode);
}
