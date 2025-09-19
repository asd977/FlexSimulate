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
    explicit SchemeGalleryWidget(QWidget *parent = nullptr);
    ~SchemeGalleryWidget();

    struct CardOptions {
        bool showAddButton = false;
        bool enableAddButton = true;
        bool showDeleteButton = true;
        bool enableDeleteButton = true;
        bool showOpenButton = false;
        bool enableOpenButton = true;
        QString hintText;
        QString addToolTip;
        QString deleteToolTip;
        QString openToolTip;
    };

    void clearSchemes();
    void addScheme(const QString& id,
                   const QString& name = QString(),
                   const QPixmap& thumb = QPixmap(),
                   const CardOptions& options = CardOptions());
    void removeSchemeById(const QString& id);

signals:
    void schemeOpenRequested(const QString& id);
    void schemeAddRequested(const QString& id);
    void schemeDeleteRequested(const QString& id);
    void createSchemeRequested();

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void relayoutCards();

private:
    Ui::SchemeGalleryWidget *ui;
    QList<QPointer<SchemeCardWidget>> m_cards;
    int m_cardW = 268;                // 估算卡片宽度（含间距）
    int m_lastColumnCount = 0;
};
