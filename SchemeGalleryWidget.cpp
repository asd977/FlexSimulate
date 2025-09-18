#include "SchemeGalleryWidget.h"
#include "ui_SchemeGalleryWidget.h"   // 由 .ui 生成
#include "SchemeCardWidget.h"

#include <QGridLayout>
#include <QScrollArea>
#include <QPixmap>

SchemeGalleryWidget::SchemeGalleryWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::SchemeGalleryWidget)
{
    ui->setupUi(this);

    connect(ui->addButton, &QPushButton::clicked,
            this, &SchemeGalleryWidget::addSchemeRequested);
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
                                    const QPixmap& thumb) {
    if (id.isEmpty())
        return;

    removeSchemeById(id);

    // 创建卡片
    auto* card = new SchemeCardWidget(id, this);
    card->setTitle(name.isEmpty() ? QStringLiteral("未命名方案") : name);
    if (!thumb.isNull()) card->setThumbnail(thumb);
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // 放进网格
    auto* grid = qobject_cast<QGridLayout*>(ui->gridLayout);
    grid->addWidget(card, 0, 0); // 先随便放，稍后统一 relayout
    m_cards.push_back(card);

    // 打开设置
    connect(card, &SchemeCardWidget::openRequested,
            this, &SchemeGalleryWidget::schemeOpenRequested);

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

    // 计算列数（根据可视宽度自适应换行）
    int viewportW = ui->scrollArea->viewport()->width();
    int hSpacing = grid->horizontalSpacing();
    int contents = grid->contentsMargins().left() + grid->contentsMargins().right();
    int cols = qMax(1, (viewportW - contents + hSpacing) / (m_cardW + hSpacing));

    int row = 0, col = 0;
    for (QPointer<SchemeCardWidget> card : m_cards) {
        if (!card) continue;
        grid->addWidget(card, row, col);
        col++;
        if (col >= cols) { col = 0; row++; }
    }
}
