#include "SchemeSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>

SchemeSettingsDialog::SchemeSettingsDialog(const QString& schemeName,
                                           const QString& workingDirectory,
                                           bool allowDirectoryChange,
                                           QWidget* parent)
    : QDialog(parent)
    , m_directoryEditable(allowDirectoryChange)
{
    setWindowTitle(tr("方案设置"));
    resize(480, 240);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(12);

    m_title = new QLabel(tr("正在编辑：%1").arg(schemeName), this);
    m_title->setStyleSheet("font-weight:600;font-size:14px;");
    v->addWidget(m_title);

    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(tr("方案名称："), this));
    m_nameEdit = new QLineEdit(schemeName, this);
    m_nameEdit->setPlaceholderText(tr("请输入方案名称"));
    nameRow->addWidget(m_nameEdit, 1);
    v->addLayout(nameRow);

    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(new QLabel(tr("工作目录："), this));
    m_directoryEdit = new QLineEdit(workingDirectory, this);
    m_directoryEdit->setPlaceholderText(tr("请选择模型计算的工作目录"));
    m_directoryEdit->setReadOnly(!allowDirectoryChange);
    dirRow->addWidget(m_directoryEdit, 1);
    m_browseButton = new QPushButton(tr("浏览..."), this);
    m_browseButton->setEnabled(allowDirectoryChange);
    dirRow->addWidget(m_browseButton);
    v->addLayout(dirRow);

    if (allowDirectoryChange)
        connect(m_browseButton, &QPushButton::clicked, this, &SchemeSettingsDialog::browseForDirectory);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(btns);
}

QString SchemeSettingsDialog::schemeName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

QString SchemeSettingsDialog::workingDirectory() const
{
    return m_directoryEdit ? QDir::cleanPath(m_directoryEdit->text().trimmed()) : QString();
}

void SchemeSettingsDialog::setSchemeName(const QString& name)
{
    if (m_nameEdit)
        m_nameEdit->setText(name);
    if (m_title)
        m_title->setText(tr("正在编辑：%1").arg(name));
}

void SchemeSettingsDialog::setWorkingDirectory(const QString& directory)
{
    if (m_directoryEdit)
        m_directoryEdit->setText(QDir::toNativeSeparators(directory));
}

void SchemeSettingsDialog::browseForDirectory()
{
    if (!m_directoryEditable)
        return;

    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择工作目录"),
                                                          workingDirectory());
    if (!dir.isEmpty())
        setWorkingDirectory(dir);
}
