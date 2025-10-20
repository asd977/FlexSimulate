#pragma once
#include <QFrame>
#include <QPixmap>

class QLabel;
class QToolButton;

class SchemeCardWidget : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief 构造用于展示指定方案的卡片组件。
     * @param id 方案的唯一标识符。
     * @param parent 可选的父组件。
     */
    explicit SchemeCardWidget(const QString& id, QWidget* parent=nullptr);

    /**
     * @brief 设置卡片展示的标题文本。
     * @param title 需要显示的标题。
     */
    void setTitle(const QString& title);

    /**
     * @brief 获取当前卡片标题。
     * @return 标题文本。
     */
    QString title() const;

    /**
     * @brief 更新卡片展示的缩略图。
     * @param pm 缩略图像。
     */
    void setThumbnail(const QPixmap& pm);

    /**
     * @brief 设置标题下方的辅助提示文本。
     * @param text 需要显示的提示内容。
     */
    void setHintText(const QString& text);

    /**
     * @brief 控制删除按钮的可见性。
     * @param visible 按钮是否可见。
     */
    void setDeleteButtonVisible(bool visible);

    /**
     * @brief 启用或禁用删除按钮。
     * @param enabled 是否允许点击。
     */
    void setDeleteButtonEnabled(bool enabled);

    /**
     * @brief 设置删除按钮的提示文本。
     * @param text 提示内容。
     */
    void setDeleteButtonToolTip(const QString& text);

    /**
     * @brief 控制打开按钮的可见性。
     * @param visible 按钮是否可见。
     */
    void setOpenButtonVisible(bool visible);

    /**
     * @brief 启用或禁用打开按钮。
     * @param enabled 是否允许点击。
     */
    void setOpenButtonEnabled(bool enabled);

    /**
     * @brief 设置打开按钮的提示文本。
     * @param text 提示内容。
     */
    void setOpenButtonToolTip(const QString& text);

    /**
     * @brief 获取卡片关联的方案标识符。
     * @return 方案标识符。
     */
    QString id() const { return m_id; }

signals:
    /**
     * @brief 当需要打开方案时发出。
     * @param id 方案标识符。
     */
    void openRequested(const QString& id);

    /**
     * @brief 当用户请求删除方案时发出。
     * @param id 方案标识符。
     */
    void deleteRequested(const QString& id);

    /**
     * @brief 当用户请求查看方案详情时发出。
     * @param id 方案标识符。
     */
    void detailsRequested(const QString& id);

protected:
    /**
     * @brief 处理鼠标按下以触发详情请求。
     * @param ev 鼠标事件信息。
     */
    void mousePressEvent(QMouseEvent* ev) override;

    /**
     * @brief 处理鼠标双击以快速打开方案。
     * @param ev 鼠标事件信息。
     */
    void mouseDoubleClickEvent(QMouseEvent* ev) override;

    /**
     * @brief 响应尺寸变更以保持布局一致。
     * @param ev 调整大小事件信息。
     */
    void resizeEvent(QResizeEvent* ev) override;

private:
    /**
     * @brief 更新缩略图显示以保持纵横比。
     */
    void updateThumbnailDisplay();

    /**
     * @brief 判断坐标点是否位于工具按钮内部。
     * @param button 需要检测的按钮。
     * @param pos 组件坐标系中的位置。
     * @return 若点在按钮内返回 true。
     */
    bool isPointInsideButton(const QToolButton* button, const QPoint& pos) const;

    QString m_id;
    QLabel* m_imageLabel;
    QLabel* m_titleLabel;
    QToolButton* m_deleteBtn;
    QToolButton* m_openBtn;
    QLabel* m_hintLabel;
    QPixmap m_thumbnail;
};
