#pragma once

#include <QObject>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class WelcomePage : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造绑定指定 UI 实例的欢迎页辅助对象。
     */
    explicit WelcomePage(Ui::MainWindow* ui, QObject* parent = nullptr);

    /**
     * @brief 为欢迎页组件应用样式并配置初始内容。
     */
    void initialize();

    /**
     * @brief 在主堆叠组件中显示欢迎页内容。
     */
    void show();

private:
    Ui::MainWindow* m_ui = nullptr;
};

