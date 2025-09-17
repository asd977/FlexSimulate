#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QTreeWidget>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDebug>

#include "JsonPageBuilder.h"
#include "SchemeGalleryWidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    SchemeGalleryWidget *widget = new SchemeGalleryWidget();
    widget->show();
}

MainWindow::~MainWindow()
{

}

void MainWindow::on_showPlanPushButton_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}
