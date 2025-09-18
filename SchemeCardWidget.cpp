#include "SchemeCardWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QSizePolicy>
#include <QSizeF>
#include <QGraphicsDropShadowEffect>
#include <QColor>

SchemeCardWidget::SchemeCardWidget(const QString& id, QWidget* parent)
    : QFrame(parent), m_id(id)
{
    setObjectName("schemeCard");
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(
        "#schemeCard{background:#ffffff;border:1px solid #e4e7f1;"
        "border-radius:14px;}"
        "#schemeCard:hover{border:1px solid #1787ff;}"
        "QLabel#titleLabel{font-weight:600;font-size:15px;color:#1b2b4d;}"
        "QLabel#hintLabel{color:#8a93a6;font-size:12px;}"
        "QLabel#imageLabel{background:#f6f7fb;border-radius:12px;"
        "border:1px dashed #d0d6e5;color:#8a93a6;font-size:13px;"
        "padding:12px;line-height:20px;}"
        "QToolButton#deleteButton{border:none;border-radius:12px;"
        "padding:4px;color:#d93025;"
        "background:rgba(217,48,37,0.08);}"
        "QToolButton#deleteButton:hover{background:rgba(217,48,37,0.16);}" );

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(27, 43, 77, 30));
    setGraphicsEffect(shadow);

    setMinimumSize(240, 300);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(16,16,16,16);
    lay->setSpacing(12);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0,0,0,0);
    header->setSpacing(8);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setText(tr("未命名方案"));
    header->addWidget(m_titleLabel, 1);

    m_deleteBtn = new QToolButton(this);
    m_deleteBtn->setObjectName("deleteButton");
    m_deleteBtn->setToolTip(tr("删除此方案"));
    m_deleteBtn->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    m_deleteBtn->setIconSize(QSize(16, 16));
    m_deleteBtn->setAutoRaise(false);
    m_deleteBtn->setCursor(Qt::ArrowCursor);
    header->addWidget(m_deleteBtn, 0, Qt::AlignRight);

    lay->addLayout(header);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setObjectName("imageLabel");
    m_imageLabel->setMinimumSize(220,160);
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setWordWrap(true);
    m_imageLabel->setText(tr("暂无封面"));
    lay->addWidget(m_imageLabel, /*stretch*/1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName("hintLabel");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setText(tr("点击卡片以查看详情"));
    lay->addWidget(m_hintLabel);

    connect(m_deleteBtn, &QToolButton::clicked, this, [this](){
        emit deleteRequested(m_id);
    });
}

void SchemeCardWidget::setTitle(const QString& title) {
    m_titleLabel->setText(title);
}
QString SchemeCardWidget::title() const { return m_titleLabel->text(); }

void SchemeCardWidget::setThumbnail(const QPixmap& pm) {
    m_thumbnail = pm;
    updateThumbnailDisplay();
}

void SchemeCardWidget::mousePressEvent(QMouseEvent* ev) {
    if (!m_deleteBtn->geometry().contains(ev->pos())) {
        emit openRequested(m_id);
    }
    QFrame::mousePressEvent(ev);
}

void SchemeCardWidget::resizeEvent(QResizeEvent* ev)
{
    QFrame::resizeEvent(ev);
    updateThumbnailDisplay();
}

void SchemeCardWidget::updateThumbnailDisplay()
{
    if (!m_imageLabel)
        return;

    if (m_thumbnail.isNull())
    {
        m_imageLabel->setPixmap(QPixmap());
        m_imageLabel->setText(tr("暂无封面"));
        return;
    }

    m_imageLabel->setText(QString());
    const QSize labelSize = m_imageLabel->size();
    if (labelSize.width() <= 0 || labelSize.height() <= 0)
        return;

    const qreal ratio = devicePixelRatioF();
    const QSize targetSize = (QSizeF(labelSize) * ratio).toSize();
    QPixmap scaled = m_thumbnail.scaled(targetSize, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(ratio);
    m_imageLabel->setPixmap(scaled);
}
