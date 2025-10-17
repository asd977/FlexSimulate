#pragma once

#include <QObject>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class WelcomePage : public QObject
{
    Q_OBJECT
public:
    explicit WelcomePage(Ui::MainWindow* ui, QObject* parent = nullptr);

    void initialize();
    void show();

private:
    Ui::MainWindow* m_ui = nullptr;
};

