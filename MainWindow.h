#pragma once
#include <QMainWindow>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTreeWidgetItem;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_showPlanPushButton_clicked();

private:
    Ui::MainWindow *ui;

};
