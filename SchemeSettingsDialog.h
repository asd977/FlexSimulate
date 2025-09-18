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
                                  QWidget* parent = nullptr);

    QString schemeName() const;
    QString workingDirectory() const;
    void setSchemeName(const QString& name);
    void setWorkingDirectory(const QString& directory);

private slots:
    void browseForDirectory();

private:
    QLabel* m_title = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    bool m_directoryEditable = true;
};
