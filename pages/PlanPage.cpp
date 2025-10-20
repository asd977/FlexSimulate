#include "pages/PlanPage.h"

#include "SchemeGalleryWidget.h"
#include "ui_MainWindow.h"

#include <QDateTime>
#include <QDir>
#include <QLabel>
#include <QLayout>
#include <QBoxLayout>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QStringList>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <algorithm>
#include <vtkActor.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNamedColors.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSTLReader.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

PlanPage::PlanPage(Ui::MainWindow* ui, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
{
}

void PlanPage::initialize()
{
    setupUiStyles();
    setupSplitterStyles();
    initializeVisualization();
}

void PlanPage::setupUiStyles()
{
    if (!m_ui)
        return;

    if (auto* central = m_ui->centralwidget)
    {
        central->setAttribute(Qt::WA_StyledBackground, true);
        central->setStyleSheet(QStringLiteral("QWidget#centralwidget{background:#f1f5f9;}"));
    }

    if (!m_galleryWidget)
    {
        m_galleryWidget = new SchemeGalleryWidget(m_ui->planPage);
        m_ui->planPageLayout->addWidget(m_galleryWidget);
    }

    auto* detailLayout = new QVBoxLayout(m_ui->settingWidget);
    detailLayout->setContentsMargins(20, 20, 20, 20);
    detailLayout->setSpacing(16);

    if (!m_selectionInfo)
    {
        if (m_ui->selectionInfo)
        {
            m_selectionInfo = m_ui->selectionInfo;
        }
        else
        {
            m_selectionInfo = new QLabel(m_ui->detailPanel);
            m_selectionInfo->setObjectName(QStringLiteral("selectionInfo"));
            if (auto* panelLayout = qobject_cast<QBoxLayout*>(m_ui->detailPanel->layout()))
            {
                panelLayout->insertWidget(1, m_selectionInfo);
            }
            else if (auto* panelLayout = m_ui->detailPanel->layout())
            {
                panelLayout->addWidget(m_selectionInfo);
            }
        }
        if (m_selectionInfo)
        {
            m_selectionInfo->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_selectionInfo->setWordWrap(true);
        }
    }

    if (m_selectionInfo)
    {
        m_selectionInfo->setStyleSheet(QStringLiteral(
            "QLabel#selectionInfo{padding:12px 16px;font-size:13px;color:#475569;}"));
    }

    const auto applyPanelCard = [](QWidget* panel, QLabel* title, const QString& extraStyles = QString()) {
        if (!panel || !title)
            return;

        panel->setAttribute(Qt::WA_StyledBackground, true);
        const QString panelSelector = panel->objectName().isEmpty()
                                          ? QStringLiteral("%1").arg(QString::fromLatin1(panel->metaObject()->className()))
                                          : QStringLiteral("%1#%2").arg(QString::fromLatin1(panel->metaObject()->className()), panel->objectName());
        const QString titleSelector = title->objectName().isEmpty()
                                          ? QStringLiteral("QLabel")
                                          : QStringLiteral("QLabel#%1").arg(title->objectName());
        QString style = QStringLiteral(
            "%1{background:#ffffff;border:1px solid #d6e1f2;border-radius:14px;}"
            "%2{font-size:15px;font-weight:600;color:#0f172a;padding:12px 16px;"
            "background:#f8fafc;border-top-left-radius:14px;border-top-right-radius:14px;"
            "border-bottom:1px solid #e2e8f0;}"
            "%3")
                             .arg(panelSelector, titleSelector, extraStyles);
        panel->setStyleSheet(style);
    };

    applyPanelCard(m_ui->navigationFrame, m_ui->navigationTitle,
                   QStringLiteral(
                       "QTreeWidget{border:none;background:transparent;padding:8px 12px;}"
                       "QTreeWidget::item{padding:6px 4px;}"
                       "QTreeWidget::item:hover{background:#f1f5f9;}"
                       "QTreeWidget::item:selected{background:#e2e8f0;color:#0f172a;}"
                       "QHeaderView::section{background:transparent;border:none;padding:4px 0;font-weight:600;color:#334155;}"));

    applyPanelCard(m_ui->detailPanel, m_ui->detailTitle,
                   QStringLiteral(
                       "QScrollArea{border:none;background:transparent;}"
                       "QWidget#scrollAreaWidgetContents{background:transparent;}"));

    applyPanelCard(m_ui->vtkPanel, m_ui->vtkTitle,
                   QStringLiteral(
                       "QFrame#vtkFrame{border:none;background:transparent;border-bottom-left-radius:14px;border-bottom-right-radius:14px;}"
                       "QVTKOpenGLNativeWidget{border:none;border-bottom-left-radius:14px;border-bottom-right-radius:14px;}"));

    applyPanelCard(m_ui->logPanel, m_ui->logTitle);
}

void PlanPage::setupSplitterStyles()
{
    if (!m_ui)
        return;

    const QString splitterStyle = QStringLiteral(
        "QSplitter::handle{background:#cbd5f5;}"
        "QSplitter::handle:horizontal{width:8px;margin:0 4px;border-radius:4px;}"
        "QSplitter::handle:vertical{height:8px;margin:4px 0;border-radius:4px;}");

    if (m_ui->mainSplitter)
        m_ui->mainSplitter->setStyleSheet(splitterStyle);
    if (m_ui->contentSplitter)
        m_ui->contentSplitter->setStyleSheet(splitterStyle);
    if (m_ui->visualizationSplitter)
        m_ui->visualizationSplitter->setStyleSheet(splitterStyle);

    if (m_ui->treeModels)
    {
        m_ui->treeModels->header()->setStretchLastSection(true);
        m_ui->treeModels->setHeaderHidden(true);
        m_ui->treeModels->setContextMenuPolicy(Qt::CustomContextMenu);
        m_ui->treeModels->setEditTriggers(QAbstractItemView::EditKeyPressed |
                                          QAbstractItemView::SelectedClicked);
    }

    if (m_ui->mainSplitter)
    {
        m_ui->mainSplitter->setStretchFactor(0, 0);
        m_ui->mainSplitter->setStretchFactor(1, 1);
    }

    if (m_ui->contentSplitter)
    {
        m_ui->contentSplitter->setStretchFactor(0, 0);
        m_ui->contentSplitter->setStretchFactor(1, 1);
        m_ui->contentSplitter->setCollapsible(1, true);
    }

    if (m_ui->visualizationSplitter)
    {
        m_ui->visualizationSplitter->setStretchFactor(0, 3);
        m_ui->visualizationSplitter->setStretchFactor(1, 1);
        m_ui->visualizationSplitter->setHandleWidth(6);
    }

    if (m_ui->logTextEdit)
    {
        m_ui->logTextEdit->setStyleSheet(
            "QPlainTextEdit{background:#0f172a;color:#f8fafc;border:none;"
            "border-bottom-left-radius:14px;border-bottom-right-radius:14px;padding:12px;"
            "font-family:\"JetBrains Mono\", \"Source Code Pro\", monospace;}");
    }
}

void PlanPage::initializeVisualization()
{
    if (!m_ui || !m_ui->vtkWidget)
        return;

    auto colors = vtkSmartPointer<vtkNamedColors>::New();
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(colors->GetColor3d("AliceBlue").GetData());
    m_renderWindow->AddRenderer(m_renderer);
    m_ui->vtkWidget->setRenderWindow(m_renderWindow);
    setVisualizationVisible(false);
    updateSelectionInfo();
}

void PlanPage::setDetailWidget(QWidget* widget)
{
    if (!m_ui)
        return;

    clearDetailWidget();
    m_currentDetailWidget = widget;
    if (!m_currentDetailWidget)
        return;

    if (auto* layout = m_ui->settingWidget->layout())
        layout->addWidget(m_currentDetailWidget);
}

void PlanPage::clearDetailWidget()
{
    if (!m_currentDetailWidget || !m_ui)
        return;

    if (auto* layout = m_ui->settingWidget->layout())
        layout->removeWidget(m_currentDetailWidget);
    m_currentDetailWidget->deleteLater();
    m_currentDetailWidget = nullptr;
}

void PlanPage::setVisualizationVisible(bool visible)
{
    if (!m_ui || !m_ui->vtkPanel || !m_ui->logPanel || !m_ui->logTextEdit || !m_ui->contentSplitter)
        return;

    if (m_visualizationVisible == visible)
        return;

    m_visualizationVisible = visible;

    if (visible)
    {
        m_ui->vtkPanel->setVisible(true);
        m_ui->logPanel->setVisible(true);
        m_ui->logTitle->setVisible(true);
        m_ui->logTextEdit->setVisible(true);

        if (!m_lastSplitterSizes.isEmpty())
        {
            m_ui->contentSplitter->setSizes(m_lastSplitterSizes);
        }
        else
        {
            QList<int> sizes = m_ui->contentSplitter->sizes();
            if (sizes.size() < 2 || (sizes.at(0) == 0 && sizes.at(1) == 0))
            {
                sizes.clear();
                sizes << 1 << 1;
            }
            m_ui->contentSplitter->setSizes(sizes);
        }

        if (m_ui->visualizationSplitter)
        {
            QList<int> vizSizes = m_ui->visualizationSplitter->sizes();
            bool invalid = vizSizes.size() < 2;
            if (!invalid)
            {
                invalid = true;
                for (int size : vizSizes)
                {
                    if (size > 0)
                    {
                        invalid = false;
                        break;
                    }
                }
            }
            if (invalid)
            {
                vizSizes.clear();
                vizSizes << 3 << 1;
                m_ui->visualizationSplitter->setSizes(vizSizes);
            }
        }
    }
    else
    {
        m_lastSplitterSizes = m_ui->contentSplitter->sizes();

        m_ui->vtkPanel->setVisible(false);
        m_ui->logPanel->setVisible(false);
        if (m_ui->logTitle)
            m_ui->logTitle->setVisible(false);
        m_ui->logTextEdit->setVisible(false);

        QList<int> sizes = m_ui->contentSplitter->sizes();
        if (sizes.size() >= 2)
        {
            const int total = std::max(1, sizes.value(0) + sizes.value(1));
            sizes[0] = total;
            sizes[1] = 0;
            m_ui->contentSplitter->setSizes(sizes);
        }
    }
}

void PlanPage::updateSelectionInfo(const QString& path, const QString& remark)
{
    if (!m_selectionInfo)
        return;

    QStringList lines;
    if (!path.isEmpty())
        lines << tr("路径：%1").arg(QDir::toNativeSeparators(path));
    if (!remark.isEmpty())
        lines << tr("备注：%1").arg(remark);

    m_selectionInfo->setText(lines.isEmpty() ? tr("未选择对象") : lines.join('\n'));
}

void PlanPage::appendLogMessage(const QString& message)
{
    if (!m_ui || !m_ui->logTextEdit)
        return;

    const QString timeStamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    m_ui->logTextEdit->appendPlainText(QStringLiteral("[%1] %2").arg(timeStamp, message));
    if (auto* bar = m_ui->logTextEdit->verticalScrollBar())
        bar->setValue(bar->maximum());
}

void PlanPage::displayResultFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return;

    QFileInfo info(filePath);
    if (!info.exists())
    {
        appendLogMessage(tr("未找到结果文件：%1").arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    vtkSmartPointer<vtkActor> actor;
    const QString suffix = info.suffix().toLower();

    if (suffix == QStringLiteral("stl"))
    {
        auto reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(info.absoluteFilePath().toUtf8().constData());
        reader->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(reader->GetOutputPort());

        actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.2, 0.45, 0.75);
        actor->GetProperty()->SetDiffuse(0.8);
        actor->GetProperty()->SetSpecular(0.3);
    }
    else
    {
        appendLogMessage(tr("不支持的结果文件类型：%1").arg(QDir::toNativeSeparators(info.absoluteFilePath())));
        return;
    }

    if (!m_renderer)
        return;

    m_renderer->RemoveAllViewProps();
    m_currentActor = actor;
    m_renderer->AddActor(actor);
    m_renderer->ResetCamera();
    if (m_ui->vtkWidget && m_ui->vtkWidget->renderWindow())
        m_ui->vtkWidget->renderWindow()->Render();
}

void PlanPage::clearVtkScene()
{
    if (!m_renderer)
        return;

    m_renderer->RemoveAllViewProps();
    if (m_ui && m_ui->vtkWidget && m_ui->vtkWidget->renderWindow())
        m_ui->vtkWidget->renderWindow()->Render();
    m_currentActor = nullptr;
}

void PlanPage::setCurrentActor(const vtkSmartPointer<vtkActor>& actor)
{
    m_currentActor = actor;
}

void PlanPage::setLastSplitterSizes(const QList<int>& sizes)
{
    m_lastSplitterSizes = sizes;
}

