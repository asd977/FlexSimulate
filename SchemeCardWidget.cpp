#include "SchemeCardWidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QStyle>

SchemeCardWidget::SchemeCardWidget(const QString& id, QWidget* parent)
    : QFrame(parent), m_id(id)
{
    setObjectName("schemeCard");
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(
        "#schemeCard{background:#ffffff;border:1px solid #e5e5e5;"
        "border-radius:10px;}"
        "#schemeCard:hover{border:1px solid #1787ff;}"
        "QLabel#title{padding:6px 8px;font-weight:600;}"
    );
    setMinimumSize(220, 220);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8,8,8,8);
    lay->setSpacing(6);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setMinimumSize(200,140);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background:#f6f7fb;border-radius:6px;");
    m_imageLabel->setScaledContents(false);
    lay->addWidget(m_imageLabel, /*stretch*/1);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("title");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    lay->addWidget(m_titleLabel);

    m_deleteBtn = new QToolButton(this);
    m_deleteBtn->setToolTip("删除此方案");
    m_deleteBtn->setText("×");
    m_deleteBtn->setAutoRaise(true);
    m_deleteBtn->setStyleSheet("QToolButton{font-size:16px;}");
    m_deleteBtn->setFixedSize(24,24);
    m_deleteBtn->move(width()-28, 4);
    m_deleteBtn->raise();

    connect(m_deleteBtn, &QToolButton::clicked, this, [this](){
        emit deleteRequested(m_id);
    });
}

void SchemeCardWidget::setTitle(const QString& title) {
    m_titleLabel->setText(title);
}
QString SchemeCardWidget::title() const { return m_titleLabel->text(); }

void SchemeCardWidget::setThumbnail(const QPixmap& pm) {
    // 居中等比显示
    QPixmap scaled = pm.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(scaled);
}

void SchemeCardWidget::mousePressEvent(QMouseEvent* ev) {
    if (!m_deleteBtn->geometry().contains(ev->pos())) {
        emit openRequested(m_id);
    }
    QFrame::mousePressEvent(ev);
}
