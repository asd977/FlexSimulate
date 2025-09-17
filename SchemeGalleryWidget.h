#pragma once
#include <QWidget>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class SchemeGalleryWidget; }
QT_END_NAMESPACE

class SchemeCardWidget;

class SchemeGalleryWidget : public QWidget {
    Q_OBJECT
public:
    explicit SchemeGalleryWidget(QWidget *parent = nullptr);
    ~SchemeGalleryWidget();

public slots:
    void addScheme(const QString& name = QString(), const QPixmap& thumb = QPixmap());
    void removeSchemeById(const QString& id);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void relayoutCards();

private:
    Ui::SchemeGalleryWidget *ui;
    QList<QPointer<SchemeCardWidget>> m_cards;
    int m_cardW = 236;                // 估算卡片宽度（含间距）
};
