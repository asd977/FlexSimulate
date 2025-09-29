#include "SchemeSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QImageReader>
#include <QResizeEvent>
#include <QFileInfo>
#include <QPixmap>

namespace
{
const QString kPrimaryTextColor = QStringLiteral("#1f2937");
const QString kSecondaryTextColor = QStringLiteral("#6b7280");
const QString kMutedTextColor = QStringLiteral("#9ca3af");
const QString kBorderColor = QStringLiteral("#d1d5db");
const QString kAccentColor = QStringLiteral("#2d5cf6");

QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{padding:8px 18px;border-radius:8px;border:none;"
        "background:%1;color:#ffffff;font-weight:600;}"
        "QPushButton:hover{background:#2448d8;}"
        "QPushButton:pressed{background:#1b36ad;}"
        "QPushButton:disabled{background:#bfc7f5;color:#e9edff;}"
    ).arg(kAccentColor);
}

QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton{padding:8px 18px;border-radius:8px;"
        "border:1px solid %1;background:#ffffff;color:%2;font-weight:500;}"
        "QPushButton:hover{background:#f3f4f6;}"
        "QPushButton:pressed{background:#e5e7eb;}"
        "QPushButton:disabled{color:%3;border-color:#e5e7eb;}"
    ).arg(kBorderColor, kPrimaryTextColor, kMutedTextColor);
}
}

SchemeSettingsDialog::SchemeSettingsDialog(const QString& schemeName,
                                           const QString& workingDirectory,
                                           bool allowDirectoryChange,
                                           QWidget* parent,
                                           const QString& thumbnailPath)
    : QDialog(parent)
    , m_directoryEditable(allowDirectoryChange)
{
    setWindowTitle(tr("总成设置"));
    resize(520, 360);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(12);

    m_title = new QLabel(tr("正在编辑：%1").arg(schemeName), this);
    m_title->setStyleSheet(QStringLiteral("font-weight:600;font-size:16px;color:%1;")
                               .arg(kPrimaryTextColor));
    v->addWidget(m_title);

    auto* nameRow = new QHBoxLayout();
    auto* nameLabel = new QLabel(tr("总成名称："), this);
    nameLabel->setStyleSheet(QStringLiteral("color:%1;font-weight:500;").arg(kPrimaryTextColor));
    nameRow->addWidget(nameLabel);
    m_nameEdit = new QLineEdit(schemeName, this);
    m_nameEdit->setPlaceholderText(tr("请输入总成名称"));
    m_nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{border:1px solid %1;border-radius:6px;padding:6px 8px;color:%2;}"
        "QLineEdit:focus{border-color:%3;}"
    ).arg(kBorderColor, kPrimaryTextColor, kAccentColor));
    nameRow->addWidget(m_nameEdit, 1);
    v->addLayout(nameRow);

    auto* dirRow = new QHBoxLayout();
    dirRow->setSpacing(8);
    auto* dirLabel = new QLabel(tr("工作目录："), this);
    dirLabel->setStyleSheet(QStringLiteral("color:%1;font-weight:500;").arg(kPrimaryTextColor));
    dirRow->addWidget(dirLabel);
    m_directoryEdit = new QLineEdit(workingDirectory, this);
    m_directoryEdit->setPlaceholderText(tr("请选择模型计算的工作目录"));
    m_directoryEdit->setReadOnly(!allowDirectoryChange);
    m_directoryEdit->setStyleSheet(QStringLiteral(
        "QLineEdit{border:1px solid %1;border-radius:6px;padding:6px 8px;color:%2;background:%4;}"
        "QLineEdit:read-only{color:%3;}"
        "QLineEdit:focus{border-color:%5;}"
    ).arg(kBorderColor, kPrimaryTextColor, kSecondaryTextColor, allowDirectoryChange ? QStringLiteral("#ffffff") : QStringLiteral("#f5f6f8"), kAccentColor));
    dirRow->addWidget(m_directoryEdit, 1);
    m_browseButton = new QPushButton(tr("浏览..."), this);
    m_browseButton->setEnabled(allowDirectoryChange);
    m_browseButton->setStyleSheet(secondaryButtonStyle());
    dirRow->addWidget(m_browseButton);
    v->addLayout(dirRow);

    if (allowDirectoryChange)
        connect(m_browseButton, &QPushButton::clicked, this, &SchemeSettingsDialog::browseForDirectory);

    auto* thumbTitle = new QLabel(tr("总成封面"), this);
    thumbTitle->setStyleSheet(QStringLiteral("font-weight:600;color:%1;").arg(kPrimaryTextColor));
    v->addWidget(thumbTitle);

    auto* thumbRow = new QHBoxLayout();
    thumbRow->setContentsMargins(0, 0, 0, 0);
    thumbRow->setSpacing(12);

    m_thumbnailPreview = new QLabel(this);
    m_thumbnailPreview->setMinimumSize(260, 160);
    m_thumbnailPreview->setAlignment(Qt::AlignCenter);
    m_thumbnailPreview->setWordWrap(true);
    m_thumbnailPreview->setStyleSheet(QStringLiteral(
        "background:#f8f9fb;"
        "border:1px solid %1;"
        "border-radius:8px;"
        "color:%2;"
        "padding:12px;line-height:20px;"
    ).arg(kBorderColor, kMutedTextColor));
    thumbRow->addWidget(m_thumbnailPreview, 1);

    auto* thumbButtons = new QVBoxLayout();
    thumbButtons->setContentsMargins(0, 0, 0, 0);
    thumbButtons->setSpacing(8);
    m_thumbnailButton = new QPushButton(tr("选择图片..."), this);
    m_thumbnailButton->setStyleSheet(primaryButtonStyle());
    thumbButtons->addWidget(m_thumbnailButton);
    m_clearThumbnailButton = new QPushButton(tr("清除图片"), this);
    m_clearThumbnailButton->setStyleSheet(secondaryButtonStyle());
    thumbButtons->addWidget(m_clearThumbnailButton);
    thumbButtons->addStretch(1);
    thumbRow->addLayout(thumbButtons, 0);

    v->addLayout(thumbRow);

    connect(m_thumbnailButton, &QPushButton::clicked,
            this, &SchemeSettingsDialog::browseForThumbnail);
    connect(m_clearThumbnailButton, &QPushButton::clicked,
            this, &SchemeSettingsDialog::clearThumbnail);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(btns);

    setThumbnailPath(thumbnailPath);
}

QString SchemeSettingsDialog::schemeName() const
{
    return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

QString SchemeSettingsDialog::workingDirectory() const
{
    return m_directoryEdit ? QDir::cleanPath(m_directoryEdit->text().trimmed()) : QString();
}

QString SchemeSettingsDialog::thumbnailPath() const
{
    return m_thumbnailPath;
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

void SchemeSettingsDialog::setDirectoryHint(const QString& hint)
{
    if (m_directoryEdit)
        m_directoryEdit->setPlaceholderText(hint);
}

void SchemeSettingsDialog::setThumbnailPath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        m_thumbnailPath.clear();
    else
        m_thumbnailPath = QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
    updateThumbnailPreview();
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

void SchemeSettingsDialog::browseForThumbnail()
{
    const QString initialDir = m_thumbnailPath.isEmpty()
                                   ? workingDirectory()
                                   : QFileInfo(m_thumbnailPath).absolutePath();
    const QString file = QFileDialog::getOpenFileName(
        this, tr("选择封面图片"), initialDir,
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"));
    if (!file.isEmpty())
        setThumbnailPath(file);
}

void SchemeSettingsDialog::clearThumbnail()
{
    if (m_thumbnailPath.isEmpty())
        return;
    m_thumbnailPath.clear();
    updateThumbnailPreview();
}

void SchemeSettingsDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateThumbnailPreview();
}

void SchemeSettingsDialog::updateThumbnailPreview()
{
    if (!m_thumbnailPreview)
        return;

    QPixmap pixmap;
    if (!m_thumbnailPath.isEmpty())
    {
        QImageReader reader(m_thumbnailPath);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (!image.isNull())
            pixmap = QPixmap::fromImage(image);
    }

    if (pixmap.isNull())
    {
        m_thumbnailPreview->setPixmap(QPixmap());
        m_thumbnailPreview->setText(tr("尚未选择封面图片"));
    }
    else
    {
        m_thumbnailPreview->setText(QString());
        const QSize labelSize = m_thumbnailPreview->size();
        if (labelSize.width() > 0 && labelSize.height() > 0)
        {
            const QPixmap scaled = pixmap.scaled(labelSize, Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);
            m_thumbnailPreview->setPixmap(scaled);
        }
    }

    if (m_clearThumbnailButton)
        m_clearThumbnailButton->setEnabled(!m_thumbnailPath.isEmpty());
}
