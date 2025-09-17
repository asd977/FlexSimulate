#pragma once
#include <QFrame>
#include <QPixmap>

class QLabel;
class QToolButton;

class SchemeCardWidget : public QFrame {
    Q_OBJECT
public:
    explicit SchemeCardWidget(const QString& id, QWidget* parent=nullptr);

    void setTitle(const QString& title);
    QString title() const;

    void setThumbnail(const QPixmap& pm);
    QString id() const { return m_id; }

signals:
    void openRequested(const QString& id);
    void deleteRequested(const QString& id);

protected:
    void mousePressEvent(QMouseEvent* ev) override;

private:
    QString m_id;
    QLabel* m_imageLabel;
    QLabel* m_titleLabel;
    QToolButton* m_deleteBtn;
};
