#pragma once
#include <QDialog>

class QLineEdit;
class QLabel;

class SchemeSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SchemeSettingsDialog(const QString& schemeName, QWidget* parent=nullptr);

private:
    QLabel* m_title;
    QLineEdit* m_nameEdit;
};
