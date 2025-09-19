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
    void setHintText(const QString& text);
    void setAddButtonVisible(bool visible);
    void setAddButtonEnabled(bool enabled);
    void setDeleteButtonVisible(bool visible);
    void setDeleteButtonEnabled(bool enabled);
    void setAddButtonToolTip(const QString& text);
    void setDeleteButtonToolTip(const QString& text);
    QString id() const { return m_id; }

signals:
    void openRequested(const QString& id);
    void addRequested(const QString& id);
    void deleteRequested(const QString& id);

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    void updateThumbnailDisplay();

    QString m_id;
    QLabel* m_imageLabel;
    QLabel* m_titleLabel;
    QToolButton* m_addBtn;
    QToolButton* m_deleteBtn;
    QLabel* m_hintLabel;
    QPixmap m_thumbnail;
};
