#include "SchemeGalleryWidget.h"
#include "ui_SchemeGalleryWidget.h"   // 由 .ui 生成
#include "SchemeCardWidget.h"

#include <QFrame>
#include <QGridLayout>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>

#include <algorithm>

SchemeGalleryWidget::SchemeGalleryWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::SchemeGalleryWidget)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_StyledBackground, true);
    if (ui->headerFrame)
        ui->headerFrame->setAttribute(Qt::WA_StyledBackground, true);
    if (ui->scrollArea)
    {
        ui->scrollArea->setFrameShape(QFrame::NoFrame);
        ui->scrollArea->setAttribute(Qt::WA_StyledBackground, true);
        if (auto* viewport = ui->scrollArea->viewport())
        {
            viewport->setAttribute(Qt::WA_StyledBackground, true);
            viewport->setStyleSheet(QStringLiteral("background:transparent;"));
        }
        if (auto* contents = ui->scrollArea->widget())
            contents->setAttribute(Qt::WA_StyledBackground, true);
    }

    const QString style = QStringLiteral(
        "QWidget#SchemeGalleryWidget{background:#ffffff;border:1px solid #d6e1f2;border-radius:14px;}"
        "QFrame#headerFrame{background:#f8fafc;border-top-left-radius:14px;border-top-right-radius:14px;"
        "border-bottom:1px solid #e2e8f0;}"
        "QLabel#titleLabel{font-size:15px;font-weight:600;color:#0f172a;padding:12px 16px;}"
        "QPushButton#newSchemeButton{padding:6px 14px;border-radius:16px;background-color:#1d4ed8;color:white;"
        "font-weight:600;margin:8px 16px 8px 0;}"
        "QPushButton#newSchemeButton:hover{background-color:#2563eb;}"
        "QPushButton#newSchemeButton:pressed{background-color:#1e3a8a;}"
        "QScrollArea{border:none;background:transparent;}"
        "QWidget#scrollAreaWidgetContents{background:transparent;}"
    );
    setStyleSheet(style);

    if (ui->newSchemeButton)
    {
        ui->newSchemeButton->setCursor(Qt::PointingHandCursor);
        connect(ui->newSchemeButton, &QPushButton::clicked,
                this, &SchemeGalleryWidget::createSchemeRequested);
    }
}

SchemeGalleryWidget::~SchemeGalleryWidget() { delete ui; }

void SchemeGalleryWidget::clearSchemes()
{
    auto* grid = qobject_cast<QGridLayout*>(ui->gridLayout);
    for (auto& card : m_cards) {
        if (card) {
            grid->removeWidget(card);
            card->deleteLater();
        }
    }
    m_cards.clear();
    relayoutCards();
}

void SchemeGalleryWidget::addScheme(const QString& id,
                                    const QString& name,
                                    const QPixmap& thumb,
                                    const CardOptions& options) {
    if (id.isEmpty())
        return;

    removeSchemeById(id);

    // 创建卡片
    auto* card = new SchemeCardWidget(id, this);
    card->setTitle(name.isEmpty() ? QStringLiteral("未命名方案") : name);
    if (!thumb.isNull()) card->setThumbnail(thumb);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    if (!options.hintText.isEmpty())
        card->setHintText(options.hintText);
    card->setDeleteButtonVisible(options.showDeleteButton);
    card->setDeleteButtonEnabled(options.enableDeleteButton);
    if (!options.deleteToolTip.isEmpty())
        card->setDeleteButtonToolTip(options.deleteToolTip);
    card->setOpenButtonVisible(options.showOpenButton);
    card->setOpenButtonEnabled(options.enableOpenButton);
    if (!options.openToolTip.isEmpty())
        card->setOpenButtonToolTip(options.openToolTip);

    // 放进网格
    auto* grid = qobject_cast<QGridLayout*>(ui->gridLayout);
    grid->addWidget(card, 0, 0); // 先随便放，稍后统一 relayout
    m_cards.push_back(card);

    // 打开设置
    connect(card, &SchemeCardWidget::openRequested,
            this, &SchemeGalleryWidget::schemeOpenRequested);

    connect(card, &SchemeCardWidget::detailsRequested,
            this, &SchemeGalleryWidget::schemeDetailsRequested);

    // 删除
    connect(card, &SchemeCardWidget::deleteRequested,
            this, &SchemeGalleryWidget::schemeDeleteRequested);

    relayoutCards();
}

void SchemeGalleryWidget::removeSchemeById(const QString& id) {
    auto* grid = qobject_cast<QGridLayout*>(ui->gridLayout);
    for (int i = 0; i < m_cards.size(); ++i) {
        if (m_cards[i] && m_cards[i]->id() == id) {
            QWidget* w = m_cards[i];
            m_cards.removeAt(i);
            if (w) {
                grid->removeWidget(w);
                w->deleteLater();
            }
            break;
        }
    }
    relayoutCards();
}

void SchemeGalleryWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayoutCards();
}

void SchemeGalleryWidget::relayoutCards() {
    auto* grid = qobject_cast<QGridLayout*>(ui->gridLayout);
    // 先清空布局项（不删除控件）
    while (grid->count() > 0) {
        QLayoutItem* it = grid->takeAt(0);
        // 不 delete it->widget()，只移除布局项
        delete it;
    }

    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // 计算列数（根据可视宽度自适应换行）
    int viewportW = ui->scrollArea->viewport()->width();
    int hSpacing = grid->horizontalSpacing();
    int contents = grid->contentsMargins().left() + grid->contentsMargins().right();
    int cardWidth = m_cardW;
    for (const auto& card : m_cards) {
        if (card) {
            cardWidth = qMax(cardWidth, card->sizeHint().width());
            break;
        }
    }

    int cols = qMax(1, (viewportW - contents + hSpacing) / (cardWidth + hSpacing));
    const int cardCount = std::max(1, static_cast<int>(m_cards.size()));
    cols = qMin(cols, cardCount);

    for (int c = 0; c < m_lastColumnCount; ++c)
        grid->setColumnStretch(c, 0);
    m_lastColumnCount = cols;
    for (int c = 0; c < m_lastColumnCount; ++c)
        grid->setColumnStretch(c, 1);

    int row = 0, col = 0;
    for (QPointer<SchemeCardWidget> card : m_cards) {
        if (!card) continue;
        grid->addWidget(card, row, col);
        col++;
        if (col >= cols) { col = 0; row++; }
    }

    grid->invalidate();
    grid->activate();
}
