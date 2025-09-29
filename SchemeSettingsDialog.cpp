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

    setStyleSheet(QStringLiteral(
        "QDialog{background:#ffffff;}"
        "QLabel{color:#1f2937;}"
        "QLineEdit{border:1px solid #d6dae2;border-radius:8px;padding:6px 10px;font-size:14px;color:#111827;}"
        "QLineEdit:disabled{background:#f3f4f6;color:#9ca3af;}"
        "QPushButton{background:#2563eb;color:#ffffff;border:none;border-radius:8px;padding:8px 14px;font-size:14px;font-weight:600;}"
        "QPushButton:disabled{background:#93c5fd;color:#ffffff;}"
        "QPushButton:hover:!disabled{background:#1d4ed8;}"
        "QPushButton:pressed:!disabled{background:#1e40af;}"
    ));

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 16, 16, 16);
    v->setSpacing(12);

    m_title = new QLabel(tr("正在编辑：%1").arg(schemeName), this);
    m_title->setStyleSheet("font-weight:600;font-size:18px;color:#111827;");
    v->addWidget(m_title);

    auto* nameRow = new QHBoxLayout();
    auto* nameLabel = new QLabel(tr("总成名称："), this);
    nameLabel->setStyleSheet(QStringLiteral("font-size:14px;font-weight:500;color:#374151;"));
    nameRow->addWidget(nameLabel);
    m_nameEdit = new QLineEdit(schemeName, this);
    m_nameEdit->setPlaceholderText(tr("请输入总成名称"));
    nameRow->addWidget(m_nameEdit, 1);
    v->addLayout(nameRow);

    auto* dirRow = new QHBoxLayout();
    dirRow->setSpacing(8);
    auto* dirLabel = new QLabel(tr("工作目录："), this);
    dirLabel->setStyleSheet(QStringLiteral("font-size:14px;font-weight:500;color:#374151;"));
    dirRow->addWidget(dirLabel);
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

    auto* thumbTitle = new QLabel(tr("总成封面"), this);
    thumbTitle->setStyleSheet("font-weight:600;font-size:15px;color:#111827;");
    v->addWidget(thumbTitle);

    auto* thumbRow = new QHBoxLayout();
    thumbRow->setContentsMargins(0, 0, 0, 0);
    thumbRow->setSpacing(12);

    m_thumbnailPreview = new QLabel(this);
    m_thumbnailPreview->setMinimumSize(260, 160);
    m_thumbnailPreview->setAlignment(Qt::AlignCenter);
    m_thumbnailPreview->setWordWrap(true);
    m_thumbnailPreview->setStyleSheet("background:#f9fafc;"
                                      "border:1px dashed #d6dae2;"
                                      "border-radius:10px;"
                                      "color:#6b7280;"
                                      "padding:12px;line-height:20px;");
    thumbRow->addWidget(m_thumbnailPreview, 1);

    auto* thumbButtons = new QVBoxLayout();
    thumbButtons->setContentsMargins(0, 0, 0, 0);
    thumbButtons->setSpacing(12);
    m_thumbnailButton = new QPushButton(tr("选择图片..."), this);
    thumbButtons->addWidget(m_thumbnailButton);
    m_clearThumbnailButton = new QPushButton(tr("清除图片"), this);
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
