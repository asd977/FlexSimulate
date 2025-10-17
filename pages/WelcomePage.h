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
     * @brief Constructs a welcome page helper bound to the provided UI instance.
     */
    explicit WelcomePage(Ui::MainWindow* ui, QObject* parent = nullptr);

    /**
     * @brief Applies styling and initial content configuration for the welcome page widgets.
     */
    void initialize();

    /**
     * @brief Displays the welcome content within the main stacked widget.
     */
    void show();

private:
    Ui::MainWindow* m_ui = nullptr;
};

