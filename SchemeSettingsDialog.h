#pragma once
#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;

class SchemeSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SchemeSettingsDialog(const QString& schemeName,
                                  const QString& workingDirectory = QString(),
                                  bool allowDirectoryChange = true,
                                  QWidget* parent = nullptr,
                                  const QString& thumbnailPath = QString());

    QString schemeName() const;
    QString workingDirectory() const;
    QString thumbnailPath() const;
    void setSchemeName(const QString& name);
    void setWorkingDirectory(const QString& directory);
    void setThumbnailPath(const QString& path);
    void setDirectoryHint(const QString& hint);

private slots:
    void browseForDirectory();
    void browseForThumbnail();
    void clearThumbnail();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateThumbnailPreview();

    QLabel* m_title = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_thumbnailPreview = nullptr;
    QPushButton* m_thumbnailButton = nullptr;
    QPushButton* m_clearThumbnailButton = nullptr;
    QString m_thumbnailPath;
    bool m_directoryEditable = true;
};
