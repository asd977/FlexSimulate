#include "JsonPageBuilder.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFont appFont(QStringLiteral("Microsoft YaHei"));
    appFont.setStyleHint(QFont::SansSerif, QFont::PreferQuality);
    a.setFont(appFont);

    MainWindow w;
    w.show();
    return a.exec();
}
