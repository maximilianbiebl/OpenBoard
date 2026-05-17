/*
 * Copyright (C) 2015-2025 Département de l'Instruction Publique (DIP-SEM)
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


#include "UBThumbnailTextItem.h"

#include <QFocusEvent>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>

UBThumbnailTextItem::UBThumbnailTextItem()
    : QGraphicsTextItem()
{
}

UBThumbnailTextItem::UBThumbnailTextItem(int index)
    : QGraphicsTextItem()
    , mUnelidedText(toPlainText())
{
    setPageNumber(index + 1);
}

UBThumbnailTextItem::UBThumbnailTextItem(const QString& text)
    : QGraphicsTextItem(text)
    , mUnelidedText(text)
{
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

QRectF UBThumbnailTextItem::boundingRect() const
{
    return QRectF(QPointF(0.0, 0.0), QSize(mWidth, QFontMetricsF(font()).height() + 5));
}

void UBThumbnailTextItem::setWidth(qreal pWidth)
{
    if (mWidth != pWidth)
    {
        prepareGeometryChange();
        mWidth = pWidth;
        computeText();

        // center text
        setTextWidth(mWidth);
        document()->setDefaultTextOption(QTextOption(Qt::AlignCenter));
    }
}

qreal UBThumbnailTextItem::width()
{
    return mWidth;
}

void UBThumbnailTextItem::setPageNumber(int i)
{
    mUnelidedText = tr("Page %0").arg(i);
    computeText();
}

void UBThumbnailTextItem::setText(const QString& text)
{
    mUnelidedText = text;
    computeText();
}

void UBThumbnailTextItem::computeText()
{
    QFontMetricsF fm(font());
    QString elidedText = fm.elidedText(mUnelidedText, Qt::ElideLeft, mWidth - 2 * document()->documentMargin() - 1);
    setPlainText(elidedText);
}

void UBThumbnailTextItem::setEditMode(bool editing)
{
    mIsEditing = editing;
    if (editing)
        document()->setPageColor(Qt::white);
    else
        document()->setPageColor(Qt::transparent);
    update();
}

void UBThumbnailTextItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if (mIsEditing)
    {
        painter->save();
        painter->setPen(QPen(QColor("#0078d4"), 2)); // blue border like Windows input
        painter->setBrush(Qt::white);
        painter->drawRect(boundingRect().adjusted(0, 0, -1, -1));
        painter->restore();
    }
    QGraphicsTextItem::paint(painter, option, widget);
}

void UBThumbnailTextItem::focusOutEvent(QFocusEvent* event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setEditMode(false);
    emit editingFinished(toPlainText());
    QGraphicsTextItem::focusOutEvent(event);
}

void UBThumbnailTextItem::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        // clearFocus() triggers focusOutEvent → editingFinished
        clearFocus();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape)
    {
        setEditMode(false);
        setTextInteractionFlags(Qt::NoTextInteraction);
        clearFocus();
        event->accept();
        return;
    }
    QGraphicsTextItem::keyPressEvent(event);
}
