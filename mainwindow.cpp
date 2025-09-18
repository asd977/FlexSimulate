#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "JsonPageBuilder.h"
#include "SchemeGalleryWidget.h"
#include "SchemeTreeWidget.h"

#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QShortcut>
#include <QUuid>
#include <QPainter>
#include <QApplication>
#include <QSignalBlocker>
#include <QSet>
#include <QCoreApplication>
#include <QFont>

namespace
{
    QString canonicalPathForDir(const QDir& dir)
    {
        QString canonical = dir.canonicalPath();
        if (canonical.isEmpty())
            canonical = dir.absolutePath();
        return QDir::cleanPath(canonical);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupUiHelpers();
    setupConnections();
    loadInitialSchemes();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiHelpers()
{
    // Gallery view on plan page
    m_galleryWidget = new SchemeGalleryWidget(this);
    ui->gridLayout_2->addWidget(m_galleryWidget, 0, 0);

    // Detail panel layout
    auto* detailLayout = new QVBoxLayout(ui->settingWidget);
    detailLayout->setContentsMargins(12, 12, 12, 12);
    detailLayout->setSpacing(12);

    // Tree widget configuration
    ui->treeModels->header()->setStretchLastSection(true);
    ui->treeModels->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeModels->setEditTriggers(QAbstractItemView::EditKeyPressed |
                                    QAbstractItemView::SelectedClicked);
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
    const QStringList candidateRoots = {
        QDir::current().absoluteFilePath("sample_data"),
        QCoreApplication::applicationDirPath() + "/sample_data",
        QCoreApplication::applicationDirPath() + "/../sample_data"
    };

    for (const QString& rootPath : candidateRoots)
    {
        QDir rootDir(rootPath);
        if (!rootDir.exists())
            continue;

        const QStringList schemeDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& name : schemeDirs)
        {
            importSchemeFromDirectory(rootDir.absoluteFilePath(name), false);
        }
        if (!m_schemes.isEmpty())
            break;
    }

    refreshNavigation();
    ui->stackedWidget->setCurrentWidget(ui->planPage);
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
            menu.addAction(tr("添加模型"), this, [this, schemeId]() {
                promptAddModel(schemeId);
            });
            menu.addAction(tr("打开方案目录"), this, [this, schemeId]() {
                if (SchemeRecord* scheme = schemeById(schemeId))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(scheme->directory));
            });
            menu.addSeparator();
            menu.addAction(tr("删除方案"), this, [this, schemeId]() {
                removeSchemeById(schemeId);
            });
        }
        else if (type == ModelItem)
        {
            const QString modelId = item->data(0, IdRole).toString();
            SchemeRecord* owner = nullptr;
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
}

void MainWindow::rebuildTree()
{
    QSignalBlocker blocker(ui->treeModels);
    m_blockTreeSignals = true;

    ui->treeModels->clear();
    m_schemeItems.clear();
    m_modelItems.clear();

    for (const SchemeRecord& scheme : m_schemes)
    {
        auto* schemeItem = new QTreeWidgetItem(ui->treeModels);
        schemeItem->setText(0, scheme.name);
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
        // 如果 m_modelItems 是 QHash
        QHash<QString, QTreeWidgetItem*>::iterator it = m_modelItems.find(modelId);
        if (it != m_modelItems.end())
        {
            ui->treeModels->setCurrentItem(it.value());
            return;
        }
    }

    if (!schemeId.isEmpty())
    {
        // 如果 m_schemeItems 是 QHash
        QHash<QString, QTreeWidgetItem*>::iterator it = m_schemeItems.find(schemeId);
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
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildModelSettingsWidget(*model);
    ui->settingWidget->layout()->addWidget(m_currentDetailWidget);
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

    auto* pathLabel = new QLabel(tr("目录：%1").arg(QDir::toNativeSeparators(scheme.directory)), container);
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
    connect(openBtn, &QPushButton::clicked, this, [path = scheme.directory]() {
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

    auto* pathLabel = new QLabel(tr("目录：%1").arg(QDir::toNativeSeparators(model.directory)), container);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(pathLabel);

    auto* jsonLabel = new QLabel(tr("配置文件：%1").arg(QDir::toNativeSeparators(model.jsonPath)), container);
    jsonLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(jsonLabel);

    auto* builder = new JsonPageBuilder(model.jsonPath, container);
    layout->addWidget(builder, 1);

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

MainWindow::SchemeRecord* MainWindow::schemeByDirectory(const QString& canonicalPath)
{
    for (SchemeRecord& scheme : m_schemes)
    {
        if (canonicalPathForDir(QDir(scheme.directory)) == canonicalPath)
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
    if (schemeByDirectory(canonical))
    {
        if (showError)
            QMessageBox::information(this, tr("提示"),
                                     tr("方案已存在：%1").arg(QDir::toNativeSeparators(dirPath)));
        return QString();
    }

    QVector<ModelRecord> models = scanSchemeFolder(canonical);
    if (models.isEmpty())
    {
        QString jsonPath, batPath;
        if (isModelFolder(dir, &jsonPath, &batPath))
        {
            ModelRecord single;
            single.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            single.name = dir.dirName();
            single.directory = canonical;
            single.jsonPath = jsonPath;
            single.batPath = batPath;
            models.push_back(single);
        }
    }
    if (models.isEmpty())
    {
        if (showError)
            QMessageBox::warning(this, tr("导入失败"),
                                 tr("未在目录中找到有效的模型：%1").arg(QDir::toNativeSeparators(dirPath)));
        return QString();
    }

    SchemeRecord scheme;
    scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    scheme.name = dir.dirName();
    scheme.directory = canonical;

    for (ModelRecord& model : models)
    {
        if (model.id.isEmpty())
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (model.directory.isEmpty())
            model.directory = QDir(canonical).filePath(model.name);
        scheme.models.push_back(model);
    }

    m_schemes.push_back(scheme);
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

    QSet<QString> existingPaths;
    for (const ModelRecord& model : scheme->models)
        existingPaths.insert(canonicalPathForDir(QDir(model.directory)));

    for (const QString& path : paths)
    {
        QDir dir(path);
        if (!dir.exists())
        {
            if (showError)
                QMessageBox::warning(this, tr("导入失败"),
                                     tr("路径不存在：%1").arg(QDir::toNativeSeparators(path)));
            continue;
        }

        const QString canonical = canonicalPathForDir(dir);

        QString jsonPath, batPath;
        if (isModelFolder(dir, &jsonPath, &batPath))
        {
            if (existingPaths.contains(canonical))
                continue;

            ModelRecord model;
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            model.name = dir.dirName();
            model.directory = canonical;
            model.jsonPath = jsonPath;
            model.batPath = batPath;
            scheme->models.push_back(model);
            existingPaths.insert(canonical);
            addedIds.push_back(model.id);
            continue;
        }

        QVector<ModelRecord> nested = scanSchemeFolder(canonical);
        if (nested.isEmpty())
        {
            if (showError)
                QMessageBox::warning(this, tr("导入失败"),
                                     tr("%1 不是有效的模型文件夹。")
                                     .arg(QDir::toNativeSeparators(path)));
            continue;
        }

        for (ModelRecord& model : nested)
        {
            const QString modelCanonical = canonicalPathForDir(QDir(model.directory));
            if (existingPaths.contains(modelCanonical))
                continue;
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            scheme->models.push_back(model);
            existingPaths.insert(modelCanonical);
            addedIds.push_back(model.id);
        }
    }

    if (!addedIds.isEmpty())
        refreshNavigation(schemeId, addedIds.first());
    else
        refreshNavigation(schemeId, m_activeModelId);

    return addedIds;
}

bool MainWindow::isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const
{
    QDir copy(dir);
    const QStringList jsons = copy.entryList(QStringList() << "*.json",
                                             QDir::Files | QDir::NoDotAndDotDot);
    const QStringList bats = copy.entryList(QStringList() << "*.bat",
                                            QDir::Files | QDir::NoDotAndDotDot);
    if (jsons.isEmpty() || bats.isEmpty())
        return false;

    if (jsonPath)
        *jsonPath = copy.absoluteFilePath(jsons.first());
    if (batPath)
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
    const QString dir = QFileDialog::getExistingDirectory(this, tr("选择方案目录"));
    if (dir.isEmpty())
        return;
    const QString id = importSchemeFromDirectory(dir);
    if (!id.isEmpty())
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        selectTreeItem(id, QString());
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
            refreshNavigation();
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
                if (scheme.models.isEmpty())
                {
                    const QString schemeId = scheme.id;
                    m_schemes.removeAt(i);
                    if (m_activeSchemeId == schemeId)
                    {
                        m_activeSchemeId.clear();
                        m_activeModelId.clear();
                    }
                    refreshNavigation();
                }
                else
                {
                    if (m_activeModelId == id)
                        m_activeModelId.clear();
                    refreshNavigation(scheme.id);
                }
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

        if (!scheme.models.isEmpty())
            updated.push_back(scheme);
    }

    m_schemes = updated;
    refreshNavigation(m_activeSchemeId, m_activeModelId);
}
