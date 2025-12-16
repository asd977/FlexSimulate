#include "JsonPageBuilder.h"
#include "mainwindow.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFont>
#include <QTimer>
#include <QDebug>

int main(int argc, char *argv[])
{
//    QElapsedTimer startTimer;
//    startTimer.start();          // 记录启动初始时间点

    QApplication a(argc, argv);

    QFont appFont(QStringLiteral("Microsoft YaHei"));
    appFont.setStyleHint(QFont::SansSerif, QFont::PreferQuality);
    a.setFont(appFont);

    MainWindow w;
    w.show();

//    // 当事件循环真正跑起来后，界面已经显示完成
//    QTimer::singleShot(0, &w, [&w, &startTimer]() {
//        qint64 ms = startTimer.elapsed();

//        qDebug() << "Startup time =" << ms << "ms";
//        QFile file("startup_time.log");
//        if (file.open(QIODevice::Append | QIODevice::Text)) {
//            QTextStream out(&file);
//            out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
//                << " : " << ms << " ms\n";
//        }
//    });
    return a.exec();
}
