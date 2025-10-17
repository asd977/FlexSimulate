#include "pages/WelcomePage.h"

#include "ui_MainWindow.h"

#include <QLabel>
#include <QVBoxLayout>

WelcomePage::WelcomePage(Ui::MainWindow* ui, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
{
}

void WelcomePage::initialize()
{
    if (!m_ui || !m_ui->welcomeLabel)
        return;

    m_ui->welcomeLabel->setStyleSheet(
        "font-size:24px;font-weight:600;color:#0f172a;margin-bottom:12px;");

    if (auto* layout = m_ui->welcomeLayout)
    {
        layout->setContentsMargins(48, 48, 48, 48);
        layout->setSpacing(24);
    }
}

void WelcomePage::show()
{
    if (m_ui && m_ui->stackedWidget && m_ui->welcomePage)
        m_ui->stackedWidget->setCurrentWidget(m_ui->welcomePage);
}

