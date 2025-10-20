#pragma once
#include <QWidget>
#include <QPointer>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class SchemeGalleryWidget; }
QT_END_NAMESPACE

class SchemeCardWidget;

class SchemeGalleryWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief 构造方案画廊组件。
     * @param parent 可选的父组件。
     */
    explicit SchemeGalleryWidget(QWidget *parent = nullptr);

    /**
     * @brief 销毁画廊组件。
     */
    ~SchemeGalleryWidget();

    struct CardOptions {
        bool showDeleteButton = true;
        bool enableDeleteButton = true;
        bool showOpenButton = false;
        bool enableOpenButton = true;
        QString hintText;
        QString deleteToolTip;
        QString openToolTip;
    };

    /**
     * @brief 移除画廊中的全部方案卡片。
     */
    void clearSchemes();

    /**
     * @brief 向画廊添加方案卡片。
     * @param id 方案标识符。
     * @param name 方案显示名称。
     * @param thumb 可选的缩略图。
     * @param options 额外的卡片配置。
     */
    void addScheme(const QString& id,
                   const QString& name = QString(),
                   const QPixmap& thumb = QPixmap(),
                   const CardOptions& options = CardOptions());

    /**
     * @brief 根据标识符移除方案卡片。
     * @param id 需要移除的方案标识符。
     */
    void removeSchemeById(const QString& id);

signals:
    /**
     * @brief 当需要从画廊打开方案时发出。
     * @param id 方案标识符。
     */
    void schemeOpenRequested(const QString& id);

    /**
     * @brief 当需要从画廊删除方案时发出。
     * @param id 方案标识符。
     */
    void schemeDeleteRequested(const QString& id);

    /**
     * @brief 当用户请求查看方案详情时发出。
     * @param id 方案标识符。
     */
    void schemeDetailsRequested(const QString& id);

    /**
     * @brief 当用户希望创建新方案时发出。
     */
    void createSchemeRequested();

protected:
    /**
     * @brief 在组件尺寸变化时重新计算布局。
     * @param e 调整大小事件信息。
     */
    void resizeEvent(QResizeEvent* e) override;

private:
    /**
     * @brief 根据当前几何信息重新排布卡片。
     */
    void relayoutCards();

private:
    Ui::SchemeGalleryWidget *ui;
    QList<QPointer<SchemeCardWidget>> m_cards;
    int m_cardW = 268;                // 估算卡片宽度（含间距）
    int m_lastColumnCount = 0;
};
