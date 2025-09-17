#include "SchemeSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>

SchemeSettingsDialog::SchemeSettingsDialog(const QString& schemeName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("方案设置");
    resize(420, 220);

    auto *v = new QVBoxLayout(this);
    m_title = new QLabel(QString("正在编辑：%1").arg(schemeName), this);
    m_title->setStyleSheet("font-weight:600;font-size:14px;");
    v->addWidget(m_title);

    auto *row = new QHBoxLayout();
    row->addWidget(new QLabel("方案名称：", this));
    m_nameEdit = new QLineEdit(schemeName, this);
    row->addWidget(m_nameEdit);
    v->addLayout(row);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(btns);
}
