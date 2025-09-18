#include "SchemeTreeWidget.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

SchemeTreeWidget::SchemeTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropMode(QAbstractItemView::DragDrop);
}

void SchemeTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() || event->source() == this)
    {
        event->acceptProposedAction();
        return;
    }
    QTreeWidget::dragEnterEvent(event);
}

void SchemeTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls() || event->source() == this)
    {
        event->acceptProposedAction();
        return;
    }
    QTreeWidget::dragMoveEvent(event);
}

bool SchemeTreeWidget::isInternalMove(const QDropEvent* event) const
{
    return event->source() == this &&
           event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist");
}

void SchemeTreeWidget::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls() && event->source() != this)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint pos = event->position().toPoint();
#else
        const QPoint pos = event->pos();
#endif
        QTreeWidgetItem* target = itemAt(pos);
        emit externalPathsDropped(event->mimeData()->urls(), target);
        event->acceptProposedAction();
        return;
    }

    QTreeWidget::dropEvent(event);

    if (isInternalMove(event))
    {
        emit itemsReordered();
    }
}
