#pragma once

#include <QTreeWidget>
#include <QUrl>

class SchemeTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造方案树组件。
     * @param parent 可选的父组件。
     */
    explicit SchemeTreeWidget(QWidget* parent = nullptr);

signals:
    /**
     * @brief 当拖拽导致节点重排后发出。
     */
    void itemsReordered();

    /**
     * @brief 当外部路径被拖放到树上时发出。
     * @param urls 被拖入的 URL 列表。
     * @param targetItem 接收拖拽的树节点。
     */
    void externalPathsDropped(const QList<QUrl>& urls, QTreeWidgetItem* targetItem);

protected:
    /**
     * @brief 处理拖入事件以验证拖拽。
     * @param event 拖入事件信息。
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief 处理拖拽过程中的移动事件。
     * @param event 拖拽移动事件信息。
     */
    void dragMoveEvent(QDragMoveEvent* event) override;

    /**
     * @brief 处理放下事件以区分内部与外部拖拽。
     * @param event 放下事件信息。
     */
    void dropEvent(QDropEvent* event) override;

private:
    /**
     * @brief 判断拖放事件是否为内部移动。
     * @param event 需要检查的放下事件。
     * @return 若为内部移动则返回 true。
     */
    bool isInternalMove(const QDropEvent* event) const;
};
