#pragma once

#include <QTreeWidget>
#include <QUrl>

class SchemeTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit SchemeTreeWidget(QWidget* parent = nullptr);

signals:
    void itemsReordered();
    void externalPathsDropped(const QList<QUrl>& urls, QTreeWidgetItem* targetItem);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool isInternalMove(const QDropEvent* event) const;
};
