#include "ProjectCreationDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDir>

ProjectCreationDialog::ProjectCreationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("新建工程"));
    resize(440, 180);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    auto* nameLabel = new QLabel(tr("工程名称："), this);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("请输入工程名称"));
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    auto* dirRow = new QHBoxLayout();
    dirRow->setSpacing(8);
    auto* dirLabel = new QLabel(tr("工作目录："), this);
    m_directoryEdit = new QLineEdit(this);
    m_directoryEdit->setPlaceholderText(tr("请选择工程所在目录"));
    m_browseButton = new QPushButton(tr("浏览..."), this);
    connect(m_browseButton, &QPushButton::clicked,
            this, &ProjectCreationDialog::browseForDirectory);
    dirRow->addWidget(dirLabel);
    dirRow->addWidget(m_directoryEdit, 1);
    dirRow->addWidget(m_browseButton);
    layout->addLayout(dirRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProjectCreationDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ProjectCreationDialog::reject);
    layout->addWidget(buttons);
}

QString ProjectCreationDialog::projectName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

QString ProjectCreationDialog::projectDirectory() const
{
    return m_directoryEdit ? m_directoryEdit->text().trimmed() : QString();
}

void ProjectCreationDialog::setInitialName(const QString& name)
{
    if (m_nameEdit)
        m_nameEdit->setText(name);
}

void ProjectCreationDialog::setInitialDirectory(const QString& directory)
{
    if (m_directoryEdit)
        m_directoryEdit->setText(QDir::toNativeSeparators(directory));
}

void ProjectCreationDialog::browseForDirectory()
{
    const QString current = projectDirectory();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择工程目录"),
                                                          current.isEmpty() ? QDir::homePath()
                                                                            : current);
    if (!dir.isEmpty() && m_directoryEdit)
        m_directoryEdit->setText(QDir::toNativeSeparators(dir));
}

