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
#include <QIcon>

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
        "QToolButton#addButton, QToolButton#openButton{border:none;border-radius:12px;padding:4px;"
        "color:#0b57d0;background:rgba(11,87,208,0.08);}"
        "QToolButton#addButton:hover, QToolButton#openButton:hover{background:rgba(11,87,208,0.16);}"
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

    m_openBtn = new QToolButton(this);
    m_openBtn->setObjectName("openButton");
    m_openBtn->setToolTip(tr("打开方案目录"));
    m_openBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/folder.svg")));
    m_openBtn->setIconSize(QSize(16, 16));
    m_openBtn->setAutoRaise(false);
    m_openBtn->setCursor(Qt::ArrowCursor);
    m_openBtn->setVisible(false);
    header->addWidget(m_openBtn, 0, Qt::AlignRight);

    m_addBtn = new QToolButton(this);
    m_addBtn->setObjectName("addButton");
    m_addBtn->setToolTip(tr("添加到当前工程"));
    m_addBtn->setIcon(QIcon(QStringLiteral(":/icons/icons/add.svg")));
    m_addBtn->setIconSize(QSize(16, 16));
    m_addBtn->setAutoRaise(false);
    m_addBtn->setCursor(Qt::ArrowCursor);
    m_addBtn->setVisible(false);
    header->addWidget(m_addBtn, 0, Qt::AlignRight);

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

    connect(m_addBtn, &QToolButton::clicked, this, [this]() {
        emit addRequested(m_id);
    });
    connect(m_deleteBtn, &QToolButton::clicked, this, [this]() {
        emit deleteRequested(m_id);
    });
    connect(m_openBtn, &QToolButton::clicked, this, [this]() {
        emit openRequested(m_id);
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

void SchemeCardWidget::setHintText(const QString& text)
{
    if (m_hintLabel)
        m_hintLabel->setText(text);
}

void SchemeCardWidget::setAddButtonVisible(bool visible)
{
    if (m_addBtn)
        m_addBtn->setVisible(visible);
}

void SchemeCardWidget::setAddButtonEnabled(bool enabled)
{
    if (m_addBtn)
        m_addBtn->setEnabled(enabled);
}

void SchemeCardWidget::setDeleteButtonVisible(bool visible)
{
    if (m_deleteBtn)
        m_deleteBtn->setVisible(visible);
}

void SchemeCardWidget::setDeleteButtonEnabled(bool enabled)
{
    if (m_deleteBtn)
        m_deleteBtn->setEnabled(enabled);
}

void SchemeCardWidget::setAddButtonToolTip(const QString& text)
{
    if (m_addBtn)
        m_addBtn->setToolTip(text);
}

void SchemeCardWidget::setDeleteButtonToolTip(const QString& text)
{
    if (m_deleteBtn)
        m_deleteBtn->setToolTip(text);
}

void SchemeCardWidget::setOpenButtonVisible(bool visible)
{
    if (m_openBtn)
        m_openBtn->setVisible(visible);
}

void SchemeCardWidget::setOpenButtonEnabled(bool enabled)
{
    if (m_openBtn)
        m_openBtn->setEnabled(enabled);
}

void SchemeCardWidget::setOpenButtonToolTip(const QString& text)
{
    if (m_openBtn)
        m_openBtn->setToolTip(text);
}

void SchemeCardWidget::mousePressEvent(QMouseEvent* ev)
{
    QFrame::mousePressEvent(ev);
}

void SchemeCardWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
    const bool onAdd = isPointInsideButton(m_addBtn, ev->pos());
    const bool onDelete = isPointInsideButton(m_deleteBtn, ev->pos());
    const bool onOpen = isPointInsideButton(m_openBtn, ev->pos());
    if (!onAdd && !onDelete && !onOpen)
    {
        if (m_addBtn && m_addBtn->isVisible() && m_addBtn->isEnabled())
            emit addRequested(m_id);
        else
            emit openRequested(m_id);
    }
    QFrame::mouseDoubleClickEvent(ev);
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


bool SchemeCardWidget::isPointInsideButton(const QToolButton* button, const QPoint& pos) const
{
    if (!button || !button->isVisible())
        return false;
    return button->geometry().contains(pos);
}
