#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "JsonPageBuilder.h"
#include "SchemeGalleryWidget.h"
#include "SchemeSettingsDialog.h"
#include "SchemeTreeWidget.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QStandardPaths>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSTLReader.h>

namespace
{
QString canonicalPathForDir(const QDir& dir)
{
    QString canonical = dir.canonicalPath();
    if (canonical.isEmpty())
        canonical = dir.absolutePath();
    return QDir::cleanPath(canonical);
}

bool ensureDirectoryExists(const QString& path)
{
    QDir dir(path);
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

bool copyDirectoryRecursively(const QString& sourcePath, const QString& targetPath)
{
    QDir source(sourcePath);
    if (!source.exists())
        return false;

    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral(".")))
        return false;

    const QFileInfoList entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries)
    {
        const QString targetFilePath = target.filePath(entry.fileName());
        if (entry.isDir())
        {
            if (!copyDirectoryRecursively(entry.absoluteFilePath(), targetFilePath))
                return false;
        }
        else
        {
            QFile::remove(targetFilePath);
            if (!QFile::copy(entry.absoluteFilePath(), targetFilePath))
                return false;
        }
    }
    return true;
}

bool moveDirectoryTo(const QString& sourcePath, const QString& targetPath)
{
    if (sourcePath == targetPath)
        return true;

    QDir targetParent = QFileInfo(targetPath).dir();
    if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral(".")))
        return false;

    QDir dir;
    if (dir.rename(sourcePath, targetPath))
        return true;

    if (!copyDirectoryRecursively(sourcePath, targetPath))
        return false;

    QDir source(sourcePath);
    return source.removeRecursively();
}

QString uniqueChildPath(const QDir& parent, const QString& baseName)
{
    QString sanitized = baseName.trimmed();
    if (sanitized.isEmpty())
        sanitized = QStringLiteral("Model");

    QString candidateName = sanitized;
    QString candidatePath = parent.filePath(candidateName);
    int index = 1;
    while (QDir(candidatePath).exists())
    {
        candidateName = QStringLiteral("%1_%2").arg(sanitized).arg(index++);
        candidatePath = parent.filePath(candidateName);
    }
    return candidatePath;
}

QString latestStlFile(const QString& directory)
{
    QDir dir(directory);
    const QFileInfoList files = dir.entryInfoList(QStringList() << "*.stl" << "*.STL",
                                                  QDir::Files, QDir::Time | QDir::IgnoreCase);
    if (!files.isEmpty())
        return files.first().absoluteFilePath();
    return QString();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(dataRoot);
    if (!dataDir.exists())
        dataDir.mkpath(QStringLiteral("."));
    m_workspaceRoot = dataDir.filePath(QStringLiteral("workspaces"));
    ensureDirectoryExists(m_workspaceRoot);
    m_storageFilePath = dataDir.filePath(QStringLiteral("schemes.json"));

    setupUiHelpers();
    setupConnections();
    loadInitialSchemes();
}

MainWindow::~MainWindow()
{
    saveSchemesToStorage();
    delete ui;
}

void MainWindow::setupUiHelpers()
{
    m_galleryWidget = new SchemeGalleryWidget(this);
    ui->planPageLayout->addWidget(m_galleryWidget);

    auto* detailLayout = new QVBoxLayout(ui->settingWidget);
    detailLayout->setContentsMargins(12, 12, 12, 12);
    detailLayout->setSpacing(12);

    ui->treeModels->header()->setStretchLastSection(true);
    ui->treeModels->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeModels->setEditTriggers(QAbstractItemView::EditKeyPressed |
                                    QAbstractItemView::SelectedClicked);

    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    ui->contentSplitter->setStretchFactor(0, 0);
    ui->contentSplitter->setStretchFactor(1, 1);

    const QString buttonStyle =
        "QPushButton{padding:6px 12px;border:1px solid #d0d5dd;"
        "border-radius:6px;background:#f5f7fb;}"
        "QPushButton:hover{background:#e8efff;}";
    ui->showPlanPushButton->setStyleSheet(buttonStyle);
    ui->addSchemeButton->setStyleSheet(buttonStyle);
    ui->addModelButton->setStyleSheet(buttonStyle);
    ui->openWorkspaceButton->setStyleSheet(buttonStyle);

    ui->logTextEdit->setStyleSheet(
        "QPlainTextEdit{background:#0f172a;color:#f8fafc;border-radius:6px;padding:6px;}"
    );

    auto colors = vtkSmartPointer<vtkNamedColors>::New();
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(colors->GetColor3d("AliceBlue").GetData());
    m_renderWindow->AddRenderer(m_renderer);
    ui->vtkWidget->setRenderWindow(m_renderWindow);
}

void MainWindow::setupConnections()
{
    connect(ui->treeModels, &QTreeWidget::currentItemChanged,
            this, &MainWindow::handleTreeSelectionChanged);
    connect(ui->treeModels, &QTreeWidget::itemChanged,
            this, &MainWindow::onTreeItemChanged);
    connect(ui->treeModels, &QWidget::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenuRequested);

    if (auto* schemeTree = qobject_cast<SchemeTreeWidget*>(ui->treeModels))
    {
        connect(schemeTree, &SchemeTreeWidget::itemsReordered,
                this, &MainWindow::onTreeItemsReordered);
        connect(schemeTree, &SchemeTreeWidget::externalPathsDropped,
                this, &MainWindow::onExternalDrop);
    }

    connect(ui->showPlanPushButton, &QPushButton::clicked,
            this, &MainWindow::on_showPlanPushButton_clicked);
    connect(ui->addSchemeButton, &QPushButton::clicked,
            this, &MainWindow::promptAddScheme);
    connect(ui->addModelButton, &QPushButton::clicked, this, [this]() {
        if (!m_activeSchemeId.isEmpty())
            promptAddModel(m_activeSchemeId);
    });
    connect(ui->openWorkspaceButton, &QPushButton::clicked, this, [this]() {
        if (!m_activeModelId.isEmpty())
        {
            SchemeRecord* owner = nullptr;
            if (ModelRecord* model = modelById(m_activeModelId, &owner))
                QDesktopServices::openUrl(QUrl::fromLocalFile(model->directory));
            return;
        }
        if (!m_activeSchemeId.isEmpty())
        {
            if (SchemeRecord* scheme = schemeById(m_activeSchemeId))
                QDesktopServices::openUrl(QUrl::fromLocalFile(scheme->workingDirectory));
        }
    });

    connect(m_galleryWidget, &SchemeGalleryWidget::addSchemeRequested,
            this, &MainWindow::onGalleryAddRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::schemeOpenRequested,
            this, &MainWindow::onGalleryOpenRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::schemeDeleteRequested,
            this, &MainWindow::onGalleryDeleteRequested);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, ui->treeModels);
    connect(deleteShortcut, &QShortcut::activated,
            this, &MainWindow::deleteCurrentTreeItem);
}

void MainWindow::loadInitialSchemes()
{
    if (loadSchemesFromStorage())
    {
        refreshNavigation();
        ui->stackedWidget->setCurrentWidget(ui->planPage);
        updateToolbarState();
        return;
    }

    QVector<SchemeRecord> imported;
    const QStringList candidateRoots = {
        QDir::current().absoluteFilePath(QStringLiteral("sample_data")),
        QCoreApplication::applicationDirPath() + QStringLiteral("/sample_data"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../sample_data")
    };

    for (const QString& rootPath : candidateRoots)
    {
        QDir rootDir(rootPath);
        if (!rootDir.exists())
            continue;

        const QStringList schemeDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& name : schemeDirs)
        {
            const QString sourcePath = rootDir.absoluteFilePath(name);
            const QString workspacePath = makeUniqueWorkspaceSubdir(name);
            if (!copyDirectoryRecursively(sourcePath, workspacePath))
                continue;

            SchemeRecord scheme;
            scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            scheme.name = name;
            scheme.workingDirectory = canonicalPathForDir(QDir(workspacePath));
            scheme.models = scanSchemeFolder(workspacePath);
            imported.push_back(scheme);
        }

        if (!imported.isEmpty())
            break;
    }

    m_schemes = imported;
    persistSchemes();
    refreshNavigation();
    ui->stackedWidget->setCurrentWidget(ui->planPage);
    updateToolbarState();
}

void MainWindow::on_showPlanPushButton_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->planPage);
    updateGallery();
}

void MainWindow::handleTreeSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem*)
{
    if (!current)
    {
        m_activeSchemeId.clear();
        m_activeModelId.clear();
        clearDetailWidget();
        clearVtkScene();
        updateToolbarState();
        return;
    }

    const int type = current->data(0, TypeRole).toInt();

    if (type == SchemeItem)
    {
        const QString schemeId = current->data(0, IdRole).toString();
        m_activeSchemeId = schemeId;
        m_activeModelId.clear();
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        showSchemeSettings(schemeId);
        clearVtkScene();
    }
    else if (type == ModelItem)
    {
        const QString modelId = current->data(0, IdRole).toString();
        const QString schemeId = current->data(0, SchemeRole).toString();
        m_activeSchemeId = schemeId;
        m_activeModelId = modelId;
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        showModelSettings(modelId);
    }
    updateToolbarState();
}

void MainWindow::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (!item || column != 0)
        return;
    if (m_blockTreeSignals)
        return;

    const int type = item->data(0, TypeRole).toInt();
    const QString id = item->data(0, IdRole).toString();

    if (type == SchemeItem)
    {
        if (SchemeRecord* scheme = schemeById(id))
        {
            scheme->name = item->text(0);
            persistSchemes();
            updateGallery();
            refreshCurrentDetail();
        }
    }
    else if (type == ModelItem)
    {
        SchemeRecord* owner = nullptr;
        if (ModelRecord* model = modelById(id, &owner))
        {
            model->name = item->text(0);
            persistSchemes();
            refreshCurrentDetail();
        }
    }
}

void MainWindow::onTreeContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->treeModels->itemAt(pos);
    QMenu menu(this);

    if (!item)
    {
        menu.addAction(tr("导入方案"), this, &MainWindow::promptAddScheme);
    }
    else
    {
        const int type = item->data(0, TypeRole).toInt();
        if (type == SchemeItem)
        {
            const QString schemeId = item->data(0, IdRole).toString();
            menu.addAction(tr("方案设置"), this, [this, schemeId]() {
                openSchemeSettings(schemeId);
            });
            menu.addAction(tr("添加模型"), this, [this, schemeId]() {
                promptAddModel(schemeId);
            });
            menu.addAction(tr("打开方案目录"), this, [this, schemeId]() {
                if (SchemeRecord* scheme = schemeById(schemeId))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(scheme->workingDirectory));
            });
            menu.addSeparator();
            menu.addAction(tr("删除方案"), this, [this, schemeId]() {
                removeSchemeById(schemeId);
            });
        }
        else if (type == ModelItem)
        {
            const QString modelId = item->data(0, IdRole).toString();
            menu.addAction(tr("打开模型目录"), this, [this, modelId]() {
                SchemeRecord* owner = nullptr;
                if (ModelRecord* model = modelById(modelId, &owner))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(model->directory));
            });
            menu.addSeparator();
            menu.addAction(tr("删除模型"), this, [this, modelId]() {
                removeModelById(modelId);
            });
        }
    }

    if (!menu.isEmpty())
        menu.exec(ui->treeModels->viewport()->mapToGlobal(pos));
}

void MainWindow::onTreeItemsReordered()
{
    syncDataFromTree();
}

void MainWindow::onExternalDrop(const QList<QUrl>& urls, QTreeWidgetItem* target)
{
    QStringList localPaths;
    for (const QUrl& url : urls)
    {
        if (url.isLocalFile())
            localPaths << QDir::cleanPath(url.toLocalFile());
    }
    if (localPaths.isEmpty())
        return;

    QString targetSchemeId;
    if (target)
    {
        const int type = target->data(0, TypeRole).toInt();
        if (type == SchemeItem)
            targetSchemeId = target->data(0, IdRole).toString();
        else if (type == ModelItem)
            targetSchemeId = target->data(0, SchemeRole).toString();
    }

    if (targetSchemeId.isEmpty())
    {
        QString firstId;
        for (const QString& path : localPaths)
        {
            const QString added = importSchemeFromDirectory(path);
            if (!added.isEmpty() && firstId.isEmpty())
                firstId = added;
        }
        if (!firstId.isEmpty())
        {
            ui->stackedWidget->setCurrentWidget(ui->MainPage);
            selectTreeItem(firstId, QString());
        }
        return;
    }

    QVector<QString> addedModels = importModelsIntoScheme(targetSchemeId, localPaths);
    if (!addedModels.isEmpty())
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        selectTreeItem(targetSchemeId, addedModels.first());
    }
}

void MainWindow::onGalleryAddRequested()
{
    promptAddScheme();
}

void MainWindow::onGalleryOpenRequested(const QString& id)
{
    if (schemeById(id))
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        selectTreeItem(id, QString());
    }
}

void MainWindow::onGalleryDeleteRequested(const QString& id)
{
    removeSchemeById(id);
}

void MainWindow::deleteCurrentTreeItem()
{
    QTreeWidgetItem* item = ui->treeModels->currentItem();
    if (!item)
        return;

    const int type = item->data(0, TypeRole).toInt();
    const QString id = item->data(0, IdRole).toString();

    if (type == SchemeItem)
        removeSchemeById(id);
    else if (type == ModelItem)
        removeModelById(id);
}

void MainWindow::refreshNavigation(const QString& schemeToSelect,
                                   const QString& modelToSelect)
{
    rebuildTree();
    updateGallery();

    QString schemeId = schemeToSelect;
    QString modelId = modelToSelect;
    if (schemeId.isEmpty() && !modelId.isEmpty())
    {
        const SchemeRecord* owner = nullptr;
        modelById(modelId, &owner);
        if (owner)
            schemeId = owner->id;
    }

    selectTreeItem(schemeId, modelId);
    if (!ui->treeModels->currentItem())
        clearDetailWidget();
    updateToolbarState();
}

void MainWindow::rebuildTree()
{
    QSignalBlocker blocker(ui->treeModels);
    m_blockTreeSignals = true;

    ui->treeModels->clear();
    m_schemeItems.clear();
    m_modelItems.clear();

    const QIcon schemeIcon(QStringLiteral(":/icons/plan.svg"));
    const QIcon modelIcon(QStringLiteral(":/icons/model.svg"));

    for (const SchemeRecord& scheme : m_schemes)
    {
        auto* schemeItem = new QTreeWidgetItem(ui->treeModels);
        schemeItem->setText(0, scheme.name);
        schemeItem->setIcon(0, schemeIcon);
        schemeItem->setData(0, TypeRole, SchemeItem);
        schemeItem->setData(0, IdRole, scheme.id);
        schemeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                             Qt::ItemIsDragEnabled | Qt::ItemIsEditable |
                             Qt::ItemIsDropEnabled);
        m_schemeItems.insert(scheme.id, schemeItem);

        for (const ModelRecord& model : scheme.models)
        {
            auto* modelItem = new QTreeWidgetItem(schemeItem);
            modelItem->setText(0, model.name);
            modelItem->setIcon(0, modelIcon);
            modelItem->setData(0, TypeRole, ModelItem);
            modelItem->setData(0, IdRole, model.id);
            modelItem->setData(0, SchemeRole, scheme.id);
            modelItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                                Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
            m_modelItems.insert(model.id, modelItem);
        }
    }

    ui->treeModels->expandAll();
    m_blockTreeSignals = false;
}

void MainWindow::updateGallery()
{
    if (!m_galleryWidget)
        return;

    m_galleryWidget->clearSchemes();
    for (const SchemeRecord& scheme : m_schemes)
    {
        m_galleryWidget->addScheme(scheme.id, scheme.name,
                                   makeSchemePlaceholder(scheme.name));
    }
}

void MainWindow::selectTreeItem(const QString& schemeId, const QString& modelId)
{
    if (!modelId.isEmpty())
    {
        auto it = m_modelItems.find(modelId);
        if (it != m_modelItems.end())
        {
            ui->treeModels->setCurrentItem(it.value());
            return;
        }
    }

    if (!schemeId.isEmpty())
    {
        auto it = m_schemeItems.find(schemeId);
        if (it != m_schemeItems.end())
        {
            ui->treeModels->setCurrentItem(it.value());
            return;
        }
    }

    if (ui->treeModels->topLevelItemCount() > 0)
        ui->treeModels->setCurrentItem(ui->treeModels->topLevelItem(0));
}

void MainWindow::clearDetailWidget()
{
    if (!m_currentDetailWidget)
        return;

    if (auto* layout = ui->settingWidget->layout())
        layout->removeWidget(m_currentDetailWidget);

    m_currentDetailWidget->deleteLater();
    m_currentDetailWidget = nullptr;
}

void MainWindow::showSchemeSettings(const QString& schemeId)
{
    const SchemeRecord* scheme = schemeById(schemeId);
    if (!scheme)
    {
        clearDetailWidget();
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildSchemeSettingsWidget(*scheme);
    ui->settingWidget->layout()->addWidget(m_currentDetailWidget);
}

void MainWindow::showModelSettings(const QString& modelId)
{
    SchemeRecord* owner = nullptr;
    const ModelRecord* model = modelById(modelId, &owner);
    if (!model)
    {
        clearDetailWidget();
        clearVtkScene();
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildModelSettingsWidget(*model);
    ui->settingWidget->layout()->addWidget(m_currentDetailWidget);

    const QString stl = latestStlFile(model->directory);
    if (!stl.isEmpty())
    {
        appendLogMessage(tr("加载最近的 STL：%1")
                             .arg(QDir::toNativeSeparators(stl)));
        displayStlFile(stl);
    }
    else
    {
        clearVtkScene();
    }
}

QWidget* MainWindow::buildSchemeSettingsWidget(const SchemeRecord& scheme)
{
    auto* container = new QWidget(ui->settingWidget);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("方案：%1").arg(scheme.name), container);
    title->setStyleSheet("font-size:18px;font-weight:600;");
    layout->addWidget(title);

    auto* pathLabel = new QLabel(tr("工作目录：%1")
                                     .arg(QDir::toNativeSeparators(scheme.workingDirectory)),
                                 container);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(pathLabel);

    auto* hint = new QLabel(tr("将模型文件夹拖到左侧树中，或使用下方按钮导入模型。"), container);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* list = new QListWidget(container);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (const ModelRecord& model : scheme.models)
    {
        auto* item = new QListWidgetItem(tr("%1  (%2)")
                                         .arg(model.name,
                                              QDir::toNativeSeparators(model.directory)));
        item->setToolTip(QDir::toNativeSeparators(model.jsonPath));
        list->addItem(item);
    }
    layout->addWidget(list, 1);

    auto* buttonRow = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("添加模型"), container);
    connect(addBtn, &QPushButton::clicked, this, [this, sid = scheme.id]() {
        promptAddModel(sid);
    });
    buttonRow->addWidget(addBtn);

    auto* openBtn = new QPushButton(tr("打开方案目录"), container);
    connect(openBtn, &QPushButton::clicked, this, [path = scheme.workingDirectory]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    buttonRow->addWidget(openBtn);
    buttonRow->addStretch(1);

    layout->addLayout(buttonRow);
    return container;
}

QWidget* MainWindow::buildModelSettingsWidget(const ModelRecord& model)
{
    auto* container = new QWidget(ui->settingWidget);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("模型：%1").arg(model.name), container);
    title->setStyleSheet("font-size:18px;font-weight:600;");
    layout->addWidget(title);

    auto* pathLabel = new QLabel(tr("目录：%1")
                                     .arg(QDir::toNativeSeparators(model.directory)),
                                 container);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(pathLabel);

    auto* jsonLabel = new QLabel(tr("配置文件：%1")
                                     .arg(QDir::toNativeSeparators(model.jsonPath)),
                                 container);
    jsonLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(jsonLabel);

    auto* builder = new JsonPageBuilder(model.jsonPath, container);
    layout->addWidget(builder, 1);

    connect(builder, &JsonPageBuilder::logMessage,
            this, &MainWindow::appendLogMessage);
    connect(builder, &JsonPageBuilder::calculationFinished,
            this, [this](const QString& stlPath) {
        if (stlPath.isEmpty())
        {
            appendLogMessage(tr("未检测到新的 STL 输出文件"));
            return;
        }
        appendLogMessage(tr("加载 STL：%1").arg(QDir::toNativeSeparators(stlPath)));
        displayStlFile(stlPath);
    });

    auto* openBtn = new QPushButton(tr("打开模型目录"), container);
    connect(openBtn, &QPushButton::clicked, this, [path = model.directory]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    layout->addWidget(openBtn, 0, Qt::AlignLeft);

    return container;
}

void MainWindow::refreshCurrentDetail()
{
    if (!m_activeModelId.isEmpty())
        showModelSettings(m_activeModelId);
    else if (!m_activeSchemeId.isEmpty())
        showSchemeSettings(m_activeSchemeId);
    else
        clearDetailWidget();
}

MainWindow::SchemeRecord* MainWindow::schemeById(const QString& id)
{
    for (SchemeRecord& scheme : m_schemes)
    {
        if (scheme.id == id)
            return &scheme;
    }
    return nullptr;
}

const MainWindow::SchemeRecord* MainWindow::schemeById(const QString& id) const
{
    for (const SchemeRecord& scheme : m_schemes)
    {
        if (scheme.id == id)
            return &scheme;
    }
    return nullptr;
}

MainWindow::SchemeRecord* MainWindow::schemeByWorkingDirectory(const QString& canonicalPath)
{
    for (SchemeRecord& scheme : m_schemes)
    {
        if (canonicalPathForDir(QDir(scheme.workingDirectory)) == canonicalPath)
            return &scheme;
    }
    return nullptr;
}

MainWindow::ModelRecord* MainWindow::modelById(const QString& id, SchemeRecord** owner)
{
    for (SchemeRecord& scheme : m_schemes)
    {
        for (ModelRecord& model : scheme.models)
        {
            if (model.id == id)
            {
                if (owner)
                    *owner = &scheme;
                return &model;
            }
        }
    }
    if (owner)
        *owner = nullptr;
    return nullptr;
}

const MainWindow::ModelRecord* MainWindow::modelById(const QString& id, const SchemeRecord** owner) const
{
    for (const SchemeRecord& scheme : m_schemes)
    {
        for (const ModelRecord& model : scheme.models)
        {
            if (model.id == id)
            {
                if (owner)
                    *owner = &scheme;
                return &model;
            }
        }
    }
    if (owner)
        *owner = nullptr;
    return nullptr;
}

QString MainWindow::createScheme(const QString& name, const QString& workingDir)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
        return QString();

    const QString canonical = canonicalPathForDir(QDir(workingDir));
    if (canonical.isEmpty())
        return QString();

    if (SchemeRecord* existing = schemeByWorkingDirectory(canonical))
        return existing->id;

    SchemeRecord scheme;
    scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    scheme.name = trimmedName;
    scheme.workingDirectory = canonical;
    m_schemes.push_back(scheme);
    return scheme.id;
}

QString MainWindow::importSchemeFromDirectory(const QString& dirPath, bool showError)
{
    QDir dir(dirPath);
    if (!dir.exists())
    {
        if (showError)
            QMessageBox::warning(this, tr("导入失败"),
                                 tr("路径不存在：%1").arg(QDir::toNativeSeparators(dirPath)));
        return QString();
    }

    const QString canonical = canonicalPathForDir(dir);

    if (SchemeRecord* existing = schemeByWorkingDirectory(canonical))
    {
        existing->name = dir.dirName();
        existing->models = scanSchemeFolder(canonical);
        persistSchemes();
        refreshNavigation(existing->id);
        return existing->id;
    }

    SchemeRecord scheme;
    scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    scheme.name = dir.dirName();
    scheme.workingDirectory = canonical;
    scheme.models = scanSchemeFolder(canonical);

    m_schemes.push_back(scheme);
    persistSchemes();
    refreshNavigation(scheme.id);
    return scheme.id;
}

QVector<QString> MainWindow::importModelsIntoScheme(const QString& schemeId,
                                                    const QStringList& paths,
                                                    bool showError)
{
    QVector<QString> addedIds;
    SchemeRecord* scheme = schemeById(schemeId);
    if (!scheme)
        return addedIds;

    if (!ensureDirectoryExists(scheme->workingDirectory))
    {
        if (showError)
            QMessageBox::warning(this, tr("导入失败"),
                                 tr("无法创建方案工作目录：%1")
                                     .arg(QDir::toNativeSeparators(scheme->workingDirectory)));
        return addedIds;
    }

    QDir workingDir(scheme->workingDirectory);
    QSet<QString> existingPaths;
    for (const ModelRecord& model : scheme->models)
        existingPaths.insert(canonicalPathForDir(QDir(model.directory)));

    for (const QString& path : paths)
    {
        QDir src(path);
        if (!src.exists())
        {
            if (showError)
                QMessageBox::warning(this, tr("导入失败"),
                                     tr("路径不存在：%1").arg(QDir::toNativeSeparators(path)));
            continue;
        }

        QString jsonPath, batPath;
        if (isModelFolder(src, &jsonPath, &batPath))
        {
            QString destPath = uniqueChildPath(workingDir, src.dirName());
            if (!moveDirectoryTo(src.absolutePath(), destPath))
            {
                if (showError)
                    QMessageBox::warning(this, tr("导入失败"),
                                         tr("无法移动模型文件夹：%1")
                                             .arg(QDir::toNativeSeparators(path)));
                continue;
            }

            QDir destDir(destPath);
            ModelRecord model;
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            model.name = destDir.dirName();
            model.directory = canonicalPathForDir(destDir);
            const QString jsonName = QFileInfo(jsonPath).fileName();
            model.jsonPath = destDir.filePath(jsonName);
            const QString batName = QFileInfo(batPath).fileName();
            model.batPath = batName.isEmpty() ? QString() : destDir.filePath(batName);
            if (existingPaths.contains(model.directory))
                continue;

            scheme->models.push_back(model);
            addedIds.push_back(model.id);
            existingPaths.insert(model.directory);
            continue;
        }

        QVector<ModelRecord> nested = scanSchemeFolder(canonicalPathForDir(src));
        if (nested.isEmpty())
        {
            if (showError)
                QMessageBox::warning(this, tr("导入失败"),
                                     tr("%1 不是有效的模型文件夹。")
                                         .arg(QDir::toNativeSeparators(path)));
            continue;
        }

        for (ModelRecord model : nested)
        {
            QDir destDir(uniqueChildPath(workingDir, QFileInfo(model.directory).fileName()));
            if (!moveDirectoryTo(model.directory, destDir.absolutePath()))
            {
                if (showError)
                    QMessageBox::warning(this, tr("导入失败"),
                                         tr("无法移动模型文件夹：%1")
                                             .arg(QDir::toNativeSeparators(model.directory)));
                continue;
            }

            model.directory = canonicalPathForDir(destDir);
            const QString jsonName = QFileInfo(model.jsonPath).fileName();
            model.jsonPath = destDir.filePath(jsonName);
            const QString batName = QFileInfo(model.batPath).fileName();
            model.batPath = batName.isEmpty() ? QString() : destDir.filePath(batName);
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

            if (existingPaths.contains(model.directory))
                continue;

            scheme->models.push_back(model);
            addedIds.push_back(model.id);
            existingPaths.insert(model.directory);
        }
    }

    if (!addedIds.isEmpty())
    {
        persistSchemes();
        refreshNavigation(schemeId, addedIds.first());
        appendLogMessage(tr("成功导入 %1 个模型").arg(addedIds.size()));
    }
    else
    {
        refreshNavigation(schemeId, m_activeModelId);
    }

    return addedIds;
}

bool MainWindow::isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const
{
    QDir copy(dir);
    const QStringList jsons = copy.entryList(QStringList() << "*.json",
                                             QDir::Files | QDir::NoDotAndDotDot);
    const QStringList bats = copy.entryList(QStringList() << "*.bat",
                                            QDir::Files | QDir::NoDotAndDotDot);
    if (jsons.isEmpty())
        return false;

    if (jsonPath)
        *jsonPath = copy.absoluteFilePath(jsons.first());
    if (batPath && !bats.isEmpty())
        *batPath = copy.absoluteFilePath(bats.first());
    return true;
}

QVector<MainWindow::ModelRecord> MainWindow::scanSchemeFolder(const QString& schemeDir) const
{
    QVector<ModelRecord> models;
    QDir dir(schemeDir);
    const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : subDirs)
    {
        QDir child(dir.absoluteFilePath(name));
        QString jsonPath, batPath;
        if (!isModelFolder(child, &jsonPath, &batPath))
            continue;

        ModelRecord model;
        model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        model.name = name;
        model.directory = canonicalPathForDir(child);
        model.jsonPath = jsonPath;
        model.batPath = batPath;
        models.push_back(model);
    }
    return models;
}

QPixmap MainWindow::makeSchemePlaceholder(const QString& name) const
{
    const QSize sz(480, 280);
    QPixmap pm(sz);
    pm.fill(Qt::white);
    QPainter painter(&pm);
    painter.fillRect(pm.rect(), QColor("#eef2f7"));
    painter.setPen(QColor("#51606f"));
    QFont font = painter.font();
    font.setPointSize(18);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pm.rect(), Qt::AlignCenter, name);
    return pm;
}

void MainWindow::promptAddScheme()
{
    const QString defaultName = tr("新方案%1").arg(m_schemes.size() + 1);
    const QString defaultDir = makeUniqueWorkspaceSubdir(defaultName);

    SchemeSettingsDialog dlg(defaultName, defaultDir, true, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = dlg.schemeName();
    const QString directory = dlg.workingDirectory();
    if (name.isEmpty() || directory.isEmpty())
    {
        QMessageBox::warning(this, tr("创建方案"), tr("方案名称和工作目录不能为空"));
        return;
    }

    if (!ensureDirectoryExists(directory))
    {
        QMessageBox::warning(this, tr("创建方案"),
                             tr("无法创建工作目录：%1")
                                 .arg(QDir::toNativeSeparators(directory)));
        return;
    }

    const QString id = importSchemeFromDirectory(directory, false);
    if (!id.isEmpty())
    {
        if (SchemeRecord* scheme = schemeById(id))
            scheme->name = name;
        persistSchemes();
        refreshNavigation(id);
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        appendLogMessage(tr("已创建方案 %1").arg(name));
    }
}

void MainWindow::promptAddModel(const QString& schemeId)
{
    if (!schemeById(schemeId))
        return;

    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择模型目录"));
    if (dir.isEmpty())
        return;

    const QVector<QString> added = importModelsIntoScheme(schemeId, {dir});
    if (!added.isEmpty())
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        selectTreeItem(schemeId, added.first());
    }
}

void MainWindow::openSchemeSettings(const QString& schemeId)
{
    SchemeRecord* scheme = schemeById(schemeId);
    if (!scheme)
        return;

    SchemeSettingsDialog dlg(scheme->name, scheme->workingDirectory, false, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    scheme->name = dlg.schemeName();
    persistSchemes();
    refreshNavigation(schemeId, m_activeModelId);
}

void MainWindow::removeSchemeById(const QString& id)
{
    for (int i = 0; i < m_schemes.size(); ++i)
    {
        if (m_schemes[i].id == id)
        {
            m_schemes.removeAt(i);
            if (m_activeSchemeId == id)
            {
                m_activeSchemeId.clear();
                m_activeModelId.clear();
            }
            persistSchemes();
            refreshNavigation();
            appendLogMessage(tr("已删除方案"));
            return;
        }
    }
}

void MainWindow::removeModelById(const QString& id)
{
    for (int i = 0; i < m_schemes.size(); ++i)
    {
        SchemeRecord& scheme = m_schemes[i];
        for (int j = 0; j < scheme.models.size(); ++j)
        {
            if (scheme.models[j].id == id)
            {
                scheme.models.removeAt(j);
                if (m_activeModelId == id)
                    m_activeModelId.clear();
                persistSchemes();
                refreshNavigation(scheme.id);
                appendLogMessage(tr("已删除模型"));
                return;
            }
        }
    }
}

void MainWindow::syncDataFromTree()
{
    const QVector<SchemeRecord> previous = m_schemes;

    QHash<QString, SchemeRecord> schemeMap;
    for (const SchemeRecord& scheme : previous)
        schemeMap.insert(scheme.id, scheme);

    QHash<QString, ModelRecord> modelMap;
    for (const SchemeRecord& scheme : previous)
        for (const ModelRecord& model : scheme.models)
            modelMap.insert(model.id, model);

    QVector<SchemeRecord> updated;
    const int topCount = ui->treeModels->topLevelItemCount();
    for (int i = 0; i < topCount; ++i)
    {
        QTreeWidgetItem* schemeItem = ui->treeModels->topLevelItem(i);
        const QString schemeId = schemeItem->data(0, IdRole).toString();
        if (schemeId.isEmpty())
            continue;

        SchemeRecord scheme = schemeMap.value(schemeId);
        scheme.name = schemeItem->text(0);
        scheme.models.clear();

        const int childCount = schemeItem->childCount();
        for (int j = 0; j < childCount; ++j)
        {
            QTreeWidgetItem* modelItem = schemeItem->child(j);
            const QString modelId = modelItem->data(0, IdRole).toString();
            if (modelId.isEmpty())
                continue;

            ModelRecord model = modelMap.value(modelId);
            if (model.id.isEmpty())
                continue;
            model.name = modelItem->text(0);
            scheme.models.push_back(model);
        }

        updated.push_back(scheme);
    }

    m_schemes = updated;
    persistSchemes();
    refreshNavigation(m_activeSchemeId, m_activeModelId);
}

void MainWindow::updateToolbarState()
{
    const bool hasScheme = !m_activeSchemeId.isEmpty();
    const bool hasModel = !m_activeModelId.isEmpty();
    const bool anyScheme = !m_schemes.isEmpty();

    ui->addModelButton->setEnabled(hasScheme);
    ui->openWorkspaceButton->setEnabled(hasScheme || hasModel);
    ui->showPlanPushButton->setEnabled(anyScheme);
}

void MainWindow::appendLogMessage(const QString& message)
{
    if (!ui->logTextEdit)
        return;

    const QString timeStamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->appendPlainText(QStringLiteral("[%1] %2").arg(timeStamp, message));
    if (auto* bar = ui->logTextEdit->verticalScrollBar())
        bar->setValue(bar->maximum());
}

void MainWindow::displayStlFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return;

    QFileInfo info(filePath);
    if (!info.exists())
    {
        appendLogMessage(tr("未找到 STL 文件：%1")
                             .arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    auto reader = vtkSmartPointer<vtkSTLReader>::New();
    reader->SetFileName(qPrintable(info.absoluteFilePath()));
    reader->Update();

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(reader->GetOutputPort());

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.2, 0.45, 0.75);
    actor->GetProperty()->SetDiffuse(0.8);
    actor->GetProperty()->SetSpecular(0.3);

    if (!m_renderer)
        return;

    m_renderer->RemoveAllViewProps();
    m_currentActor = actor;
    m_renderer->AddActor(actor);
    m_renderer->ResetCamera();
    if (ui->vtkWidget && ui->vtkWidget->renderWindow())
        ui->vtkWidget->renderWindow()->Render();
}

void MainWindow::clearVtkScene()
{
    if (!m_renderer)
        return;
    m_renderer->RemoveAllViewProps();
    if (ui->vtkWidget && ui->vtkWidget->renderWindow())
        ui->vtkWidget->renderWindow()->Render();
    m_currentActor = nullptr;
}

bool MainWindow::loadSchemesFromStorage()
{
    QFile file(m_storageFilePath);
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QString storedRoot = root.value(QStringLiteral("workspaceRoot")).toString();
    if (!storedRoot.isEmpty())
        m_workspaceRoot = storedRoot;
    ensureDirectoryExists(m_workspaceRoot);

    QVector<SchemeRecord> loaded;
    const QJsonArray schemeArray = root.value(QStringLiteral("schemes")).toArray();
    for (const QJsonValue& value : schemeArray)
    {
        const QJsonObject obj = value.toObject();
        SchemeRecord scheme;
        scheme.id = obj.value(QStringLiteral("id")).toString();
        if (scheme.id.isEmpty())
            scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        scheme.name = obj.value(QStringLiteral("name")).toString();
        scheme.workingDirectory = canonicalPathForDir(QDir(obj.value(QStringLiteral("workingDirectory")).toString()));
        if (scheme.workingDirectory.isEmpty())
            continue;

        const QJsonArray modelArray = obj.value(QStringLiteral("models")).toArray();
        for (const QJsonValue& mv : modelArray)
        {
            const QJsonObject mo = mv.toObject();
            ModelRecord model;
            model.id = mo.value(QStringLiteral("id")).toString();
            if (model.id.isEmpty())
                model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            model.name = mo.value(QStringLiteral("name")).toString();
            model.directory = canonicalPathForDir(QDir(mo.value(QStringLiteral("directory")).toString()));
            model.jsonPath = QDir::cleanPath(mo.value(QStringLiteral("jsonPath")).toString());
            model.batPath = QDir::cleanPath(mo.value(QStringLiteral("batPath")).toString());
            if (model.directory.isEmpty() || model.jsonPath.isEmpty())
                continue;
            scheme.models.push_back(model);
        }
        loaded.push_back(scheme);
    }

    m_schemes = loaded;
    return true;
}

void MainWindow::saveSchemesToStorage() const
{
    if (m_storageFilePath.isEmpty())
        return;

    QFileInfo info(m_storageFilePath);
    QDir dir = info.dir();
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QJsonArray schemeArray;
    for (const SchemeRecord& scheme : m_schemes)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), scheme.id);
        obj.insert(QStringLiteral("name"), scheme.name);
        obj.insert(QStringLiteral("workingDirectory"), scheme.workingDirectory);

        QJsonArray modelArray;
        for (const ModelRecord& model : scheme.models)
        {
            QJsonObject mo;
            mo.insert(QStringLiteral("id"), model.id);
            mo.insert(QStringLiteral("name"), model.name);
            mo.insert(QStringLiteral("directory"), model.directory);
            mo.insert(QStringLiteral("jsonPath"), model.jsonPath);
            mo.insert(QStringLiteral("batPath"), model.batPath);
            modelArray.append(mo);
        }
        obj.insert(QStringLiteral("models"), modelArray);
        schemeArray.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("workspaceRoot"), m_workspaceRoot);
    root.insert(QStringLiteral("schemes"), schemeArray);

    QFile file(m_storageFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::persistSchemes() const
{
    saveSchemesToStorage();
}

QString MainWindow::makeUniqueWorkspaceSubdir(const QString& baseName) const
{
    QDir base(workspaceRoot());
    if (!base.exists())
        ensureDirectoryExists(base.absolutePath());
    QString sanitized = baseName;
    sanitized.replace(QRegularExpression("\\s+"), "_");
    if (sanitized.isEmpty())
        sanitized = QStringLiteral("Workspace");

    QString candidate = base.filePath(sanitized);
    int index = 1;
    while (QDir(candidate).exists())
    {
        candidate = base.filePath(QStringLiteral("%1_%2").arg(sanitized).arg(index++));
    }
    return candidate;
}

QString MainWindow::workspaceRoot() const
{
    return m_workspaceRoot;
}
