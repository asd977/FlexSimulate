#pragma once

#include <QTreeWidget>
#include <QUrl>

class SchemeTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the scheme tree widget.
     * @param parent Optional parent widget.
     */
    explicit SchemeTreeWidget(QWidget* parent = nullptr);

signals:
    /**
     * @brief Emitted after items are reordered via drag and drop.
     */
    void itemsReordered();

    /**
     * @brief Emitted when external paths are dropped onto the tree.
     * @param urls Dropped URLs.
     * @param targetItem Item that received the drop.
     */
    void externalPathsDropped(const QList<QUrl>& urls, QTreeWidgetItem* targetItem);

protected:
    /**
     * @brief Handles drag enter events to validate drops.
     * @param event Drag enter event information.
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief Handles drag move events during drag and drop.
     * @param event Drag move event information.
     */
    void dragMoveEvent(QDragMoveEvent* event) override;

    /**
     * @brief Handles drop events to process internal or external drops.
     * @param event Drop event information.
     */
    void dropEvent(QDropEvent* event) override;

private:
    /**
     * @brief Determines whether a drop event represents an internal move.
     * @param event Drop event to inspect.
     * @return True if the drop is an internal move.
     */
    bool isInternalMove(const QDropEvent* event) const;
};
