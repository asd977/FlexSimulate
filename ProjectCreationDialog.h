#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

class ProjectCreationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProjectCreationDialog(QWidget* parent = nullptr);

    QString projectName() const;
    QString projectDirectory() const;

    void setInitialName(const QString& name);
    void setInitialDirectory(const QString& directory);

private slots:
    void browseForDirectory();

private:
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
};

