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
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QDialog>
#include <QHeaderView>
#include <QIcon>
#include <QBrush>
#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QListWidget>
#include <QList>
#include <QInputDialog>
#include <QImageReader>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QLocale>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QSharedPointer>
#include <QSplitter>
#include <QEvent>
#include <QtGlobal>
#include <QVector>
#include <QVector3D>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <algorithm>
#include <functional>
#include <utility>
#include <cmath>

#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNamedColors.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkOBJReader.h>
#include <vtkSTLReader.h>
#include <QSettings>
#include <QDialogButtonBox>

namespace
{
const QString kMaterialsTenantId = QStringLiteral("517397886036606977");
const QString kMaterialsUserId = QStringLiteral("517397886036606977");
const QUrl kMaterialsTokenUrl(QStringLiteral("https://api7.gacrnd.com:9443/gmds/material-gygc-material-ds/custom/basics/token"));
const QUrl kMaterialsPageUrl(QStringLiteral("https://api7.gacrnd.com:9443/gmds/material-gygc-material-ds/custom/api/material-base-info/pageQuery"));
const QUrl kMaterialsDetailUrl(QStringLiteral("https://api7.gacrnd.com:9443/gmds/material-gygc-material-ds/custom/api/material-base-info/queryMaterialProperties"));
const int kMaterialsPageSize = 50;
const char kMaterialsConnectionName[] = "materials_connection";

class ResizeWatcher : public QObject
{
public:
    explicit ResizeWatcher(std::function<void()> callback, QObject* parent = nullptr)
        : QObject(parent)
        , m_callback(std::move(callback))
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event && event->type() == QEvent::Resize && m_callback)
            m_callback();
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_callback;
};

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

QString latestResultFile(const QString& directory)
{
    QDir dir(directory);
    const QStringList objPatterns{ QStringLiteral("*.obj"), QStringLiteral("*.OBJ") };
    QFileInfoList files = dir.entryInfoList(objPatterns, QDir::Files,
                                            QDir::Time | QDir::IgnoreCase);
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
    m_baseWindowTitle = windowTitle();

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(dataRoot);
    if (!dataDir.exists())
        dataDir.mkpath(QStringLiteral("."));
    m_appStateFilePath = dataDir.filePath(QStringLiteral("app_state.json"));
    m_materialsDbPath = dataDir.filePath(QStringLiteral("materials.db"));

    initializeMaterialsDatabase();

    setupUiHelpers();
    loadMaterialsFromDatabase();
    setupConnections();
    loadSchemeLibrary();
    loadInitialSchemes();
}

MainWindow::~MainWindow()
{
    saveSchemesToStorage();
    saveSchemeLibrary();
    saveApplicationState();
    if (m_materialsDb.isValid())
    {
        const QString connectionName = m_materialsDb.connectionName();
        if (m_materialsDb.isOpen())
            m_materialsDb.close();
        m_materialsDb = QSqlDatabase();
        if (!connectionName.isEmpty())
            QSqlDatabase::removeDatabase(connectionName);
    }
    delete ui;
}

void MainWindow::setupUiHelpers()
{
    if (auto* central = ui->centralwidget)
    {
        central->setAttribute(Qt::WA_StyledBackground, true);
        central->setStyleSheet(QStringLiteral("QWidget#centralwidget{background:#f1f5f9;}"));
    }

    m_galleryWidget = new SchemeGalleryWidget(this);
    ui->planPageLayout->addWidget(m_galleryWidget);

    auto* detailLayout = new QVBoxLayout(ui->settingWidget);
    detailLayout->setContentsMargins(20, 20, 20, 20);
    detailLayout->setSpacing(16);

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
            "%3"
        ).arg(panelSelector, titleSelector, extraStyles);
        panel->setStyleSheet(style);
    };

    applyPanelCard(ui->navigationFrame, ui->navigationTitle,
                   QStringLiteral(
                       "QTreeWidget{border:none;background:transparent;padding:8px 12px;}"
                       "QTreeWidget::item{padding:6px 4px;}"
                       "QTreeWidget::item:hover{background:#f1f5f9;}"
                       "QTreeWidget::item:selected{background:#e2e8f0;color:#0f172a;}"
                       "QHeaderView::section{background:transparent;border:none;padding:4px 0;font-weight:600;color:#334155;}"
                   ));

    applyPanelCard(ui->detailPanel, ui->detailTitle,
                   QStringLiteral(
                       "QScrollArea{border:none;background:transparent;}"
                       "QWidget#scrollAreaWidgetContents{background:transparent;}"
                   ));

    applyPanelCard(ui->vtkPanel, ui->vtkTitle,
                   QStringLiteral(
                       "QFrame#vtkFrame{border:none;background:transparent;border-bottom-left-radius:14px;border-bottom-right-radius:14px;}"
                       "QVTKOpenGLNativeWidget{border:none;border-bottom-left-radius:14px;border-bottom-right-radius:14px;}"
                   ));

    applyPanelCard(ui->materialsListFrame, ui->materialsListTitle,
                   QStringLiteral(
                       "QListWidget{border:none;background:transparent;padding:8px 12px;}"
                       "QListWidget::item{padding:6px 4px;}"
                       "QListWidget::item:selected{background:#e2e8f0;border-radius:6px;color:#0f172a;}"
                   ));

    applyPanelCard(ui->materialDetailsFrame, ui->materialDetailsTitle,
                   QStringLiteral(
                       "QLabel#materialBasicInfoLabel{padding:12px 16px;color:#0f172a;line-height:22px;}"
                       "QLabel#materialSpecsLabel{padding:0 16px 12px 16px;color:#475569;}"
                       "QTableWidget{border:none;background:transparent;margin:0 16px 16px 16px;}"
                       "QHeaderView::section{background:#eef2ff;border:none;padding:6px 8px;font-weight:600;color:#1e293b;}"
                   ));

    applyPanelCard(ui->logPanel, ui->logTitle);

    if (ui->modelImageLabel)
    {
        auto* watcher = new ResizeWatcher([this]() { refreshModelImagePreview(); }, ui->modelImageLabel);
        ui->modelImageLabel->installEventFilter(watcher);
    }

    const QString splitterStyle = QStringLiteral(
        "QSplitter::handle{background:#cbd5f5;}"
        "QSplitter::handle:horizontal{width:8px;margin:0 4px;border-radius:4px;}"
        "QSplitter::handle:vertical{height:8px;margin:4px 0;border-radius:4px;}"
    );
    ui->mainSplitter->setStyleSheet(splitterStyle);
    ui->contentSplitter->setStyleSheet(splitterStyle);
    if (ui->visualizationSplitter)
        ui->visualizationSplitter->setStyleSheet(splitterStyle);
    if (ui->materialsSplitter)
        ui->materialsSplitter->setStyleSheet(splitterStyle);

    ui->treeModels->header()->setStretchLastSection(true);
    ui->treeModels->setHeaderHidden(true);
    ui->treeModels->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeModels->setEditTriggers(QAbstractItemView::EditKeyPressed |
                                    QAbstractItemView::SelectedClicked);

    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);

    QList<int> sizes;
    sizes << 50 << 50;
    ui->contentSplitter->setSizes(sizes);
    if (ui->visualizationSplitter)
    {
        ui->visualizationSplitter->setStretchFactor(0, 3);
        ui->visualizationSplitter->setStretchFactor(1, 1);
        ui->visualizationSplitter->setHandleWidth(6);
    }

    ui->logTextEdit->setStyleSheet(
        "QPlainTextEdit{background:#0f172a;color:#f8fafc;border:none;"
        "border-bottom-left-radius:14px;border-bottom-right-radius:14px;padding:12px;"
        "font-family:\"JetBrains Mono\", \"Source Code Pro\", monospace;}"
    );

    setVisualizationVisible(false);
    updateSelectionInfo();

    auto colors = vtkSmartPointer<vtkNamedColors>::New();
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(colors->GetColor3d("AliceBlue").GetData());
    m_renderWindow->AddRenderer(m_renderer);
    ui->vtkWidget->setRenderWindow(m_renderWindow);

    if (ui->materialsListWidget)
    {
        ui->materialsListWidget->setFrameShape(QFrame::NoFrame);
        ui->materialsListWidget->setFocusPolicy(Qt::NoFocus);
    }

    if (ui->materialPropertiesTable)
    {
        ui->materialPropertiesTable->setFocusPolicy(Qt::NoFocus);
        ui->materialPropertiesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->materialPropertiesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui->materialPropertiesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->materialPropertiesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        ui->materialPropertiesTable->verticalHeader()->setVisible(false);
    }

    if (ui->materialBasicInfoLabel)
    {
        ui->materialBasicInfoLabel->setTextFormat(Qt::RichText);
        ui->materialBasicInfoLabel->setWordWrap(true);
    }

    if (ui->materialSpecsLabel)
    {
        ui->materialSpecsLabel->setText(QString());
        ui->materialSpecsLabel->setTextFormat(Qt::RichText);
    }

    if (ui->materialsStatusLabel)
        ui->materialsStatusLabel->setText(tr("尚未同步材料数据"));

    updateModelImagePreview(nullptr);
}

void MainWindow::setupConnections()
{
    if (ui->actionNewProject)
        connect(ui->actionNewProject, &QAction::triggered,
                this, &MainWindow::onNewProjectTriggered);
    if (ui->actionOpenProject)
        connect(ui->actionOpenProject, &QAction::triggered,
                this, &MainWindow::onOpenProjectTriggered);

    connect(ui->treeModels, &QTreeWidget::currentItemChanged,
            this, &MainWindow::handleTreeSelectionChanged);
    connect(ui->treeModels, &QTreeWidget::itemChanged,
            this, &MainWindow::onTreeItemChanged);
    connect(ui->treeModels, &QTreeWidget::itemClicked,
            this, &MainWindow::onTreeItemClicked);
    connect(ui->treeModels, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onTreeItemDoubleClicked);
    connect(ui->treeModels, &QWidget::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenuRequested);

    if (auto* schemeTree = qobject_cast<SchemeTreeWidget*>(ui->treeModels))
    {
        connect(schemeTree, &SchemeTreeWidget::itemsReordered,
                this, &MainWindow::onTreeItemsReordered);
        connect(schemeTree, &SchemeTreeWidget::externalPathsDropped,
                this, &MainWindow::onExternalDrop);
    }

    connect(m_galleryWidget, &SchemeGalleryWidget::schemeOpenRequested,
            this, &MainWindow::onGalleryOpenRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::schemeDeleteRequested,
            this, &MainWindow::onGalleryDeleteRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::schemeDetailsRequested,
            this, &MainWindow::onGalleryDetailsRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::createSchemeRequested,
            this, &MainWindow::onAddLibraryScheme);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, ui->treeModels);
    connect(deleteShortcut, &QShortcut::activated,
            this, &MainWindow::deleteCurrentTreeItem);
}

void MainWindow::initializeMaterialsDatabase()
{
    if (QSqlDatabase::contains(QString::fromLatin1(kMaterialsConnectionName)))
        m_materialsDb = QSqlDatabase::database(QString::fromLatin1(kMaterialsConnectionName));
    else
        m_materialsDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                  QString::fromLatin1(kMaterialsConnectionName));

    if (!m_materialsDb.isValid())
    {
        appendLogMessage(tr("无法初始化材料数据库连接。"));
        return;
    }

    m_materialsDb.setDatabaseName(m_materialsDbPath);
    if (!m_materialsDb.isOpen() && !m_materialsDb.open())
    {
        appendLogMessage(tr("无法打开材料数据库：%1").arg(m_materialsDb.lastError().text()));
        return;
    }

    QSqlQuery query(m_materialsDb);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON;")))
        appendLogMessage(tr("无法启用材料数据库外键支持：%1").arg(query.lastError().text()));

    const QString createMaterials = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS materials ("
        "material_key TEXT PRIMARY KEY,"
        "base_id TEXT,"
        "material_id TEXT,"
        "material_name TEXT,"
        "material_type TEXT,"
        "material_type_code TEXT,"
        "material_type_value TEXT,"
        "material_status TEXT,"
        "supplier_option_value TEXT,"
        "supplier_option_code TEXT,"
        "supplier_code TEXT,"
        "supplier_produce_code TEXT,"
        "material_trademark TEXT,"
        "gac_material_trademark TEXT,"
        "authentication_status_value TEXT,"
        "standard_type TEXT,"
        "standard_code TEXT,"
        "creation_date TEXT,"
        "last_update_date TEXT,"
        "status TEXT,"
        "form_id TEXT"
        ");");
    if (!query.exec(createMaterials))
        appendLogMessage(tr("无法创建材料表：%1").arg(query.lastError().text()));

    const QString createProperties = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS material_properties ("
        "property_id TEXT PRIMARY KEY,"
        "material_key TEXT,"
        "property_name TEXT,"
        "property_value TEXT,"
        "property_unit TEXT,"
        "FOREIGN KEY(material_key) REFERENCES materials(material_key) ON DELETE CASCADE"
        ");");
    if (!query.exec(createProperties))
        appendLogMessage(tr("无法创建材料属性表：%1").arg(query.lastError().text()));

    const QString createSpecs = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS material_specs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "material_key TEXT,"
        "spec_value TEXT,"
        "FOREIGN KEY(material_key) REFERENCES materials(material_key) ON DELETE CASCADE"
        ");");
    if (!query.exec(createSpecs))
        appendLogMessage(tr("无法创建材料规格表：%1").arg(query.lastError().text()));
}

void MainWindow::loadMaterialsFromDatabase()
{
    m_materials.clear();

    if (!m_materialsDb.isValid())
    {
        refreshMaterialsUi();
        return;
    }

    if (!m_materialsDb.isOpen() && !m_materialsDb.open())
    {
        appendLogMessage(tr("无法打开材料数据库：%1").arg(m_materialsDb.lastError().text()));
        refreshMaterialsUi();
        return;
    }

    QSqlQuery query(m_materialsDb);
    const QString selectMaterials = QStringLiteral(
        "SELECT material_key, base_id, material_id, material_name, material_type, material_type_code,"
        " material_type_value, material_status, supplier_option_value, supplier_option_code,"
        " supplier_code, supplier_produce_code, material_trademark, gac_material_trademark,"
        " authentication_status_value, standard_type, standard_code, creation_date, last_update_date,"
        " status, form_id FROM materials ORDER BY material_name COLLATE NOCASE, material_key COLLATE NOCASE;"
    );

    if (!query.exec(selectMaterials))
    {
        appendLogMessage(tr("读取材料数据失败：%1").arg(query.lastError().text()));
        refreshMaterialsUi();
        return;
    }

    while (query.next())
    {
        MaterialRecord record;
        record.materialKey = query.value(0).toString();
        record.baseId = query.value(1).toString();
        record.materialId = query.value(2).toString();
        record.materialName = query.value(3).toString();
        record.materialType = query.value(4).toString();
        record.materialTypeCode = query.value(5).toString();
        record.materialTypeValue = query.value(6).toString();
        record.materialStatus = query.value(7).toString();
        record.supplierOptionValue = query.value(8).toString();
        record.supplierOptionCode = query.value(9).toString();
        record.supplierCode = query.value(10).toString();
        record.supplierProduceCode = query.value(11).toString();
        record.materialTrademark = query.value(12).toString();
        record.gacMaterialTrademark = query.value(13).toString();
        record.authenticationStatusValue = query.value(14).toString();
        record.standardType = query.value(15).toString();
        record.standardCode = query.value(16).toString();
        record.creationDate = query.value(17).toString();
        record.lastUpdateDate = query.value(18).toString();
        record.status = query.value(19).toString();
        record.formId = query.value(20).toString();
        m_materials.append(record);
    }

    QSqlQuery propertyQuery(m_materialsDb);
    propertyQuery.prepare(QStringLiteral(
        "SELECT property_id, property_name, property_value, property_unit FROM material_properties"
        " WHERE material_key = ? ORDER BY property_name COLLATE NOCASE;"));

    QSqlQuery specQuery(m_materialsDb);
    specQuery.prepare(QStringLiteral(
        "SELECT spec_value FROM material_specs WHERE material_key = ? ORDER BY id;"));

    for (MaterialRecord& record : m_materials)
    {
        record.properties.clear();
        record.specs.clear();

        propertyQuery.bindValue(0, record.materialKey);
        if (propertyQuery.exec())
        {
            while (propertyQuery.next())
            {
                MaterialProperty property;
                property.id = propertyQuery.value(0).toString();
                property.name = propertyQuery.value(1).toString();
                property.value = propertyQuery.value(2).toString();
                property.unit = propertyQuery.value(3).toString();
                record.properties.append(property);
            }
        }
        else
        {
            appendLogMessage(tr("读取材料属性失败：%1").arg(propertyQuery.lastError().text()));
        }
        propertyQuery.finish();

        specQuery.bindValue(0, record.materialKey);
        if (specQuery.exec())
        {
            while (specQuery.next())
            {
                const QString spec = specQuery.value(0).toString().trimmed();
                if (!spec.isEmpty())
                    record.specs.append(spec);
            }
        }
        else
        {
            appendLogMessage(tr("读取材料规格失败：%1").arg(specQuery.lastError().text()));
        }
        specQuery.finish();
    }

    refreshMaterialsUi();
}

void MainWindow::saveMaterialsToDatabase(const QVector<MaterialRecord>& materials)
{
    if (!m_materialsDb.isValid())
        return;

    if (!m_materialsDb.isOpen() && !m_materialsDb.open())
    {
        appendLogMessage(tr("无法打开材料数据库：%1").arg(m_materialsDb.lastError().text()));
        return;
    }

    QSqlQuery query(m_materialsDb);
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE TRANSACTION;")))
    {
        appendLogMessage(tr("无法开始材料数据事务：%1").arg(query.lastError().text()));
        return;
    }

    const auto rollback = [this, &query]() {
        if (!query.exec(QStringLiteral("ROLLBACK;")))
            appendLogMessage(tr("材料数据回滚失败：%1").arg(query.lastError().text()));
    };

    if (!query.exec(QStringLiteral("DELETE FROM material_properties;")))
    {
        appendLogMessage(tr("清空材料属性失败：%1").arg(query.lastError().text()));
        rollback();
        return;
    }

    if (!query.exec(QStringLiteral("DELETE FROM material_specs;")))
    {
        appendLogMessage(tr("清空材料规格失败：%1").arg(query.lastError().text()));
        rollback();
        return;
    }

    if (!query.exec(QStringLiteral("DELETE FROM materials;")))
    {
        appendLogMessage(tr("清空材料基础信息失败：%1").arg(query.lastError().text()));
        rollback();
        return;
    }

    QSqlQuery insertMaterial(m_materialsDb);
    insertMaterial.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO materials(material_key, base_id, material_id, material_name, material_type,"
        " material_type_code, material_type_value, material_status, supplier_option_value,"
        " supplier_option_code, supplier_code, supplier_produce_code, material_trademark,"
        " gac_material_trademark, authentication_status_value, standard_type, standard_code,"
        " creation_date, last_update_date, status, form_id)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);"));

    QSqlQuery insertProperty(m_materialsDb);
    insertProperty.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO material_properties(property_id, material_key, property_name, property_value, property_unit)"
        " VALUES(?,?,?,?,?);"));

    QSqlQuery insertSpec(m_materialsDb);
    insertSpec.prepare(QStringLiteral(
        "INSERT INTO material_specs(material_key, spec_value) VALUES(?,?);"));

    for (const MaterialRecord& record : materials)
    {
        QString key = record.materialKey.trimmed();
        if (key.isEmpty())
        {
            if (!record.materialId.trimmed().isEmpty())
                key = record.materialId.trimmed();
            else if (!record.baseId.trimmed().isEmpty())
                key = record.baseId.trimmed();
            else
                key = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        insertMaterial.bindValue(0, key);
        insertMaterial.bindValue(1, record.baseId);
        insertMaterial.bindValue(2, record.materialId);
        insertMaterial.bindValue(3, record.materialName);
        insertMaterial.bindValue(4, record.materialType);
        insertMaterial.bindValue(5, record.materialTypeCode);
        insertMaterial.bindValue(6, record.materialTypeValue);
        insertMaterial.bindValue(7, record.materialStatus);
        insertMaterial.bindValue(8, record.supplierOptionValue);
        insertMaterial.bindValue(9, record.supplierOptionCode);
        insertMaterial.bindValue(10, record.supplierCode);
        insertMaterial.bindValue(11, record.supplierProduceCode);
        insertMaterial.bindValue(12, record.materialTrademark);
        insertMaterial.bindValue(13, record.gacMaterialTrademark);
        insertMaterial.bindValue(14, record.authenticationStatusValue);
        insertMaterial.bindValue(15, record.standardType);
        insertMaterial.bindValue(16, record.standardCode);
        insertMaterial.bindValue(17, record.creationDate);
        insertMaterial.bindValue(18, record.lastUpdateDate);
        insertMaterial.bindValue(19, record.status);
        insertMaterial.bindValue(20, record.formId);

        if (!insertMaterial.exec())
        {
            appendLogMessage(tr("写入材料基础信息失败：%1").arg(insertMaterial.lastError().text()));
            rollback();
            return;
        }
        insertMaterial.finish();

        for (const MaterialProperty& property : record.properties)
        {
            QString propertyId = property.id.trimmed();
            if (propertyId.isEmpty())
                propertyId = QUuid::createUuid().toString(QUuid::WithoutBraces);

            insertProperty.bindValue(0, propertyId);
            insertProperty.bindValue(1, key);
            insertProperty.bindValue(2, property.name);
            insertProperty.bindValue(3, property.value);
            insertProperty.bindValue(4, property.unit);

            if (!insertProperty.exec())
            {
                appendLogMessage(tr("写入材料属性失败：%1").arg(insertProperty.lastError().text()));
                rollback();
                return;
            }
            insertProperty.finish();
        }

        for (const QString& spec : record.specs)
        {
            insertSpec.bindValue(0, key);
            insertSpec.bindValue(1, spec);
            if (!insertSpec.exec())
            {
                appendLogMessage(tr("写入材料规格失败：%1").arg(insertSpec.lastError().text()));
                rollback();
                return;
            }
            insertSpec.finish();
        }
    }

    if (!query.exec(QStringLiteral("COMMIT;")))
        appendLogMessage(tr("提交材料数据失败：%1").arg(query.lastError().text()));
}

void MainWindow::refreshMaterialsUi()
{
    if (!ui->materialsListWidget)
        return;

    QString previousKey;
    if (QListWidgetItem* current = ui->materialsListWidget->currentItem())
        previousKey = current->data(Qt::UserRole).toString();

    QString selectedKey;

    {
        QSignalBlocker blocker(ui->materialsListWidget);
        ui->materialsListWidget->clear();

        for (const MaterialRecord& material : m_materials)
        {
            auto* item = new QListWidgetItem(materialDisplayName(material));
            item->setData(Qt::UserRole, material.materialKey);

            QStringList tooltipLines;
            const QString typeText = material.materialTypeValue.isEmpty()
                                         ? material.materialType
                                         : material.materialTypeValue;
            if (!typeText.isEmpty())
                tooltipLines << tr("类型：%1").arg(typeText);
            if (!material.supplierOptionValue.isEmpty())
                tooltipLines << tr("供应商：%1").arg(material.supplierOptionValue);
            else if (!material.supplierCode.isEmpty())
                tooltipLines << tr("供应商：%1").arg(material.supplierCode);
            if (!material.materialStatus.isEmpty())
                tooltipLines << tr("状态：%1").arg(material.materialStatus);
            if (!material.standardCode.isEmpty())
                tooltipLines << tr("标准：%1").arg(material.standardCode);
            if (!tooltipLines.isEmpty())
                item->setToolTip(tooltipLines.join(QLatin1Char('\n')));

            ui->materialsListWidget->addItem(item);
        }

        int rowToSelect = -1;
        if (!previousKey.isEmpty())
        {
            for (int i = 0; i < ui->materialsListWidget->count(); ++i)
            {
                QListWidgetItem* item = ui->materialsListWidget->item(i);
                if (item->data(Qt::UserRole).toString() == previousKey)
                {
                    rowToSelect = i;
                    break;
                }
            }
        }

        if (rowToSelect < 0 && ui->materialsListWidget->count() > 0)
            rowToSelect = 0;

        if (rowToSelect >= 0)
        {
            ui->materialsListWidget->setCurrentRow(rowToSelect);
            selectedKey = ui->materialsListWidget->item(rowToSelect)->data(Qt::UserRole).toString();
        }
    }

    if (ui->materialsStatusLabel)
        ui->materialsStatusLabel->setText(tr("已加载 %1 条材料数据").arg(m_materials.size()));

    if (!selectedKey.isEmpty())
        displayMaterialDetails(materialByKey(selectedKey));
    else
        displayMaterialDetails(nullptr);
}

void MainWindow::displayMaterialDetails(const MaterialRecord* material)
{
    if (!ui->materialBasicInfoLabel || !ui->materialPropertiesTable)
        return;

    if (!material)
    {
        ui->materialBasicInfoLabel->setText(tr("请选择材料以查看详细信息。"));
        if (ui->materialSpecsLabel)
            ui->materialSpecsLabel->clear();
        ui->materialPropertiesTable->clearContents();
        ui->materialPropertiesTable->setRowCount(0);
        return;
    }

    QStringList infoLines;

    if (!material->materialName.isEmpty())
        infoLines << QStringLiteral("<h3 style=\"margin:0;\">%1</h3>").arg(material->materialName.toHtmlEscaped());

    QStringList primaryMeta;
    const QString typeText = material->materialTypeValue.isEmpty()
                                 ? material->materialType
                                 : material->materialTypeValue;
    if (!typeText.isEmpty())
        primaryMeta << tr("类型：%1").arg(typeText.toHtmlEscaped());
    if (!material->materialStatus.isEmpty())
        primaryMeta << tr("状态：%1").arg(material->materialStatus.toHtmlEscaped());
    if (!material->supplierOptionValue.isEmpty())
        primaryMeta << tr("供应商：%1").arg(material->supplierOptionValue.toHtmlEscaped());
    else if (!material->supplierCode.isEmpty())
        primaryMeta << tr("供应商：%1").arg(material->supplierCode.toHtmlEscaped());
    if (!primaryMeta.isEmpty())
        infoLines << primaryMeta.join(QStringLiteral("  |  "));

    QStringList extraMeta;
    const QString trademark = !material->gacMaterialTrademark.isEmpty()
                                  ? material->gacMaterialTrademark
                                  : material->materialTrademark;
    if (!trademark.isEmpty())
        extraMeta << tr("材料牌号：%1").arg(trademark.toHtmlEscaped());
    if (!material->standardCode.isEmpty())
        extraMeta << tr("标准：%1").arg(material->standardCode.toHtmlEscaped());
    if (!material->authenticationStatusValue.isEmpty())
        extraMeta << tr("认证状态：%1").arg(material->authenticationStatusValue.toHtmlEscaped());
    if (!extraMeta.isEmpty())
        infoLines << extraMeta.join(QStringLiteral("  |  "));

    QStringList timeMeta;
    if (!material->creationDate.isEmpty())
        timeMeta << tr("创建时间：%1").arg(material->creationDate.toHtmlEscaped());
    if (!material->lastUpdateDate.isEmpty())
        timeMeta << tr("更新时间：%1").arg(material->lastUpdateDate.toHtmlEscaped());
    if (!timeMeta.isEmpty())
        infoLines << timeMeta.join(QStringLiteral("  |  "));

    if (infoLines.isEmpty())
        infoLines << materialDisplayName(*material).toHtmlEscaped();

    ui->materialBasicInfoLabel->setText(infoLines.join(QStringLiteral("<br/>")));

    if (ui->materialSpecsLabel)
    {
        if (material->specs.isEmpty())
            ui->materialSpecsLabel->clear();
        else
            ui->materialSpecsLabel->setText(tr("可选规格：%1").arg(material->specs.join(QStringLiteral("、")).toHtmlEscaped()));
    }

    ui->materialPropertiesTable->clearContents();
    ui->materialPropertiesTable->setRowCount(material->properties.size());
    for (int row = 0; row < material->properties.size(); ++row)
    {
        const MaterialProperty& prop = material->properties.at(row);
        ui->materialPropertiesTable->setItem(row, 0, new QTableWidgetItem(prop.name));
        ui->materialPropertiesTable->setItem(row, 1, new QTableWidgetItem(prop.value));
        ui->materialPropertiesTable->setItem(row, 2, new QTableWidgetItem(prop.unit));
    }
    ui->materialPropertiesTable->scrollToTop();
}

const MainWindow::MaterialRecord* MainWindow::materialByKey(const QString& key) const
{
    if (key.isEmpty())
        return nullptr;

    for (const MaterialRecord& record : m_materials)
    {
        if (record.materialKey == key)
            return &record;
    }
    return nullptr;
}

QString MainWindow::materialDisplayName(const MaterialRecord& material) const
{
    QStringList parts;
    if (!material.materialName.isEmpty())
        parts << material.materialName;

    const QString trademark = !material.gacMaterialTrademark.isEmpty()
                                  ? material.gacMaterialTrademark
                                  : material.materialTrademark;
    if (!trademark.isEmpty())
        parts << QStringLiteral("(%1)").arg(trademark);

    if (!material.materialTypeValue.isEmpty())
        parts << QStringLiteral("- %1").arg(material.materialTypeValue);

    if (parts.isEmpty())
        return material.materialKey.isEmpty() ? tr("未命名材料") : material.materialKey;

    return parts.join(QStringLiteral(" "));
}

bool MainWindow::parseMaterialsPage(const QJsonObject& root,
                                    QVector<MaterialRecord>* outRecords,
                                    int* totalOut,
                                    QString* errorMessage) const
{
    if (!outRecords)
        return false;

    if (errorMessage)
        errorMessage->clear();

    const QString code = root.value(QStringLiteral("code")).toString();
    if (code.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0)
    {
        if (errorMessage)
            *errorMessage = tr("材料列表接口返回错误：%1").arg(code);
        return false;
    }

    int totalValue = -1;
    const QJsonValue totalJson = root.value(QStringLiteral("total"));
    if (!totalJson.isUndefined())
        totalValue = totalJson.toVariant().toInt();
    if (totalOut)
        *totalOut = totalValue;

    const QJsonArray table = root.value(QStringLiteral("table")).toArray();
    for (const QJsonValue& value : table)
    {
        const QJsonObject obj = value.toObject();
        MaterialRecord record;
        record.baseId = obj.value(QStringLiteral("id")).toString();
        record.materialId = obj.value(QStringLiteral("materialId")).toString();
        record.materialKey = record.materialId.trimmed().isEmpty() ? record.baseId : record.materialId;
        record.materialName = obj.value(QStringLiteral("materialName")).toString();
        record.materialType = obj.value(QStringLiteral("materialType")).toString();
        record.materialTypeCode = obj.value(QStringLiteral("materialTypeCode")).toString();
        record.materialTypeValue = obj.value(QStringLiteral("materialTypeValue")).toString();
        record.materialStatus = obj.value(QStringLiteral("materialStatus")).toString();
        record.supplierOptionValue = obj.value(QStringLiteral("supplierOptionValue")).toString();
        record.supplierOptionCode = obj.value(QStringLiteral("supplierOptionCode")).toString();
        record.supplierCode = obj.value(QStringLiteral("supplierCode")).toString();
        record.supplierProduceCode = obj.value(QStringLiteral("supplierProduceCode")).toString();
        record.materialTrademark = obj.value(QStringLiteral("materialTrademark")).toString();
        record.gacMaterialTrademark = obj.value(QStringLiteral("gacMaterialTrademark")).toString();
        record.authenticationStatusValue = obj.value(QStringLiteral("authenticationStatusValue")).toString(
            obj.value(QStringLiteral("authenticationStatus")).toString());
        record.standardType = obj.value(QStringLiteral("standardType")).toString();
        record.standardCode = obj.value(QStringLiteral("standardCode")).toString();
        record.creationDate = obj.value(QStringLiteral("creationDate")).toString();
        record.lastUpdateDate = obj.value(QStringLiteral("lastUpdateDate")).toString();
        record.status = obj.value(QStringLiteral("status")).toString();
        record.formId = obj.value(QStringLiteral("formId")).toString();

        if (record.materialKey.trimmed().isEmpty())
        {
            if (!record.materialId.trimmed().isEmpty())
                record.materialKey = record.materialId.trimmed();
            else if (!record.baseId.trimmed().isEmpty())
                record.materialKey = record.baseId.trimmed();
        }

        outRecords->append(record);
    }

    return true;
}

bool MainWindow::applyMaterialDetail(MaterialRecord& record,
                                     const QJsonObject& detailRoot,
                                     QString* errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();

    const QString detailCode = detailRoot.value(QStringLiteral("code")).toString();
    if (detailCode.compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0)
    {
        if (errorMessage)
            *errorMessage = tr("材料 %1 详细信息返回错误：%2")
                                .arg(materialDisplayName(record), detailCode);
        return false;
    }

    const QJsonObject dataObj = detailRoot.value(QStringLiteral("data")).toObject();
    const QJsonArray baseList = dataObj.value(QStringLiteral("materialBaseDataList")).toArray();
    if (!baseList.isEmpty())
    {
        const QJsonObject entry = baseList.first().toObject();
        const QJsonObject info = entry.value(QStringLiteral("materialBaseInfo")).toObject();
        if (!info.isEmpty())
        {
            record.baseId = info.value(QStringLiteral("id")).toString(record.baseId);
            const QString materialId = info.value(QStringLiteral("materialId")).toString();
            if (!materialId.trimmed().isEmpty())
            {
                record.materialId = materialId.trimmed();
                record.materialKey = record.materialId;
            }
            record.materialName = info.value(QStringLiteral("materialName")).toString(record.materialName);
            record.materialType = info.value(QStringLiteral("materialType")).toString(record.materialType);
            record.materialTypeValue = info.value(QStringLiteral("materialTypeValue")).toString(record.materialTypeValue);
            record.materialTypeCode = info.value(QStringLiteral("materialTypeCode")).toString(record.materialTypeCode);
            record.materialStatus = info.value(QStringLiteral("materialStatus")).toString(record.materialStatus);
            record.supplierOptionValue = info.value(QStringLiteral("supplierOptionValue")).toString(record.supplierOptionValue);
            record.supplierOptionCode = info.value(QStringLiteral("supplierOptionCode")).toString(record.supplierOptionCode);
            record.supplierCode = info.value(QStringLiteral("supplierCode")).toString(record.supplierCode);
            record.supplierProduceCode = info.value(QStringLiteral("supplierProduceCode")).toString(record.supplierProduceCode);
            record.materialTrademark = info.value(QStringLiteral("materialTrademark")).toString(record.materialTrademark);
            record.gacMaterialTrademark = info.value(QStringLiteral("gacMaterialTrademark")).toString(record.gacMaterialTrademark);
            record.authenticationStatusValue = info.value(QStringLiteral("authenticationStatusValue")).toString(
                record.authenticationStatusValue);
            record.standardType = info.value(QStringLiteral("standardType")).toString(record.standardType);
            record.standardCode = info.value(QStringLiteral("standardCode")).toString(record.standardCode);
            record.creationDate = info.value(QStringLiteral("creationDate")).toString(record.creationDate);
            record.lastUpdateDate = info.value(QStringLiteral("lastUpdateDate")).toString(record.lastUpdateDate);
            record.status = info.value(QStringLiteral("status")).toString(record.status);
            record.formId = info.value(QStringLiteral("formId")).toString(record.formId);
        }

        record.properties.clear();
        const QJsonArray propertiesArray = entry.value(QStringLiteral("materialPropertiesEntityList")).toArray();
        for (const QJsonValue& propValue : propertiesArray)
        {
            const QJsonObject propObj = propValue.toObject();
            MaterialProperty property;
            property.id = propObj.value(QStringLiteral("id")).toString();
            property.name = propObj.value(QStringLiteral("propertyName")).toString();
            const QJsonValue numberValue = propObj.value(QStringLiteral("propertyNumericalValue"));
            if (numberValue.isDouble())
                property.value = QString::number(numberValue.toDouble());
            else
                property.value = numberValue.toVariant().toString();
            property.unit = propObj.value(QStringLiteral("propertyUnit")).toString();
            record.properties.append(property);
        }
    }
    else
    {
        record.properties.clear();
    }

    record.specs.clear();
    const QJsonArray specArray = dataObj.value(QStringLiteral("specList")).toArray();
    for (const QJsonValue& specValue : specArray)
    {
        QString spec = specValue.toVariant().toString().trimmed();
        if (!spec.isEmpty())
            record.specs.append(spec);
    }

    return true;
}

QVector<MainWindow::MaterialRecord> MainWindow::loadMaterialsFromTestData(QString* errorMessage) const
{
    if (errorMessage)
        *errorMessage = QString();

    QVector<MaterialRecord> materials;

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir appDirPath(appDir);
    QStringList searchRoots;
    searchRoots << appDirPath.filePath(QStringLiteral("sample_data/materials"));
    searchRoots << appDirPath.filePath(QStringLiteral("../sample_data/materials"));
    searchRoots << QDir::current().filePath(QStringLiteral("sample_data/materials"));

    QString basePath;
    for (const QString& candidate : std::as_const(searchRoots))
    {
        QDir dir(candidate);
        if (dir.exists())
        {
            basePath = dir.canonicalPath();
            if (basePath.isEmpty())
                basePath = dir.absolutePath();
            break;
        }
    }

    if (basePath.isEmpty())
    {
        if (errorMessage)
            *errorMessage = tr("未找到材料测试数据目录。请确认 sample_data/materials 已存在。");
        return materials;
    }

    QDir baseDir(basePath);
    const QStringList pageFiles = baseDir.entryList(QStringList() << QStringLiteral("materials_page*.json"), QDir::Files, QDir::Name);
    if (pageFiles.isEmpty())
    {
        if (errorMessage)
            *errorMessage = tr("材料测试数据目录中缺少列表文件 materials_page*.json。");
        return materials;
    }

    QSet<QString> seenKeys;
    for (const QString& fileName : pageFiles)
    {
        QFile file(baseDir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly))
        {
            if (errorMessage)
                *errorMessage = tr("无法读取材料测试数据文件：%1").arg(file.fileName());
            return {};
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            if (errorMessage)
                *errorMessage = tr("解析材料测试数据失败：%1").arg(file.fileName());
            return {};
        }

        QVector<MaterialRecord> pageRecords;
        int pageTotal = -1;
        QString parseErrorMessage;
        if (!parseMaterialsPage(doc.object(), &pageRecords, &pageTotal, &parseErrorMessage))
        {
            if (errorMessage)
                *errorMessage = parseErrorMessage;
            return {};
        }

        for (MaterialRecord& record : pageRecords)
        {
            const QString key = record.materialKey.trimmed();
            if (!key.isEmpty())
            {
                if (seenKeys.contains(key))
                    continue;
                seenKeys.insert(key);
            }
            materials.append(record);
        }
    }

    const QStringList detailFiles = baseDir.entryList(QStringList() << QStringLiteral("material_detail_*.json"), QDir::Files, QDir::Name);
    QHash<QString, QString> detailFileMap;
    for (const QString& fileName : detailFiles)
    {
        const QString baseName = fileName.mid(QStringLiteral("material_detail_").size(), fileName.size() - QStringLiteral("material_detail_").size() - QStringLiteral(".json").size());
        detailFileMap.insert(baseName.toLower(), baseDir.filePath(fileName));
    }

    const auto sanitizeKey = [](const QString& key) {
        QString cleaned = key;
        cleaned.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_\-]")), QStringLiteral("_"));
        return cleaned.toLower();
    };

    for (MaterialRecord& record : materials)
    {
        const QString trademark = !record.gacMaterialTrademark.isEmpty() ? record.gacMaterialTrademark : record.materialTrademark;
        if (trademark.isEmpty())
            continue;

        const QString sanitized = sanitizeKey(trademark);
        const QString detailPath = detailFileMap.value(sanitized);
        if (detailPath.isEmpty())
            continue;

        QFile detailFile(detailPath);
        if (!detailFile.open(QIODevice::ReadOnly))
        {
            if (errorMessage)
                *errorMessage = tr("无法读取材料详细测试数据：%1").arg(detailFile.fileName());
            return {};
        }

        QJsonParseError detailError{};
        const QJsonDocument detailDoc = QJsonDocument::fromJson(detailFile.readAll(), &detailError);
        if (detailError.error != QJsonParseError::NoError || !detailDoc.isObject())
        {
            if (errorMessage)
                *errorMessage = tr("解析材料详细测试数据失败：%1").arg(detailFile.fileName());
            return {};
        }

        QString applyError;
        if (!applyMaterialDetail(record, detailDoc.object(), &applyError))
        {
            if (errorMessage)
                *errorMessage = applyError;
            return {};
        }
    }

    if (errorMessage)
        errorMessage->clear();
    return materials;
}

QVector<MainWindow::MaterialRecord> MainWindow::fetchMaterialsFromRemote(QString* errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    appendLogMessage(tr("正在向材料数据服务请求数据..."));

    QUrl tokenUrl(kMaterialsTokenUrl);
    QUrlQuery tokenQuery(tokenUrl);
    tokenQuery.addQueryItem(QStringLiteral("userId"), kMaterialsUserId);
    tokenUrl.setQuery(tokenQuery);

    QString networkError;
    QByteArray tokenData = performGetRequest(tokenUrl, {}, &networkError);
    if (!networkError.isEmpty())
    {
        if (errorMessage)
            *errorMessage = tr("获取材料访问令牌失败：%1").arg(networkError);
        return {};
    }

    const QString token = QString::fromUtf8(tokenData).trimmed();
    if (token.isEmpty())
    {
        if (errorMessage)
            *errorMessage = tr("材料访问令牌为空。");
        return {};
    }

    QVector<MaterialRecord> materials;
    QSet<QString> seenKeys;
    int page = 1;
    int total = -1;

    const auto makeHeaders = [&token](const QString& timestamp) {
        QMap<QString, QString> headers;
        headers.insert(QStringLiteral("Xdaptenantid"), kMaterialsTenantId);
        headers.insert(QStringLiteral("Xdaptimestamp"), timestamp);
        headers.insert(QStringLiteral("Xdaptoken"), token);
        headers.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json"));
        headers.insert(QStringLiteral("Accept"), QStringLiteral("application/json"));
        return headers;
    };

    QJsonObject pageBody;
    pageBody.insert(QStringLiteral("searchContent"), QString());
    pageBody.insert(QStringLiteral("materialType"), QJsonValue::Null);

    while (true)
    {
        QUrl pageUrl(kMaterialsPageUrl);
        QUrlQuery pageQuery(pageUrl);
        pageQuery.addQueryItem(QStringLiteral("page"), QString::number(page));
        pageQuery.addQueryItem(QStringLiteral("pageSize"), QString::number(kMaterialsPageSize));
        pageUrl.setQuery(pageQuery);

        const QByteArray payload = QJsonDocument(pageBody).toJson(QJsonDocument::Compact);
        const QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
        networkError.clear();
        QByteArray response = performPostRequest(pageUrl, payload, makeHeaders(timestamp), &networkError);
        if (!networkError.isEmpty())
        {
            if (errorMessage)
                *errorMessage = tr("获取材料列表失败：%1").arg(networkError);
            return {};
        }

        if (response.isEmpty())
            break;

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            if (errorMessage)
                *errorMessage = tr("解析材料列表数据失败：%1").arg(QString::fromUtf8(response.left(256)));
            return {};
        }

        QVector<MaterialRecord> pageRecords;
        int pageTotal = -1;
        QString parseErrorMessage;
        if (!parseMaterialsPage(doc.object(), &pageRecords, &pageTotal, &parseErrorMessage))
        {
            if (errorMessage)
                *errorMessage = parseErrorMessage;
            return {};
        }

        if (total < 0 && pageTotal >= 0)
            total = pageTotal;

        for (MaterialRecord& record : pageRecords)
        {
            const QString key = record.materialKey.trimmed();
            if (!key.isEmpty())
            {
                if (seenKeys.contains(key))
                    continue;
                seenKeys.insert(key);
            }
            materials.append(record);
        }

        if (pageRecords.isEmpty())
            break;

        if (total >= 0 && materials.size() >= total)
            break;

        ++page;
    }

    const QUrl detailUrl = kMaterialsDetailUrl;
    for (MaterialRecord& record : materials)
    {
        const QString trademark = !record.gacMaterialTrademark.isEmpty()
                                       ? record.gacMaterialTrademark
                                       : record.materialTrademark;
        if (trademark.isEmpty())
        {
            record.properties.clear();
            record.specs.clear();
            continue;
        }

        QJsonObject detailBody;
        detailBody.insert(QStringLiteral("materialTrademark"), trademark);
        const QByteArray detailPayload = QJsonDocument(detailBody).toJson(QJsonDocument::Compact);
        const QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
        networkError.clear();
        QByteArray detailResponse = performPostRequest(detailUrl, detailPayload, makeHeaders(timestamp), &networkError);
        if (!networkError.isEmpty())
        {
            if (errorMessage)
                *errorMessage = tr("获取材料 %1 详细信息失败：%2")
                                    .arg(materialDisplayName(record), networkError);
            return {};
        }

        QJsonParseError detailError{};
        const QJsonDocument detailDoc = QJsonDocument::fromJson(detailResponse, &detailError);
        if (detailError.error != QJsonParseError::NoError || !detailDoc.isObject())
        {
            if (errorMessage)
                *errorMessage = tr("解析材料 %1 详细信息失败：%2")
                                    .arg(materialDisplayName(record), QString::fromUtf8(detailResponse.left(256)));
            return {};
        }

        QString detailErrorMessage;
        if (!applyMaterialDetail(record, detailDoc.object(), &detailErrorMessage))
        {
            if (errorMessage)
                *errorMessage = detailErrorMessage;
            return {};
        }
    }

    appendLogMessage(tr("共获取 %1 条材料记录。").arg(materials.size()));

    if (errorMessage)
        errorMessage->clear();
    return materials;
}

QByteArray MainWindow::performGetRequest(const QUrl& url,
                                         const QMap<QString, QString>& headers,
                                         QString* errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();

    QNetworkRequest request(url);
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError)
        data = reply->readAll();
    else if (errorMessage)
        *errorMessage = reply->errorString();

    reply->deleteLater();
    return data;
}

QByteArray MainWindow::performPostRequest(const QUrl& url,
                                          const QByteArray& body,
                                          const QMap<QString, QString>& headers,
                                          QString* errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();

    QNetworkRequest request(url);
    bool hasContentType = false;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
    {
        if (it.key().compare(QStringLiteral("Content-Type"), Qt::CaseInsensitive) == 0)
        {
            request.setHeader(QNetworkRequest::ContentTypeHeader, it.value());
            hasContentType = true;
        }
        else
        {
            request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }
    }

    if (!hasContentType)
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError)
        data = reply->readAll();
    else if (errorMessage)
        *errorMessage = reply->errorString();

    reply->deleteLater();
    return data;
}

void MainWindow::on_syncMaterialsButton_clicked()
{
    if (!ui->syncMaterialsButton)
        return;

    ui->syncMaterialsButton->setEnabled(false);

    if (ui->materialsStatusLabel)
        ui->materialsStatusLabel->setText(tr("正在同步材料数据..."));

    QApplication::setOverrideCursor(Qt::BusyCursor);
    QString error;
    QVector<MaterialRecord> fetched = fetchMaterialsFromRemote(&error);
    QApplication::restoreOverrideCursor();

    if (!error.isEmpty())
    {
        QString testError;
        QVector<MaterialRecord> testMaterials = loadMaterialsFromTestData(&testError);
        if (!testMaterials.isEmpty())
        {
            appendLogMessage(tr("同步失败，已加载本地测试材料数据：%1").arg(error));
            saveMaterialsToDatabase(testMaterials);
            loadMaterialsFromDatabase();
            if (ui->materialsStatusLabel)
            {
                const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                ui->materialsStatusLabel->setText(tr("网络同步失败，已于 %1 加载 %2 条测试数据")
                                                    .arg(timestamp)
                                                    .arg(m_materials.size()));
            }
            ui->syncMaterialsButton->setEnabled(true);
            return;
        }

        if (ui->materialsStatusLabel)
            ui->materialsStatusLabel->setText(tr("同步失败：%1").arg(error));
        if (!testError.isEmpty())
            error = tr("同步失败：%1\n测试数据加载失败：%2").arg(error, testError);
        QMessageBox::warning(this, tr("同步材料数据"), error);
        ui->syncMaterialsButton->setEnabled(true);
        return;
    }

    saveMaterialsToDatabase(fetched);
    loadMaterialsFromDatabase();

    if (ui->materialsStatusLabel)
    {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        ui->materialsStatusLabel->setText(tr("已于 %1 同步 %2 条材料数据").arg(timestamp).arg(m_materials.size()));
    }

    ui->syncMaterialsButton->setEnabled(true);
    appendLogMessage(tr("成功同步 %1 条材料数据。").arg(fetched.size()));
}

void MainWindow::on_materialsListWidget_currentItemChanged(QListWidgetItem* current,
                                                           QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    const QString key = current ? current->data(Qt::UserRole).toString() : QString();
    displayMaterialDetails(materialByKey(key));
}

void MainWindow::loadSchemeLibrary()
{
    m_librarySchemes.clear();

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir baseDir(appDir);
    const QString defaultRoot = baseDir.filePath(QStringLiteral("scheme_library"));
    QDir root(defaultRoot);
    if (!root.exists())
        root.mkpath(QStringLiteral("."));

    m_schemeLibraryRoot = canonicalPathForDir(root);
    if (m_schemeLibraryRoot.isEmpty())
        m_schemeLibraryRoot = QDir::cleanPath(defaultRoot);

    QSet<QString> seenPaths;
    QSet<QString> seenIds;
    const auto addLibraryEntry = [&](SchemeLibraryEntry entry) {
        if (entry.directory.isEmpty())
            return;

        const QString pathKey = entry.directory.toCaseFolded();
        if (seenPaths.contains(pathKey))
            return;

        QString idKey = entry.id.trimmed();
        entry.id = idKey;
        if (idKey.isEmpty() || seenIds.contains(idKey.toCaseFolded()))
        {
            idKey = QUuid::createUuid().toString(QUuid::WithoutBraces);
            entry.id = idKey;
        }

        seenPaths.insert(pathKey);
        seenIds.insert(idKey.toCaseFolded());
        m_librarySchemes.push_back(entry);
    };
    QDir libraryRoot(m_schemeLibraryRoot);

    const QString indexFile = libraryRoot.filePath(QStringLiteral("library.json"));
    QFile file(indexFile);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QByteArray data = file.readAll();
        file.close();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
        {
            const QJsonArray arr = doc.object().value(QStringLiteral("schemes")).toArray();
            for (const QJsonValue& value : arr)
            {
                const QJsonObject obj = value.toObject();
                const QString id = obj.value(QStringLiteral("id")).toString();
                QString name = obj.value(QStringLiteral("name")).toString();
                const QString relDir = obj.value(QStringLiteral("directory")).toString();
                if (relDir.trimmed().isEmpty())
                    continue;
                const QString absoluteDir = libraryRoot.filePath(relDir);
                const QString canonical = canonicalPathForDir(QDir(absoluteDir));
                if (canonical.isEmpty())
                    continue;
                if (!QDir(canonical).exists())
                    continue;

                SchemeLibraryEntry entry;
                entry.id = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
                entry.name = name.isEmpty() ? QDir(canonical).dirName() : name;
                entry.directory = canonical;
                entry.deletable = true;

                const QString thumbRel = obj.value(QStringLiteral("thumbnail")).toString().trimmed();
                if (!thumbRel.isEmpty())
                {
                    const QString thumbPath = QDir(canonical).filePath(thumbRel);
                    if (QFileInfo::exists(thumbPath))
                        entry.thumbnailPath = QDir::cleanPath(QFileInfo(thumbPath).absoluteFilePath());
                }
                if (entry.thumbnailPath.isEmpty())
                {
                    QDir dir(canonical);
                    const QStringList covers = dir.entryList(QStringList() << QStringLiteral("scheme_cover.*"),
                                                             QDir::Files | QDir::NoDotAndDotDot);
                    if (!covers.isEmpty())
                        entry.thumbnailPath = QDir::cleanPath(dir.filePath(covers.first()));
                }
                addLibraryEntry(entry);
            }
        }
    }

    const QStringList builtinRoots = {
        QDir::current().absoluteFilePath(QStringLiteral("sample_data")),
        QCoreApplication::applicationDirPath() + QStringLiteral("/sample_data"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../sample_data")
    };
    for (const QString& rootPath : builtinRoots)
    {
        QDir rootDir(rootPath);
        if (!rootDir.exists())
            continue;
        const QFileInfoList dirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& info : dirs)
        {
            const QString canonical = canonicalPathForDir(QDir(info.absoluteFilePath()));
            if (canonical.isEmpty())
                continue;

            SchemeLibraryEntry entry;
            entry.id = QString::fromLatin1(QCryptographicHash::hash(canonical.toUtf8(),
                                                                    QCryptographicHash::Md5).toHex());
            entry.name = info.fileName();
            entry.directory = canonical;
            entry.deletable = false;

            QDir dir(canonical);
            const QStringList covers = dir.entryList(QStringList() << QStringLiteral("scheme_cover.*"),
                                                     QDir::Files | QDir::NoDotAndDotDot);
            if (!covers.isEmpty())
                entry.thumbnailPath = QDir::cleanPath(dir.filePath(covers.first()));

            addLibraryEntry(entry);
        }
    }

    std::sort(m_librarySchemes.begin(), m_librarySchemes.end(), [](const SchemeLibraryEntry& a,
                                                                   const SchemeLibraryEntry& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}

void MainWindow::saveSchemeLibrary() const
{
    if (m_schemeLibraryRoot.isEmpty())
        return;

    QDir root(m_schemeLibraryRoot);
    if (!root.exists())
        root.mkpath(QStringLiteral("."));

    QJsonArray array;
    for (const SchemeLibraryEntry& entry : m_librarySchemes)
    {
        if (!entry.deletable)
            continue;
        if (!isPathWithinDirectory(entry.directory, m_schemeLibraryRoot))
            continue;

        const QString relativeDir = root.relativeFilePath(entry.directory);
        if (relativeDir.startsWith(QStringLiteral("..")))
            continue;

        QJsonObject obj;
        obj.insert(QStringLiteral("id"), entry.id);
        obj.insert(QStringLiteral("name"), entry.name);
        obj.insert(QStringLiteral("directory"), relativeDir);

        if (!entry.thumbnailPath.isEmpty())
        {
            QDir entryDir(entry.directory);
            const QString relThumb = entryDir.relativeFilePath(entry.thumbnailPath);
            if (!relThumb.startsWith(QStringLiteral("..")))
                obj.insert(QStringLiteral("thumbnail"), relThumb);
        }

        array.append(obj);
    }

    QJsonObject rootObj;
    rootObj.insert(QStringLiteral("schemes"), array);

    QFile file(root.filePath(QStringLiteral("library.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QJsonDocument doc(rootObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::loadInitialSchemes()
{
    loadApplicationState();
}

void MainWindow::loadApplicationState()
{
    QString lastProject;
    QStringList recentProjects;
    QFile file(m_appStateFilePath);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QByteArray data = file.readAll();
        file.close();

        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
        {
            const QJsonObject obj = doc.object();
            lastProject = obj.value(QStringLiteral("lastProject")).toString().trimmed();
            const QJsonArray recentArray = obj.value(QStringLiteral("recentProjects")).toArray();
            for (const QJsonValue& value : recentArray)
            {
                const QString path = value.toString().trimmed();
                if (!path.isEmpty() && !recentProjects.contains(path))
                    recentProjects.append(path);
            }
        }
    }

    m_recentProjects = recentProjects;

    if (!lastProject.isEmpty() && openProjectAt(lastProject, /*silent*/true))
        return;

    for (const QString& candidate : std::as_const(m_recentProjects))
    {
        if (openProjectAt(candidate, /*silent*/true))
            return;
    }

    enterProjectlessState();
}

void MainWindow::saveApplicationState() const
{
    if (m_appStateFilePath.isEmpty())
        return;

    QFileInfo info(m_appStateFilePath);
    QDir dir = info.dir();
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    QJsonObject root;
    root.insert(QStringLiteral("lastProject"), m_projectRoot);
    QJsonArray recentArray;
    for (const QString& path : m_recentProjects)
        recentArray.append(path);
    root.insert(QStringLiteral("recentProjects"), recentArray);

    QFile file(m_appStateFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::updateRecentProjects(const QString& canonicalPath)
{
    const QString trimmed = canonicalPath.trimmed();
    if (trimmed.isEmpty())
        return;

    int foundIndex = -1;
    for (int i = 0; i < m_recentProjects.size(); ++i)
    {
        const QString existing = m_recentProjects.at(i);
        if (existing == trimmed)
        {
            foundIndex = i;
            break;
        }
        const QString existingCanonical = canonicalPathForDir(QDir(existing));
        if (!existingCanonical.isEmpty() && existingCanonical == trimmed)
        {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex >= 0)
    {
        if (m_recentProjects.at(foundIndex) != trimmed)
            m_recentProjects[foundIndex] = trimmed;
        for (int i = m_recentProjects.size() - 1; i >= 0; --i)
        {
            if (i == foundIndex)
                continue;
            const QString existing = m_recentProjects.at(i);
            if (existing == trimmed)
            {
                m_recentProjects.removeAt(i);
                continue;
            }
            const QString existingCanonical = canonicalPathForDir(QDir(existing));
            if (!existingCanonical.isEmpty() && existingCanonical == trimmed)
                m_recentProjects.removeAt(i);
        }
    }
    else
    {
        m_recentProjects.append(trimmed);
    }

    const int maxEntries = 8;
    while (m_recentProjects.size() > maxEntries)
        m_recentProjects.removeFirst();
}

bool MainWindow::removeProjectFromRecents(const QString& projectPath)
{
    const QString trimmed = projectPath.trimmed();
    if (trimmed.isEmpty())
        return false;

    const QString canonical = canonicalPathForDir(QDir(trimmed));
    bool removed = false;

    for (int i = m_recentProjects.size() - 1; i >= 0; --i)
    {
        const QString existing = m_recentProjects.at(i);
        if (existing.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            m_recentProjects.removeAt(i);
            removed = true;
            continue;
        }

        if (!canonical.isEmpty())
        {
            const QString existingCanonical = canonicalPathForDir(QDir(existing));
            if (!existingCanonical.isEmpty() &&
                existingCanonical.compare(canonical, Qt::CaseInsensitive) == 0)
            {
                m_recentProjects.removeAt(i);
                removed = true;
            }
        }
    }

    return removed;
}

void MainWindow::closeProject(const QString& projectPath)
{
    const QString trimmed = projectPath.trimmed();
    if (trimmed.isEmpty())
        return;

    const QString canonical = canonicalPathForDir(QDir(trimmed));
    const QString targetPath = canonical.isEmpty() ? QDir::cleanPath(trimmed) : canonical;

    const bool wasActive = (!canonical.isEmpty() && canonical == m_projectRoot) ||
                           (canonical.isEmpty() && targetPath == m_projectRoot);

    const bool removed = removeProjectFromRecents(targetPath);

    if (wasActive)
    {
        enterProjectlessState();
        appendLogMessage(tr("已关闭工程：%1")
                             .arg(QDir::toNativeSeparators(targetPath)));
        return;
    }

    if (removed)
    {
        refreshNavigation();
        saveApplicationState();
        appendLogMessage(tr("已关闭工程：%1")
                             .arg(QDir::toNativeSeparators(targetPath)));
    }
}

void MainWindow::enterProjectlessState()
{
    m_projectRoot.clear();
    m_workspaceRoot.clear();
    m_storageFilePath.clear();
    m_projectRemarks.clear();
    m_projectCreatedAt = QDateTime();
    m_projectUpdatedAt = QDateTime();
    m_activeSchemeId.clear();
    m_activeModelId.clear();
    m_schemes.clear();
    m_schemeItems.clear();
    m_modelItems.clear();
    m_projectRootItem = nullptr;
    m_libraryRootItem = nullptr;

    clearDetailWidget();
    clearVtkScene();
    setVisualizationVisible(false);
    updateModelImagePreview(nullptr);
    updateSelectionInfo();

    if (ui->stackedWidget && ui->welcomePage)
        ui->stackedWidget->setCurrentWidget(ui->welcomePage);

    refreshNavigation();
    updateWindowTitle();
    updateToolbarState();
    saveApplicationState();
}

bool MainWindow::openProjectAt(const QString& path, bool silent)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return false;

    if (!ensureProjectStructure(trimmed))
    {
        if (!silent)
            QMessageBox::warning(this, tr("打开工程"),
                                 tr("无法创建或访问工程目录：%1")
                                     .arg(QDir::toNativeSeparators(trimmed)));
        return false;
    }

    QDir projectDir(trimmed);
    const QString canonicalProject = canonicalPathForDir(projectDir);
    if (canonicalProject.isEmpty())
    {
        if (!silent)
            QMessageBox::warning(this, tr("打开工程"),
                                 tr("无法解析工程路径：%1")
                                     .arg(QDir::toNativeSeparators(trimmed)));
        return false;
    }

    updateRecentProjects(canonicalProject);

    if (canonicalProject == m_projectRoot)
    {
        refreshNavigation();
        ui->stackedWidget->setCurrentWidget(ui->planPage);
        updateToolbarState();
        updateWindowTitle();
        saveApplicationState();
        return true;
    }

    m_projectRoot = canonicalProject;

    QDir canonicalDir(m_projectRoot);
    const QString workspacePath = canonicalDir.filePath(QStringLiteral("workspaces"));
    ensureDirectoryExists(workspacePath);
    m_workspaceRoot = canonicalPathForDir(QDir(workspacePath));
    if (m_workspaceRoot.isEmpty())
        m_workspaceRoot = QDir::cleanPath(workspacePath);

    m_storageFilePath = canonicalDir.filePath(QStringLiteral("schemes.json"));

    m_projectRemarks.clear();
    m_projectCreatedAt = QDateTime();
    m_projectUpdatedAt = QDateTime();

    m_schemes.clear();
    if (!loadSchemesFromStorage())
    {
        m_schemes.clear();
        const QDateTime now = QDateTime::currentDateTimeUtc();
        m_projectCreatedAt = now;
        m_projectUpdatedAt = now;
        persistSchemes();
    }

    refreshNavigation();
    if (ui->stackedWidget)
        ui->stackedWidget->setCurrentWidget(ui->planPage);
    updateToolbarState();
    updateWindowTitle();

    if (!silent)
        appendLogMessage(tr("已打开工程：%1")
                             .arg(QDir::toNativeSeparators(m_projectRoot)));
    saveApplicationState();
    return true;
}

bool MainWindow::ensureProjectStructure(const QString& rootPath)
{
    if (rootPath.isEmpty())
        return false;

    QDir root(rootPath);
    const QString absolute = root.absolutePath();
    if (!QDir(absolute).exists())
    {
        QDir dir;
        if (!dir.mkpath(absolute))
            return false;
    }

    QDir absoluteDir(absolute);
    if (!ensureDirectoryExists(absoluteDir.filePath(QStringLiteral("workspaces"))))
        return false;

    return true;
}

void MainWindow::updateWindowTitle()
{

}

void MainWindow::onNewProjectTriggered()
{
    // 读取默认路径
    QSettings settings("./setting.ini", QSettings::IniFormat);
    QString defaultPath = settings.value("defaultProjectPath", QDir::homePath()).toString();

    // === 创建自定义输入对话框 ===
    QDialog dialog(this);
    dialog.setWindowTitle(tr("新建工程"));

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 工程名称
    QLabel *nameLabel = new QLabel(tr("工程名称："));
    QLineEdit *nameEdit = new QLineEdit(tr("NewProject"));
    mainLayout->addWidget(nameLabel);
    mainLayout->addWidget(nameEdit);

    // 工作路径
    QLabel *pathLabel = new QLabel(tr("工作路径："));
    QLineEdit *pathEdit = new QLineEdit(defaultPath);
    QPushButton *browseButton = new QPushButton(tr("选择..."));

    QHBoxLayout *pathLayout = new QHBoxLayout;
    pathLayout->addWidget(pathEdit);
    pathLayout->addWidget(browseButton);

    mainLayout->addWidget(pathLabel);
    mainLayout->addLayout(pathLayout);

    // 选择路径按钮逻辑
    connect(browseButton, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(
            &dialog, tr("选择工程位置"), pathEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty())
            pathEdit->setText(dir);
    });

    // 确认/取消按钮
    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 显示对话框
    if (dialog.exec() != QDialog::Accepted)
        return;

    // 获取用户输入
    const QString trimmedName = nameEdit->text().trimmed();
    const QString baseDir = pathEdit->text().trimmed();

    // === 校验 ===
    if (trimmedName.isEmpty())
    {
        QMessageBox::warning(this, tr("新建工程"), tr("工程名称不能为空。"));
        return;
    }

    if (baseDir.isEmpty())
    {
        QMessageBox::warning(this, tr("新建工程"), tr("工作路径不能为空。"));
        return;
    }

    // 保存新的默认路径
    settings.setValue("defaultProjectPath", baseDir);

    // 检查目录
    QDir base(baseDir);
    const QString projectPath = base.filePath(trimmedName);
    QDir projectDir(projectPath);
    if (projectDir.exists())
    {
        const QStringList contents =
            projectDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (!contents.isEmpty())
        {
            QMessageBox::warning(this, tr("新建工程"),
                                 tr("选定的工程目录已存在且非空，请选择其它位置。"));
            return;
        }
    }

    // 创建工程目录
    if (!ensureProjectStructure(projectPath))
    {
        QMessageBox::warning(this, tr("新建工程"),
                             tr("无法创建工程目录：%1")
                                 .arg(QDir::toNativeSeparators(projectPath)));
        return;
    }

    // 打开工程
    if (openProjectAt(projectPath, /*silent*/false))
        appendLogMessage(tr("已创建工程 %1").arg(trimmedName));
}
void MainWindow::onOpenProjectTriggered()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("打开工程"), QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty())
        return;

    openProjectAt(dir, /*silent*/false);
}

void MainWindow::onAddLibraryScheme()
{
    const QString defaultName = tr("NewAssembly%1").arg(m_librarySchemes.size() + 1);
    SchemeSettingsDialog dlg(defaultName, QString(), false, this);
    dlg.setDirectoryHint(tr("总成库目录将在软件运行目录中自动生成"));
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = dlg.schemeName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("创建总成库"), tr("总成名称不能为空"));
        return;
    }

    QString directory = makeUniqueLibrarySubdir(name);
    if (directory.isEmpty())
    {
        QMessageBox::warning(this, tr("创建总成库"), tr("无法创建总成库目录"));
        return;
    }

    if (!ensureDirectoryExists(directory))
    {
        QMessageBox::warning(this, tr("创建总成库"),
                             tr("无法创建目录：%1")
                                 .arg(QDir::toNativeSeparators(directory)));
        return;
    }

    const QString canonical = canonicalPathForDir(QDir(directory));
    if (canonical.isEmpty())
    {
        QDir(directory).removeRecursively();
        QMessageBox::warning(this, tr("创建总成库"), tr("无法解析总成库目录"));
        return;
    }

    SchemeLibraryEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.name = name;
    entry.directory = canonical;
    entry.deletable = true;

    applyLibraryThumbnail(entry, dlg.thumbnailPath());
    if (entry.thumbnailPath.isEmpty())
    {
        QDir dir(canonical);
        const QStringList covers = dir.entryList(QStringList() << QStringLiteral("scheme_cover.*"),
                                                 QDir::Files | QDir::NoDotAndDotDot);
        if (!covers.isEmpty())
            entry.thumbnailPath = QDir::cleanPath(dir.filePath(covers.first()));
    }

    m_librarySchemes.push_back(entry);
    std::sort(m_librarySchemes.begin(), m_librarySchemes.end(), [](const SchemeLibraryEntry& a,
                                                                   const SchemeLibraryEntry& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    saveSchemeLibrary();
    updateGallery();
    appendLogMessage(tr("已创建总成库 %1").arg(name));
}

void MainWindow::on_selectModelButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择模型文件"),
        QDir::homePath(),
        tr("OBJ 文件 (*.obj);;所有文件 (*.*)")
    );

    if (filePath.isEmpty())
        return;

    ui->modelPathLineEdit->setText(filePath);
}

void MainWindow::on_loadModelButton_clicked()
{
    QString modelPath = ui->modelPathLineEdit->text();
    displayResultFile(modelPath);
}

void MainWindow::handleTreeSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem*)
{
    if (!current)
    {
        m_activeSchemeId.clear();
        m_activeModelId.clear();
        clearDetailWidget();
        clearVtkScene();
        setVisualizationVisible(false);
        updateModelImagePreview(nullptr);
        updateSelectionInfo();
        updateToolbarState();
        return;
    }

    const int type = current->data(0, TypeRole).toInt();

    if (type == LibraryItem)
    {
        m_activeSchemeId.clear();
        m_activeModelId.clear();
        clearDetailWidget();
        clearVtkScene();
        setVisualizationVisible(false);
        updateModelImagePreview(nullptr);
        if (ui->stackedWidget)
            ui->stackedWidget->setCurrentWidget(ui->planPage);
        updateGallery();
        updateSelectionInfo();
    }
    else if (type == SchemeItem)
    {
        const QString schemeId = current->data(0, IdRole).toString();
        m_activeSchemeId = schemeId;
        m_activeModelId.clear();
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        clearVtkScene();
        setVisualizationVisible(false);
        updateModelImagePreview(nullptr);

        SchemeRecord* scheme = schemeById(schemeId);
        if (scheme && !scheme->libraryId.isEmpty() &&
            libraryEntryById(scheme->libraryId))
        {
            showLibrarySchemeDetail(scheme->libraryId, schemeId);
        }
        else
        {
            showSchemeSettings(schemeId);
            if (scheme)
                updateSelectionInfo(scheme->workingDirectory, scheme->remarks);
            else
                updateSelectionInfo();
        }
    }
    else if (type == ModelItem)
    {
        const QString modelId = current->data(0, IdRole).toString();
        const QString schemeId = current->data(0, SchemeRole).toString();
        m_activeSchemeId = schemeId;
        m_activeModelId = modelId;
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        showModelSettings(modelId);
        setVisualizationVisible(true);

        SchemeRecord* owner = nullptr;
        if (ModelRecord* model = modelById(modelId, &owner))
            updateSelectionInfo(model->directory, model->remarks);
        else
            updateSelectionInfo();
    }
    else if (type == ProjectItem)
    {
        const bool isActiveProject = current->data(0, ActiveRole).toBool();
        if (!isActiveProject)
        {
            clearDetailWidget();
            clearVtkScene();
            setVisualizationVisible(false);
            if (ui->stackedWidget)
                ui->stackedWidget->setCurrentWidget(ui->planPage);
            updateSelectionInfo();
            return;
        }

        const QString projectPath = current->data(0, IdRole).toString();
        const QString canonical = canonicalPathForDir(QDir(projectPath));
        if (canonical != m_projectRoot)
        {
            openProjectAt(projectPath, /*silent*/false);
            return;
        }

        m_activeSchemeId.clear();
        m_activeModelId.clear();
        if (ui->stackedWidget)
            ui->stackedWidget->setCurrentWidget(ui->MainPage);
        showProjectInfo();
    }
    else
    {
        updateSelectionInfo();
    }
    updateToolbarState();
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int)
{
    if (!item)
        return;

    if (item->data(0, TypeRole).toInt() != LibraryItem)
        return;

    if (!m_currentDetailWidget ||
        m_currentDetailWidget->objectName() != QLatin1String("librarySchemeDetail"))
    {
        return;
    }

    handleTreeSelectionChanged(item, nullptr);
}

void MainWindow::onTreeItemDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item)
        return;

    if (item->data(0, TypeRole).toInt() != ProjectItem)
        return;

    if (item->data(0, ActiveRole).toBool())
        return;

    const QString projectPath = item->data(0, IdRole).toString();
    if (!projectPath.isEmpty())
        openProjectAt(projectPath, /*silent*/false);
}

void MainWindow::onTreeItemChanged(QTreeWidgetItem* item, int column)
{
    if (!item || column != 0)
        return;
    if (m_blockTreeSignals)
        return;

    const int type = item->data(0, TypeRole).toInt();
    if (type == ProjectItem || type == LibraryItem)
        return;

    const QString id = item->data(0, IdRole).toString();

    auto restoreText = [this, item](const QString& text) {
        QScopedValueRollback<bool> guard(m_blockTreeSignals, true);
        item->setText(0, text);
    };

    if (type == SchemeItem)
    {
        if (SchemeRecord* scheme = schemeById(id))
        {
            const QString originalName = scheme->name;
            const QString trimmed = item->text(0).trimmed();
            if (trimmed.isEmpty())
            {
                QMessageBox::warning(this, tr("重命名总成"), tr("总成名称不能为空。"));
                restoreText(scheme->name);
                return;
            }

            const QString unique = makeUniqueSchemeName(trimmed, scheme->id);
            if (unique.compare(trimmed, Qt::CaseSensitive) != 0)
            {
                QMessageBox::warning(this, tr("重命名总成"), tr("已存在同名总成，请输入其他名称。"));
                restoreText(scheme->name);
                return;
            }

            if (item->text(0) != trimmed)
            {
                QScopedValueRollback<bool> guard(m_blockTreeSignals, true);
                item->setText(0, trimmed);
            }

            bool libraryUpdated = false;
            if (!scheme->libraryId.trimmed().isEmpty())
            {
                if (SchemeLibraryEntry* entry = libraryEntryById(scheme->libraryId))
                {
                    if (entry->name != trimmed)
                    {
                        entry->name = trimmed;
                        libraryUpdated = true;
                    }
                }
            }

            if (originalName == trimmed)
            {
                if (libraryUpdated)
                {
                    saveSchemeLibrary();
                    updateGallery();
                }
                refreshCurrentDetail();
                return;
            }

            scheme->name = trimmed;
            persistSchemes();
            if (libraryUpdated)
                saveSchemeLibrary();
            refreshCurrentDetail();
            refreshNavigation(scheme->id);
            return;
        }
    }
    else if (type == ModelItem)
    {
        SchemeRecord* owner = nullptr;
        if (ModelRecord* model = modelById(id, &owner))
        {
            const QString originalName = model->name;
            const QString trimmed = item->text(0).trimmed();
            if (trimmed.isEmpty())
            {
                QMessageBox::warning(this, tr("重命名模型"), tr("模型名称不能为空。"));
                restoreText(model->name);
                return;
            }

            const QString unique = makeUniqueModelName(*owner, trimmed, model->id);
            if (unique.compare(trimmed, Qt::CaseSensitive) != 0)
            {
                QMessageBox::warning(this, tr("重命名模型"), tr("该总成下已存在同名模型。"));
                restoreText(model->name);
                return;
            }

            if (item->text(0) != trimmed)
            {
                QScopedValueRollback<bool> guard(m_blockTreeSignals, true);
                item->setText(0, trimmed);
            }

            if (originalName == trimmed)
            {
                refreshCurrentDetail();
                return;
            }

            model->name = trimmed;
            persistSchemes();
            refreshCurrentDetail();
            if (owner)
                refreshNavigation(owner->id, model->id);
            else
                refreshNavigation(QString(), model->id);
            return;
        }
    }
}

void MainWindow::onTreeContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->treeModels->itemAt(pos);
    QMenu menu(this);

    if (!item)
    {
        menu.addAction(tr("导入总成"), this, &MainWindow::promptAddScheme);
    }
    else
    {
        const int type = item->data(0, TypeRole).toInt();
        if (type == LibraryItem)
        {
            menu.addAction(tr("查看总成库"), this, [this]() {
                if (ui->stackedWidget)
                    ui->stackedWidget->setCurrentWidget(ui->planPage);
                updateGallery();
            });
            if (hasActiveProject())
            {
                menu.addSeparator();
                menu.addAction(tr("导入总成"), this, &MainWindow::promptAddScheme);
            }
        }
        else if (type == ProjectItem)
        {
            const QString projectPath = item->data(0, IdRole).toString();
            if (!projectPath.isEmpty())
            {
                menu.addAction(tr("打开工程目录"), this, [projectPath]() {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(projectPath));
                });
            }

            const QString canonical = canonicalPathForDir(QDir(projectPath));
            if (!canonical.isEmpty() && canonical == m_projectRoot)
            {
                menu.addAction(tr("导入总成"), this, &MainWindow::promptAddScheme);
            }
            else
            {
                menu.addAction(tr("激活此工程"), this, [this, projectPath]() {
                    openProjectAt(projectPath, /*silent*/false);
                });
            }

            if (!projectPath.isEmpty())
            {
                menu.addSeparator();
                menu.addAction(tr("关闭工程"), this, [this, projectPath]() {
                    closeProject(projectPath);
                });
            }
        }
        else if (type == SchemeItem)
        {
            if (!item->data(0, ActiveRole).toBool())
            {
                QAction* info = menu.addAction(tr("请激活工程后再操作此总成"));
                info->setEnabled(false);
                if (QTreeWidgetItem* projectItem = item->parent())
                {
                    const QString projectPath = projectItem->data(0, IdRole).toString();
                    if (!projectPath.isEmpty())
                    {
                        menu.addSeparator();
                        menu.addAction(tr("激活此工程"), this, [this, projectPath]() {
                            openProjectAt(projectPath, /*silent*/false);
                        });
                    }
                }
            }
            else
            {
                const QString schemeId = item->data(0, IdRole).toString();
                menu.addAction(tr("总成设置"), this, [this, schemeId]() {
                    openSchemeSettings(schemeId);
                });
                menu.addAction(tr("重命名"), this, [this, item]() {
                    if (ui->treeModels)
                        ui->treeModels->editItem(item, 0);
                });
                menu.addAction(tr("添加模型"), this, [this, schemeId]() {
                    promptAddModel(schemeId);
                });
                menu.addAction(tr("打开总成目录"), this, [this, schemeId]() {
                    if (SchemeRecord* scheme = schemeById(schemeId))
                        QDesktopServices::openUrl(QUrl::fromLocalFile(scheme->workingDirectory));
                });
                menu.addSeparator();
                menu.addAction(tr("移除总成"), this, [this, schemeId]() {
                    if (SchemeRecord* scheme = schemeById(schemeId))
                    {
                        if (confirmSchemeDeletion(*scheme))
                            removeSchemeById(schemeId);
                    }
                });
            }
        }
        else if (type == ModelItem)
        {
            if (!item->data(0, ActiveRole).toBool())
            {
                QAction* info = menu.addAction(tr("请激活工程后再操作此模型"));
                info->setEnabled(false);
                QTreeWidgetItem* schemeItem = item->parent();
                QTreeWidgetItem* projectItem = schemeItem ? schemeItem->parent() : nullptr;
                if (projectItem)
                {
                    const QString projectPath = projectItem->data(0, IdRole).toString();
                    if (!projectPath.isEmpty())
                    {
                        menu.addSeparator();
                        menu.addAction(tr("激活此工程"), this, [this, projectPath]() {
                            openProjectAt(projectPath, /*silent*/false);
                        });
                    }
                }
            }
            else
            {
                const QString modelId = item->data(0, IdRole).toString();
                menu.addAction(tr("打开模型目录"), this, [this, modelId]() {
                    SchemeRecord* owner = nullptr;
                    if (ModelRecord* model = modelById(modelId, &owner))
                        QDesktopServices::openUrl(QUrl::fromLocalFile(model->directory));
                });
                menu.addAction(tr("设置模型图片"), this, [this, modelId]() {
                    SchemeRecord* owner = nullptr;
                    if (ModelRecord* model = modelById(modelId, &owner))
                    {
                        const QString initialDir = model->thumbnailPath.isEmpty()
                                                       ? model->directory
                                                       : QFileInfo(model->thumbnailPath).absolutePath();
                        const QString file = QFileDialog::getOpenFileName(
                            this, tr("选择模型图片"), initialDir,
                            tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"));
                        if (file.isEmpty())
                            return;
                        applyModelThumbnail(*model, file);
                        m_lastModelImageDir = QFileInfo(file).absolutePath();
                        persistSchemes();
                        refreshCurrentDetail();
                    }
                });
                menu.addAction(tr("清除模型图片"), this, [this, modelId]() {
                    SchemeRecord* owner = nullptr;
                    if (ModelRecord* model = modelById(modelId, &owner))
                    {
                        if (model->thumbnailPath.isEmpty())
                            return;
                        applyModelThumbnail(*model, QString());
                        persistSchemes();
                        refreshCurrentDetail();
                    }
                });
                menu.addAction(tr("重命名"), this, [this, item]() {
                    if (ui->treeModels)
                        ui->treeModels->editItem(item, 0);
                });
                menu.addSeparator();
                menu.addAction(tr("移除模型"), this, [this, modelId]() {
                    SchemeRecord* owner = nullptr;
                    if (ModelRecord* model = modelById(modelId, &owner))
                    {
                        if (owner && confirmModelDeletion(*model, *owner))
                            removeModelById(modelId);
                    }
                });
            }
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
        else if (type == ProjectItem)
            targetSchemeId.clear();
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

void MainWindow::onGalleryOpenRequested(const QString& id)
{
    if (const SchemeLibraryEntry* entry = libraryEntryById(id))
    {
        if (!entry->directory.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(entry->directory));
        return;
    }

    if (schemeById(id))
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        selectTreeItem(id, QString());
    }
}

void MainWindow::onGalleryDeleteRequested(const QString& id)
{
    if (SchemeLibraryEntry* entry = libraryEntryById(id))
    {
        if (!entry->deletable)
        {
            QMessageBox::information(this, tr("删除总成库"), tr("此总成属于内置模板，无法删除。"));
            return;
        }
        const QString entryName = entry->name.isEmpty() ? tr("Untitled Assembly") : entry->name;
        const QString text = tr("确定要从总成库中删除“%1”吗？").arg(entryName);
        if (QMessageBox::question(this, tr("删除总成库"), text,
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) == QMessageBox::Yes)
        {
            if (removeLibraryEntry(id))
            {
                updateGallery();
                appendLogMessage(tr("已删除总成库 %1").arg(entryName));
            }
        }
        return;
    }

    if (SchemeRecord* scheme = schemeById(id))
    {
        if (confirmSchemeDeletion(*scheme))
            removeSchemeById(id);
    }
}

void MainWindow::onGalleryDetailsRequested(const QString& id)
{
    if (libraryEntryById(id))
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        showLibrarySchemeDetail(id);
        return;
    }

    if (schemeById(id))
    {
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        selectTreeItem(id, QString());
    }
}

void MainWindow::deleteCurrentTreeItem()
{
    QTreeWidgetItem* item = ui->treeModels->currentItem();
    if (!item)
        return;

    const int type = item->data(0, TypeRole).toInt();
    if (type == ProjectItem || type == LibraryItem)
        return;
    const QString id = item->data(0, IdRole).toString();

    if (type == SchemeItem)
    {
        if (SchemeRecord* scheme = schemeById(id))
        {
            if (confirmSchemeDeletion(*scheme))
                removeSchemeById(id);
        }
    }
    else if (type == ModelItem)
    {
        SchemeRecord* owner = nullptr;
        if (ModelRecord* model = modelById(id, &owner))
        {
            if (owner && confirmModelDeletion(*model, *owner))
                removeModelById(id);
        }
    }
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
    m_projectRootItem = nullptr;
    m_libraryRootItem = nullptr;

    const QIcon projectIcon(QStringLiteral(":/icons/icons/project_logo.svg"));
    const QIcon libraryIcon(QStringLiteral(":/icons/icons/gallery.svg"));
    const QIcon schemeIcon(QStringLiteral(":/icons/icons/plan.svg"));
    const QIcon modelIcon(QStringLiteral(":/icons/icons/model.svg"));

    const QBrush inactiveBrush(QColor(148, 163, 184));

    m_libraryRootItem = new QTreeWidgetItem();
    m_libraryRootItem->setText(0, tr("总成库"));
    m_libraryRootItem->setIcon(0, libraryIcon);
    m_libraryRootItem->setData(0, TypeRole, LibraryItem);
    m_libraryRootItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->treeModels->insertTopLevelItem(0, m_libraryRootItem);

    for (const QString& projectPath : m_recentProjects)
    {
        const QString trimmed = projectPath.trimmed();
        if (trimmed.isEmpty())
            continue;

        auto* projectItem = new QTreeWidgetItem(ui->treeModels);
        projectItem->setIcon(0, projectIcon);
        projectItem->setData(0, TypeRole, ProjectItem);
        projectItem->setData(0, IdRole, trimmed);

        bool isActive = false;
        if (hasActiveProject())
        {
            const QString canonical = canonicalPathForDir(QDir(trimmed));
            isActive = !canonical.isEmpty() && canonical == m_projectRoot;
        }

        projectItem->setData(0, ActiveRole, isActive);

        QString displayName = projectDisplayName(trimmed);
        projectItem->setText(0, displayName);

        Qt::ItemFlags projectFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (isActive)
            projectFlags |= Qt::ItemIsDropEnabled;
        projectItem->setFlags(projectFlags);

        if (!isActive)
            projectItem->setForeground(0, inactiveBrush);

        QVector<SchemeRecord> previewSchemes;
        const QVector<SchemeRecord>* schemesForTree = nullptr;
        if (isActive)
        {
            m_projectRootItem = projectItem;
            schemesForTree = &m_schemes;
        }
        else
        {
            previewSchemes = loadProjectPreviewSchemes(trimmed);
            schemesForTree = &previewSchemes;
        }

        if (schemesForTree)
        {
            for (const SchemeRecord& scheme : *schemesForTree)
            {
                auto* schemeItem = new QTreeWidgetItem(projectItem);
                schemeItem->setText(0, scheme.name);
                schemeItem->setIcon(0, schemeIcon);
                schemeItem->setData(0, TypeRole, SchemeItem);
                schemeItem->setData(0, IdRole, scheme.id);
                schemeItem->setData(0, ActiveRole, isActive);

                if (isActive)
                {
                    schemeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                                         Qt::ItemIsDragEnabled | Qt::ItemIsEditable |
                                         Qt::ItemIsDropEnabled);
                    m_schemeItems.insert(scheme.id, schemeItem);
                }
                else
                {
                    schemeItem->setFlags(Qt::ItemIsEnabled);
                    schemeItem->setForeground(0, inactiveBrush);
                }

                for (const ModelRecord& model : scheme.models)
                {
                    auto* modelItem = new QTreeWidgetItem(schemeItem);
                    modelItem->setText(0, model.name);
                    modelItem->setIcon(0, modelIcon);
                    modelItem->setData(0, TypeRole, ModelItem);
                    modelItem->setData(0, IdRole, model.id);
                    modelItem->setData(0, SchemeRole, scheme.id);
                    modelItem->setData(0, ActiveRole, isActive);

                    if (isActive)
                    {
                        modelItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                                            Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
                        m_modelItems.insert(model.id, modelItem);
                    }
                    else
                    {
                        modelItem->setFlags(Qt::ItemIsEnabled);
                        modelItem->setForeground(0, inactiveBrush);
                    }
                }

                if (isActive)
                    ui->treeModels->expandItem(schemeItem);
            }
        }

        ui->treeModels->expandItem(projectItem);
    }

    m_blockTreeSignals = false;
}

void MainWindow::updateGallery()
{
    if (!m_galleryWidget)
        return;

    m_galleryWidget->clearSchemes();
    for (const SchemeLibraryEntry& entry : m_librarySchemes)
    {
        QPixmap thumb = loadLibraryThumbnail(entry);
        if (thumb.isNull())
            thumb = makeSchemePlaceholder(entry.name);

        SchemeGalleryWidget::CardOptions options;
        options.showDeleteButton = entry.deletable;
        options.enableDeleteButton = entry.deletable;
        options.deleteToolTip = entry.deletable
                                     ? tr("从总成库中删除此总成")
                                     : tr("内置模板不可删除");
        options.showOpenButton = true;
        options.enableOpenButton = true;
        options.openToolTip = tr("打开总成所在目录");
        options.hintText = tr("双击卡片查看详情");

        m_galleryWidget->addScheme(entry.id, entry.name, thumb, options);
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
    {
        if (m_projectRootItem)
            ui->treeModels->setCurrentItem(m_projectRootItem);
        else if (m_libraryRootItem)
            ui->treeModels->setCurrentItem(m_libraryRootItem);
        else
            ui->treeModels->setCurrentItem(ui->treeModels->topLevelItem(0));
    }
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

void MainWindow::showProjectInfo()
{
    if (!hasActiveProject())
    {
        clearDetailWidget();
        clearVtkScene();
        setVisualizationVisible(false);
        updateModelImagePreview(nullptr);
        updateSelectionInfo();
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildProjectInfoWidget();
    if (auto* layout = ui->settingWidget->layout())
        layout->addWidget(m_currentDetailWidget);
    setVisualizationVisible(false);
    updateModelImagePreview(nullptr);
    clearVtkScene();
    updateSelectionInfo(m_projectRoot, m_projectRemarks);
}

void MainWindow::showSchemeSettings(const QString& schemeId)
{
    SchemeRecord* scheme = schemeById(schemeId);
    if (!scheme)
    {
        clearDetailWidget();
        updateModelImagePreview(nullptr);
        return;
    }

    if (!scheme->libraryId.isEmpty() && libraryEntryById(scheme->libraryId))
    {
        showLibrarySchemeDetail(scheme->libraryId, scheme->id);
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildSchemeSettingsWidget(*scheme);
    ui->settingWidget->layout()->addWidget(m_currentDetailWidget);
    setVisualizationVisible(false);
    updateModelImagePreview(nullptr);
    updateSelectionInfo(scheme->workingDirectory, scheme->remarks);
}

void MainWindow::showModelSettings(const QString& modelId)
{
    SchemeRecord* owner = nullptr;
    const ModelRecord* model = modelById(modelId, &owner);
    if (!model)
    {
        clearDetailWidget();
        clearVtkScene();
        updateModelImagePreview(nullptr);
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildModelSettingsWidget(*model);
    ui->settingWidget->layout()->addWidget(m_currentDetailWidget);
    setVisualizationVisible(true);
    updateModelImagePreview(model);
    updateSelectionInfo(model->directory, model->remarks);

    const QString resultPath = latestResultFile(model->directory);
    if (!resultPath.isEmpty())
    {
        appendLogMessage(tr("加载最近的 OBJ：%1")
                             .arg(QDir::toNativeSeparators(resultPath)));
        displayResultFile(resultPath);
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
    layout->setSpacing(12);

    auto* title = new QLabel(tr("总成：%1").arg(scheme.name), container);
    title->setStyleSheet("font-size:18px;font-weight:600;color:#0f172a;");
    layout->addWidget(title);

    auto* listFrame = new QFrame(container);
    listFrame->setObjectName("modelListFrame");
    listFrame->setStyleSheet(
        "QFrame#modelListFrame{background:#ffffff;border:1px solid #d0d5dd;border-radius:10px;}"
        "QListWidget#modelList{border:none;background:transparent;}"
        "QListWidget#modelList::item{padding:10px;border-radius:8px;}"
        "QListWidget#modelList::item:hover{background:rgba(23,135,255,0.08);}"
        "QListWidget#modelList::item:selected{background:rgba(23,135,255,0.12);}");
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(8);

    auto* listHeader = new QHBoxLayout();
    listHeader->setContentsMargins(0, 0, 0, 0);
    auto* listTitle = new QLabel(tr("模型列表"), listFrame);
    listTitle->setStyleSheet("font-weight:600;color:#1b2b4d;");
    auto* listCount = new QLabel(tr("%1 个模型").arg(scheme.models.size()), listFrame);
    listCount->setStyleSheet("color:#64748b;");
    listHeader->addWidget(listTitle);
    listHeader->addStretch();
    listHeader->addWidget(listCount);
    listLayout->addLayout(listHeader);

    auto* list = new QListWidget(listFrame);
    list->setObjectName("modelList");
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setSpacing(6);
    list->setIconSize(QSize(20, 20));
    list->setFrameShape(QFrame::NoFrame);
    list->setWordWrap(true);
    listLayout->addWidget(list);

    auto* emptyLabel = new QLabel(tr("暂无模型，请点击“添加模型”按钮导入。"), listFrame);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color:#94a3b8;");
    emptyLabel->setVisible(scheme.models.isEmpty());
    emptyLabel->setMargin(12);
    listLayout->addWidget(emptyLabel);

    if (scheme.models.isEmpty())
    {
        list->setVisible(false);
    }
    else
    {
        for (const ModelRecord& model : scheme.models)
        {
            auto* item = new QListWidgetItem(
                QIcon(QStringLiteral(":/icons/icons/model.svg")),
                tr("%1\n%2").arg(model.name,
                               QDir::toNativeSeparators(model.directory)));
            item->setToolTip(QDir::toNativeSeparators(model.jsonPath));
            item->setData(Qt::UserRole, model.id);
            list->addItem(item);
        }
    }

    connect(list, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                if (!item)
                    return;
                const QString modelId = item->data(Qt::UserRole).toString();
                if (modelId.isEmpty())
                    return;
                SchemeRecord* owner = nullptr;
                const ModelRecord* selected = modelById(modelId, &owner);
                if (!selected)
                    return;
                clearVtkScene();
                setVisualizationVisible(true);
                updateModelImagePreview(selected);
                if (ui->previewTabs && ui->imagePreviewTab)
                    ui->previewTabs->setCurrentWidget(ui->imagePreviewTab);
            });

    layout->addWidget(listFrame, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->setContentsMargins(0, 0, 0, 0);
    auto* addBtn = new QPushButton(tr("添加模型"), container);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
        "QPushButton{padding:8px 18px;border-radius:18px;border:none;"
        "background-color:#2563eb;color:#ffffff;font-weight:600;}"
        "QPushButton:hover{background-color:#1d4ed8;}"
        "QPushButton:pressed{background-color:#1e3a8a;}"
    );
    connect(addBtn, &QPushButton::clicked, this, [this, sid = scheme.id]() {
        promptAddModel(sid);
    });
    buttonRow->addWidget(addBtn);

    auto* openBtn = new QPushButton(tr("打开总成目录"), container);
    openBtn->setCursor(Qt::PointingHandCursor);
    openBtn->setStyleSheet(
        "QPushButton{padding:8px 18px;border-radius:18px;"
        "border:1px solid #cbd5f5;background:#f1f5ff;color:#1d4ed8;}"
        "QPushButton:hover{background:#e0e7ff;}"
        "QPushButton:pressed{background:#bfdbfe;}"
    );
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
    layout->setSpacing(12);

    auto* builder = new JsonPageBuilder(model.jsonPath, container);
    layout->addWidget(builder, 1);

    connect(builder, &JsonPageBuilder::logMessage,
            this, &MainWindow::appendLogMessage);
    connect(builder, &JsonPageBuilder::calculationFinished,
            this, [this](const QString& resultPath) {
        if (resultPath.isEmpty())
        {
            appendLogMessage(tr("未检测到新的输出文件"));
            return;
        }
        appendLogMessage(tr("加载 OBJ：%1")
                             .arg(QDir::toNativeSeparators(resultPath)));
        displayResultFile(resultPath);
    });

//    auto* openBtn = new QPushButton(tr("打开模型目录"), container);
//    openBtn->setCursor(Qt::PointingHandCursor);
//    openBtn->setStyleSheet(
//        "QPushButton{padding:8px 18px;border-radius:18px;"
//        "border:1px solid #cbd5f5;background:#f8faff;color:#1d4ed8;}"
//        "QPushButton:hover{background:#e0e7ff;}"
//        "QPushButton:pressed{background:#bfdbfe;}"
//    );
//    connect(openBtn, &QPushButton::clicked, this, [path = model.directory]() {
//        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
//    });
//    layout->addWidget(openBtn, 0, Qt::AlignLeft);

    return container;
}

QWidget* MainWindow::buildProjectInfoWidget()
{
    auto* container = new QWidget(ui->settingWidget);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("工程：%1").arg(projectDisplayName()), container);
    title->setStyleSheet("font-size:20px;font-weight:700;color:#1b2b4d;");
    layout->addWidget(title);

    auto* pathLabel = new QLabel(
        tr("目录：%1").arg(QDir::toNativeSeparators(m_projectRoot)), container);
    pathLabel->setStyleSheet("color:#475569;");
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    auto* frame = new QFrame(container);
    frame->setObjectName(QStringLiteral("projectInfoFrame"));
    frame->setStyleSheet(
        "QFrame#projectInfoFrame{background:#ffffff;border:1px solid #d0d5dd;"
        "border-radius:10px;}");
    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(16, 16, 16, 16);
    frameLayout->setSpacing(12);

    auto* remarkLabel = new QLabel(tr("工程备注"), frame);
    remarkLabel->setStyleSheet("font-weight:600;color:#1b2b4d;");
    frameLayout->addWidget(remarkLabel);

    auto* remarkEdit = new QPlainTextEdit(frame);
    remarkEdit->setPlaceholderText(tr("请输入工程备注"));
    remarkEdit->setPlainText(m_projectRemarks);
    remarkEdit->setMinimumHeight(100);
    remarkEdit->setStyleSheet(
        "QPlainTextEdit{border:1px solid #cbd5f5;border-radius:8px;"
        "background:#f8fafc;color:#0f172a;}"
        "QPlainTextEdit:focus{border-color:#2563eb;background:#ffffff;}");
    frameLayout->addWidget(remarkEdit);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->setContentsMargins(0, 0, 0, 0);

    auto* saveBtn = new QPushButton(tr("保存备注"), frame);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(
        "QPushButton{padding:8px 18px;border-radius:18px;border:none;"
        "background:#2563eb;color:#ffffff;font-weight:600;}"
        "QPushButton:disabled{background:#94a3b8;}"
        "QPushButton:hover:!disabled{background:#1d4ed8;}"
        "QPushButton:pressed:!disabled{background:#1e3a8a;}");
    saveBtn->setEnabled(false);
    buttonRow->addWidget(saveBtn);

    auto* openBtn = new QPushButton(tr("打开工程目录"), frame);
    openBtn->setCursor(Qt::PointingHandCursor);
    openBtn->setStyleSheet(
        "QPushButton{padding:8px 18px;border-radius:18px;"
        "border:1px solid #cbd5f5;background:#f8faff;color:#1d4ed8;}"
        "QPushButton:hover{background:#e0e7ff;}"
        "QPushButton:pressed{background:#bfdbfe;}");
    buttonRow->addWidget(openBtn);
    buttonRow->addStretch(1);
    frameLayout->addLayout(buttonRow);

    auto* timeFrame = new QFrame(frame);
    timeFrame->setObjectName(QStringLiteral("projectTimeFrame"));
    timeFrame->setStyleSheet(
        "QFrame#projectTimeFrame{background:#f8fafc;border:1px dashed #cbd5f5;"
        "border-radius:10px;}");
    auto* timeLayout = new QVBoxLayout(timeFrame);
    timeLayout->setContentsMargins(12, 12, 12, 12);
    timeLayout->setSpacing(6);

    auto* createdRow = new QHBoxLayout();
    createdRow->setSpacing(6);
    createdRow->setContentsMargins(0, 0, 0, 0);
    auto* createdTitle = new QLabel(tr("创建时间"), timeFrame);
    createdTitle->setStyleSheet("font-weight:600;color:#1b2b4d;");
    createdRow->addWidget(createdTitle);
    createdRow->addStretch();
    auto* createdValue = new QLabel(timeFrame);
    createdValue->setStyleSheet("color:#475569;");
    createdValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    createdRow->addWidget(createdValue);
    timeLayout->addLayout(createdRow);

    frameLayout->addWidget(timeFrame);
    layout->addWidget(frame);

    const auto updateTimeLabels = [this, createdValue]() {
        const auto formatDate = [this](const QDateTime& dt) -> QString {
            if (!dt.isValid())
                return tr("未知");
            return QLocale::system().toString(dt.toLocalTime(),
                                              QLocale::LongFormat);
        };
        createdValue->setText(formatDate(m_projectCreatedAt));
    };
    updateTimeLabels();

    const auto refreshSaveState = [this, remarkEdit, saveBtn]() {
        saveBtn->setEnabled(remarkEdit->toPlainText() != m_projectRemarks);
    };
    refreshSaveState();

    connect(remarkEdit, &QPlainTextEdit::textChanged, this, refreshSaveState);
    connect(saveBtn, &QPushButton::clicked, this,
            [this, remarkEdit, saveBtn, updateTimeLabels]() {
                const QString newRemark = remarkEdit->toPlainText();
                if (newRemark == m_projectRemarks)
                {
                    saveBtn->setEnabled(false);
                    return;
                }
                m_projectRemarks = newRemark;
                persistSchemes();
                saveBtn->setEnabled(false);
                updateTimeLabels();
                appendLogMessage(tr("已更新工程备注"));
            });
    connect(openBtn, &QPushButton::clicked, this, [this]() {
        if (!m_projectRoot.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_projectRoot));
    });

    layout->addStretch(1);
    return container;
}

void MainWindow::showLibrarySchemeDetail(const QString& entryId,
                                         const QString& projectSchemeId)
{
    updateModelImagePreview(nullptr);
    SchemeLibraryEntry* entry = libraryEntryById(entryId);
    if (!entry)
    {
        clearDetailWidget();
        setVisualizationVisible(false);
        updateSelectionInfo();
        return;
    }

    bool linkedNameAdjusted = false;
    bool linkEstablished = false;
    SchemeRecord* linkedScheme = nullptr;
    if (!projectSchemeId.isEmpty())
    {
        linkedScheme = schemeById(projectSchemeId);
        if (linkedScheme && !linkedScheme->libraryId.isEmpty() &&
            linkedScheme->libraryId.compare(entryId, Qt::CaseInsensitive) != 0)
        {
            linkedScheme = nullptr;
        }
        else if (linkedScheme && linkedScheme->libraryId.isEmpty() &&
                 !entry->id.trimmed().isEmpty())
        {
            linkedScheme->libraryId = entry->id;
            linkEstablished = true;
        }
    }

    if (!linkedScheme)
    {
        linkedScheme = resolveSchemeForLibraryEntry(*entry, &linkedNameAdjusted, &linkEstablished);
    }

    if (linkedScheme && linkedNameAdjusted)
    {
        auto it = m_schemeItems.find(linkedScheme->id);
        if (it != m_schemeItems.end() && it.value())
            it.value()->setText(0, linkedScheme->name);
    }
    if (linkedScheme && (linkEstablished || linkedNameAdjusted))
        persistSchemes();

    const QString directory = entry->directory;
    if (directory.isEmpty())
    {
        QMessageBox::warning(this, tr("总成详情"), tr("总成库目录不存在或不可访问。"));
        clearDetailWidget();
        setVisualizationVisible(false);
        updateSelectionInfo();
        return;
    }

    if (!ensureDirectoryExists(directory))
    {
        QMessageBox::warning(this, tr("总成详情"),
                             tr("无法访问总成库目录：%1")
                                 .arg(QDir::toNativeSeparators(directory)));
        clearDetailWidget();
        setVisualizationVisible(false);
        updateSelectionInfo();
        return;
    }

    if (!linkedScheme)
        linkedScheme = schemeByLibraryId(entry->id);

    const auto entryDisplayName = [this, entry]() -> QString {
        const QString trimmed = entry->name.trimmed();
        return trimmed.isEmpty() ? tr("Untitled Assembly") : trimmed;
    };

    clearDetailWidget();
    if (linkedScheme)
        m_activeSchemeId = linkedScheme->id;
    else
        m_activeSchemeId.clear();
    m_activeModelId.clear();

    auto* container = new QWidget(ui->settingWidget);
    container->setObjectName(QStringLiteral("librarySchemeDetail"));
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(12);

    auto* backBtn = new QPushButton(tr("返回总成库"), container);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton{padding:4px 14px;border-radius:16px;border:1px solid #cbd5f5;"
        "background:#ffffff;color:#1d4ed8;font-weight:600;}"
        "QPushButton:hover{background:#e0e7ff;}"
        "QPushButton:pressed{background:#dbeafe;}"
    );
    titleLayout->addWidget(backBtn, 0, Qt::AlignLeft);

    auto* titleLabel = new QLabel(entryDisplayName(), container);
    titleLabel->setStyleSheet("font-size:20px;font-weight:700;color:#1b2b4d;");
    titleLayout->addWidget(titleLabel, 1);

    auto* renameBtn = new QPushButton(tr("重命名"), container);
    renameBtn->setCursor(Qt::PointingHandCursor);
    renameBtn->setStyleSheet(
        "QPushButton{padding:4px 14px;border-radius:16px;"
        "border:1px solid #cbd5f5;background:#f8faff;color:#1d4ed8;}"
        "QPushButton:hover{background:#e0e7ff;}"
        "QPushButton:pressed{background:#bfdbfe;}");
    titleLayout->addStretch();
    titleLayout->addWidget(renameBtn);

    layout->addLayout(titleLayout);

    connect(backBtn, &QPushButton::clicked, this, [this]() {
        bool selectionChanged = false;
        if (ui->treeModels && m_libraryRootItem)
        {
            if (ui->treeModels->currentItem() != m_libraryRootItem)
            {
                ui->treeModels->setCurrentItem(m_libraryRootItem);
                selectionChanged = true;
            }
        }

        if (!selectionChanged)
        {
            if (ui->stackedWidget)
                ui->stackedWidget->setCurrentWidget(ui->planPage);
            updateGallery();
            clearDetailWidget();
            clearVtkScene();
            setVisualizationVisible(false);
            m_activeSchemeId.clear();
            m_activeModelId.clear();
            updateSelectionInfo();
        }
    });

    auto* listFrame = new QFrame(container);
    listFrame->setObjectName("libraryModelFrame");
    listFrame->setStyleSheet(
        "QFrame#libraryModelFrame{background:#ffffff;border:1px solid #d0d5dd;"
        "border-radius:10px;}"
        "QListWidget#libraryModelList{border:none;background:transparent;}"
        "QListWidget#libraryModelList::item{padding:10px;border-radius:8px;color:#0f172a;}"
        "QListWidget#libraryModelList::item:hover{background:rgba(37,99,235,0.12);}" 
        "QListWidget#libraryModelList::item:selected{background:rgba(37,99,235,0.18);"
        "border:1px solid rgba(37,99,235,0.35);color:#0f172a;}"
        "QListWidget#libraryModelList::item:selected:!active{background:rgba(37,99,235,0.15);}");
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    auto* listTitle = new QLabel(tr("模型列表"), listFrame);
    listTitle->setStyleSheet("font-weight:600;color:#1b2b4d;");

    listTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* countLabel = new QLabel(listFrame);
    countLabel->setStyleSheet("color:#64748b;");
    headerLayout->addWidget(listTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(countLabel);
    countLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    listLayout->addLayout(headerLayout);




    auto* listWidget = new QListWidget(listFrame);
    listWidget->setObjectName("libraryModelList");
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listWidget->setSpacing(6);
    listWidget->setIconSize(QSize(48, 48));
    listWidget->setFrameShape(QFrame::NoFrame);
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    listLayout->addWidget(listWidget);

    auto* emptyLabel = new QLabel(
        tr("暂无模型，请点击下方“添加模型”按钮导入。"), listFrame);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet("color:#94a3b8;");
    emptyLabel->setMargin(12);
    listLayout->addWidget(emptyLabel);

    layout->addWidget(listFrame, 1);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(0, 0, 0, 0);

    const QString actionButtonStyle =
        QStringLiteral("QPushButton{padding:8px 18px;border-radius:18px;border:none;"
                       "background-color:#2563eb;color:#ffffff;font-weight:600;}"
                       "QPushButton:hover{background-color:#1d4ed8;}"
                       "QPushButton:pressed{background-color:#1e3a8a;}");

    auto* addModelBtn = new QPushButton(tr("添加模型"), container);
    addModelBtn->setCursor(Qt::PointingHandCursor);
    addModelBtn->setStyleSheet(actionButtonStyle);
    buttonLayout->addWidget(addModelBtn);

    auto* deleteModelBtn = new QPushButton(tr("删除模型"), container);
    deleteModelBtn->setCursor(Qt::PointingHandCursor);
    deleteModelBtn->setStyleSheet(actionButtonStyle);
    deleteModelBtn->setEnabled(false);
    deleteModelBtn->setToolTip(tr("请选择要删除的模型。"));
    buttonLayout->addWidget(deleteModelBtn);

    auto* addToProjectBtn = new QPushButton(tr("添加到工程"), container);
    addToProjectBtn->setCursor(Qt::PointingHandCursor);
    addToProjectBtn->setStyleSheet(actionButtonStyle);
    buttonLayout->addWidget(addToProjectBtn);

    auto* openDirBtn = new QPushButton(tr("打开总成目录"), container);
    openDirBtn->setCursor(Qt::PointingHandCursor);
    openDirBtn->setStyleSheet(actionButtonStyle);
    buttonLayout->addWidget(openDirBtn);

    buttonLayout->addStretch(1);

    layout->addLayout(buttonLayout);

    auto applySelectionState = [this, listWidget, deleteModelBtn, addToProjectBtn]() {
        const bool hasSelection = !listWidget->selectedItems().isEmpty();
        deleteModelBtn->setEnabled(hasSelection);
        deleteModelBtn->setToolTip(
            hasSelection ? QString() : tr("请选择要删除的模型。"));

        if (!hasActiveProject())
        {
            addToProjectBtn->setEnabled(false);
            addToProjectBtn->setToolTip(tr("请先新建或打开工程。"));
        }
        else
        {
            addToProjectBtn->setEnabled(hasSelection);
            addToProjectBtn->setToolTip(
                hasSelection ? QString() : tr("请选择要导入的模型。"));
        }
    };

    auto libraryModels = QSharedPointer<QVector<ModelRecord>>::create();

    auto refreshModels = [this, entry, listWidget, emptyLabel, countLabel,
                          applySelectionState, libraryModels]() {
        QSet<QString> previouslySelected;
        const QList<QListWidgetItem*> currentSelection = listWidget->selectedItems();
        for (QListWidgetItem* item : currentSelection)
        {
            if (!item)
                continue;
            previouslySelected.insert(item->data(Qt::UserRole).toString());
        }

        listWidget->clear();
        libraryModels->clear();
        *libraryModels = scanSchemeFolder(entry->directory);
        countLabel->setText(tr("%1 个模型").arg(libraryModels->size()));
        const bool empty = libraryModels->isEmpty();
        listWidget->setVisible(!empty);
        emptyLabel->setVisible(empty);

        for (int i = 0; i < libraryModels->size(); ++i)
        {
            const ModelRecord& model = libraryModels->at(i);
            QIcon icon(QStringLiteral(":/icons/icons/model.svg"));
            const QPixmap thumb = loadModelThumbnail(model);
            if (!thumb.isNull())
            {
                QPixmap scaled = thumb.scaled(listWidget->iconSize(), Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation);
                icon = QIcon(scaled);
            }
            auto* item = new QListWidgetItem(
                icon,
                tr("%1\n%2")
                    .arg(model.name,
                         QDir::toNativeSeparators(model.directory)));
            item->setToolTip(QDir::toNativeSeparators(model.jsonPath));
            item->setData(Qt::UserRole, model.directory);
            item->setData(Qt::UserRole + 1, model.name);
            item->setData(Qt::UserRole + 2, model.thumbnailPath);
            item->setData(Qt::UserRole + 3, i);
            listWidget->addItem(item);

            if (previouslySelected.contains(model.directory))
                item->setSelected(true);
        }

        applySelectionState();

        if (listWidget->selectedItems().isEmpty())
        {
            updateModelImagePreview(nullptr);
            clearVtkScene();
        }
        else
        {
            QListWidgetItem* first = listWidget->selectedItems().first();
            const int index = first->data(Qt::UserRole + 3).toInt();
            if (index >= 0 && index < libraryModels->size())
            {
                m_libraryPreviewModel = libraryModels->at(index);
                updateModelImagePreview(&m_libraryPreviewModel);
                clearVtkScene();
                if (ui->previewTabs && ui->imagePreviewTab)
                    ui->previewTabs->setCurrentWidget(ui->imagePreviewTab);
            }
            else
            {
                updateModelImagePreview(nullptr);
                clearVtkScene();
            }
        }
    };

    refreshModels();

    const auto refreshEntryUi = [entryDisplayName, titleLabel]() {
        titleLabel->setText(entryDisplayName());
    };

    connect(listWidget, &QListWidget::itemSelectionChanged, this,
            [this, listWidget, applySelectionState, libraryModels]() {
                applySelectionState();
                const QList<QListWidgetItem*> selected = listWidget->selectedItems();
                if (selected.isEmpty())
                {
                    updateModelImagePreview(nullptr);
                    clearVtkScene();
                    return;
                }

                const int index = selected.first()->data(Qt::UserRole + 3).toInt();
                if (index < 0 || index >= libraryModels->size())
                {
                    updateModelImagePreview(nullptr);
                    clearVtkScene();
                    return;
                }

                m_libraryPreviewModel = libraryModels->at(index);
                updateModelImagePreview(&m_libraryPreviewModel);
                clearVtkScene();
                if (ui->previewTabs && ui->imagePreviewTab)
                    ui->previewTabs->setCurrentWidget(ui->imagePreviewTab);
            });

    connect(listWidget, &QListWidget::customContextMenuRequested, this,
            [this, listWidget, libraryModels](const QPoint& pos) {
                QListWidgetItem* item = listWidget->itemAt(pos);
                if (!item)
                    return;

                const int index = item->data(Qt::UserRole + 3).toInt();
                if (index < 0 || index >= libraryModels->size())
                    return;

                QMenu menu(listWidget);
                QAction* setImageAction = menu.addAction(tr("设置模型图片"));
                QAction* clearImageAction = menu.addAction(tr("清除模型图片"));

                ModelRecord& model = (*libraryModels)[index];
                clearImageAction->setEnabled(!model.thumbnailPath.isEmpty());

                QAction* chosen = menu.exec(listWidget->viewport()->mapToGlobal(pos));
                if (!chosen)
                    return;

                if (chosen == setImageAction)
                {
                    QString initialDir = m_lastModelImageDir;
                    if (initialDir.isEmpty())
                    {
                        if (!model.thumbnailPath.isEmpty())
                            initialDir = QFileInfo(model.thumbnailPath).absolutePath();
                        else
                            initialDir = model.directory;
                    }

                    const QString caption = tr("选择模型图片 - %1")
                                                .arg(model.name.isEmpty()
                                                         ? tr("Untitled Model")
                                                         : model.name);
                    const QString file = QFileDialog::getOpenFileName(
                        this, caption, initialDir,
                        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"));
                    if (file.isEmpty())
                        return;

                    applyModelThumbnail(model, file);
                    m_lastModelImageDir = QFileInfo(file).absolutePath();

                    QPixmap thumb = loadModelThumbnail(model);
                    QIcon icon(QStringLiteral(":/icons/icons/model.svg"));
                    if (!thumb.isNull())
                    {
                        QPixmap scaled = thumb.scaled(listWidget->iconSize(), Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation);
                        icon = QIcon(scaled);
                    }
                    item->setIcon(icon);
                    item->setData(Qt::UserRole + 2, model.thumbnailPath);

                    if (item->isSelected())
                    {
                        m_libraryPreviewModel = model;
                        updateModelImagePreview(&m_libraryPreviewModel);
                        clearVtkScene();
                        if (ui->previewTabs && ui->imagePreviewTab)
                            ui->previewTabs->setCurrentWidget(ui->imagePreviewTab);
                    }
                }
                else if (chosen == clearImageAction)
                {
                    if (model.thumbnailPath.isEmpty())
                        return;

                    applyModelThumbnail(model, QString());
                    item->setIcon(QIcon(QStringLiteral(":/icons/icons/model.svg")));
                    item->setData(Qt::UserRole + 2, model.thumbnailPath);

                    if (item->isSelected())
                    {
                        m_libraryPreviewModel = model;
                        updateModelImagePreview(&m_libraryPreviewModel);
                        clearVtkScene();
                    }
                }
            });

    connect(renameBtn, &QPushButton::clicked, this,
            [this, entry, entryDisplayName, refreshEntryUi, refreshModels]() {
                bool ok = false;
                const QString currentName = entry->name;
                const QString newName = QInputDialog::getText(
                    this, tr("重命名总成"), tr("新的总成名称："), QLineEdit::Normal,
                    currentName, &ok);
                if (!ok)
                    return;

                const QString trimmed = newName.trimmed();
                if (trimmed.isEmpty())
                {
                    QMessageBox::warning(this, tr("重命名总成"), tr("总成名称不能为空。"));
                    return;
                }

                if (trimmed == entry->name)
                {
                    refreshEntryUi();
                    return;
                }

                const QString oldDirectory = entry->directory;
                QString updatedDirectory = oldDirectory;
                bool directoryChanged = false;

                if (entry->deletable &&
                    isPathWithinDirectory(oldDirectory, m_schemeLibraryRoot))
                {
                    QFileInfo dirInfo(oldDirectory);
                    QDir parentDir = dirInfo.dir();
                    QString sanitized = trimmed;
                    sanitized.replace(QRegularExpression("\\s+"), "_");
                    if (sanitized.isEmpty())
                        sanitized = QStringLiteral("Assembly");

                    QString desiredName = sanitized;
                    QString candidatePath = parentDir.filePath(desiredName);
                    int index = 1;
                    while (candidatePath.compare(oldDirectory, Qt::CaseInsensitive) != 0 &&
                           QDir(candidatePath).exists())
                    {
                        desiredName = QStringLiteral("%1_%2").arg(sanitized).arg(index++);
                        candidatePath = parentDir.filePath(desiredName);
                    }

                    if (candidatePath.compare(oldDirectory, Qt::CaseInsensitive) != 0)
                    {
                        if (!parentDir.rename(dirInfo.fileName(), desiredName))
                        {
                            QMessageBox::warning(this, tr("重命名总成"),
                                                 tr("无法重命名总成目录。"));
                            refreshEntryUi();
                            return;
                        }

                        QString canonical = canonicalPathForDir(QDir(candidatePath));
                        if (canonical.isEmpty())
                            canonical = QDir::cleanPath(candidatePath);
                        updatedDirectory = canonical;
                        directoryChanged = true;
                    }
                }

                entry->name = trimmed;
                if (directoryChanged)
                {
                    if (!entry->thumbnailPath.isEmpty() &&
                        isPathWithinDirectory(entry->thumbnailPath, oldDirectory))
                    {
                        QDir oldDir(oldDirectory);
                        const QString relThumb = oldDir.relativeFilePath(entry->thumbnailPath);
                        entry->thumbnailPath = QDir(updatedDirectory).filePath(relThumb);
                    }
                    entry->directory = updatedDirectory;
                }

                bool schemeUpdated = false;
                {
                    QScopedValueRollback<bool> guard(m_blockTreeSignals, true);
                    for (SchemeRecord& scheme : m_schemes)
                    {
                        if (scheme.libraryId.compare(entry->id, Qt::CaseInsensitive) != 0)
                            continue;

                        scheme.name = trimmed;
                        schemeUpdated = true;
                        auto it = m_schemeItems.find(scheme.id);
                        if (it != m_schemeItems.end() && it.value())
                            it.value()->setText(0, scheme.name);
                    }
                }

                refreshEntryUi();
                if (directoryChanged)
                    refreshModels();
                saveSchemeLibrary();
                if (schemeUpdated)
                    persistSchemes();
                const bool previewsUpdated =
                    renameLibrarySchemesInProjects(entry->id, trimmed);
                updateGallery();
                if (previewsUpdated)
                    refreshNavigation(m_activeSchemeId, m_activeModelId);
                appendLogMessage(tr("已将总成库重命名为 %1").arg(entryDisplayName()));
            });

    connect(openDirBtn, &QPushButton::clicked, this, [entry]() {
        if (!entry->directory.trimmed().isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(entry->directory));
    });

    connect(addModelBtn, &QPushButton::clicked, this,
            [this, entry, entryDisplayName, refreshModels]() {
                QFileDialog dlg(this, tr("选择模型目录"));
                dlg.setFileMode(QFileDialog::Directory);
                dlg.setOption(QFileDialog::ShowDirsOnly, true);
                dlg.setOption(QFileDialog::DontUseNativeDialog, true);
                dlg.setLabelText(QFileDialog::Accept, tr("导入"));
                dlg.setLabelText(QFileDialog::Reject, tr("取消"));
                if (dlg.exec() != QDialog::Accepted)
                    return;

                const QStringList selected = dlg.selectedFiles();
                const QStringList added =
                    importModelsIntoLibraryEntry(*entry, selected, true);
                if (!added.isEmpty())
                {
                    refreshModels();
                    appendLogMessage(
                        tr("已向总成库 %1 添加 %2 个模型")
                            .arg(entryDisplayName())
                            .arg(added.size()));
                    updateGallery();
                }
            });

    connect(deleteModelBtn, &QPushButton::clicked, this,
            [this, entry, listWidget, entryDisplayName, refreshModels]() {
                QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
                if (selectedItems.isEmpty())
                    return;

                QStringList modelDirs;
                QStringList modelNames;
                QSet<QString> fingerprints;
                modelDirs.reserve(selectedItems.size());
                modelNames.reserve(selectedItems.size());
                for (QListWidgetItem* item : selectedItems)
                {
                    if (!item)
                        continue;
                    const QString dir = item->data(Qt::UserRole).toString();
                    if (dir.trimmed().isEmpty())
                        continue;
                    if (!isPathWithinDirectory(dir, entry->directory))
                        continue;

                    QString jsonPath, batPath;
                    if (isModelFolder(QDir(dir), &jsonPath, &batPath))
                    {
                        const QString fingerprint = computeModelFingerprint(jsonPath);
                        if (!fingerprint.isEmpty())
                            fingerprints.insert(fingerprint);
                    }

                    modelDirs << dir;
                    modelNames << item->data(Qt::UserRole + 1).toString();
                }

                if (modelDirs.isEmpty())
                    return;

                const QString displayName = entryDisplayName();
                QString confirmText;
                if (modelDirs.size() == 1)
                {
                    QString modelName = modelNames.value(0).trimmed();
                    if (modelName.isEmpty())
                        modelName = tr("Untitled Model");
                    confirmText =
                        tr("确定要从总成库“%1”中删除模型“%2”吗？")
                            .arg(displayName, modelName);
                }
                else
                {
                    confirmText =
                        tr("确定要从总成库“%1”中删除选中的 %2 个模型吗？")
                            .arg(displayName)
                            .arg(modelDirs.size());
                }

                if (QMessageBox::question(this, tr("删除模型"), confirmText,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) != QMessageBox::Yes)
                    return;

                int removedCount = 0;
                QStringList failed;
                for (const QString& dir : modelDirs)
                {
                    if (dir.trimmed().isEmpty())
                        continue;

                    QDir folder(dir);
                    if (!folder.exists())
                    {
                        ++removedCount;
                        continue;
                    }

                    if (!folder.removeRecursively())
                    {
                        failed << QDir::toNativeSeparators(dir);
                        continue;
                    }

                    ++removedCount;
                }

                if (!failed.isEmpty())
                {
                    QMessageBox::warning(this, tr("删除模型"),
                                         tr("无法删除以下模型目录：\n%1")
                                             .arg(failed.join(QLatin1Char('\n'))));
                }

                refreshModels();

                if (removedCount > 0)
                {
                    appendLogMessage(tr("已从总成库 %1 删除 %2 个模型")
                                         .arg(displayName)
                                         .arg(removedCount));
                    updateGallery();

                    if (!fingerprints.isEmpty())
                    {
                        bool projectUpdated = false;
                        QString schemeToRefresh;
                        for (SchemeRecord& scheme : m_schemes)
                        {
                            if (scheme.libraryId.compare(entry->id, Qt::CaseInsensitive) != 0)
                                continue;
                            if (removeModelsByFingerprint(scheme, fingerprints))
                            {
                                if (schemeToRefresh.isEmpty())
                                    schemeToRefresh = scheme.id;
                                projectUpdated = true;
                            }
                        }
                        if (projectUpdated)
                        {
                            persistSchemes();
                            refreshNavigation(schemeToRefresh);
                            appendLogMessage(tr("已同步移除工程中的关联模型"));
                        }
                    }
                }
            });

    auto performAddToProject = [this, entry, entryDisplayName,
                                refreshEntryUi](const QList<QListWidgetItem*>& selectedItems) {
        if (!hasActiveProject())
        {
            QMessageBox::information(this, tr("添加到工程"),
                                     tr("请先新建或打开工程。"));
            return;
        }

        if (selectedItems.isEmpty())
        {
            QMessageBox::information(this, tr("添加到工程"),
                                     tr("请选择要导入的模型。"));
            return;
        }

        QStringList modelDirectories;
        for (QListWidgetItem* item : selectedItems)
        {
            if (!item)
                continue;
            const QString dir = item->data(Qt::UserRole).toString();
            if (!dir.trimmed().isEmpty())
                modelDirectories << dir;
        }
        if (modelDirectories.isEmpty())
            return;

        bool linkEstablished = false;
        bool linkedNameAdjusted = false;
        SchemeRecord* linkedSchemeForImport =
            resolveSchemeForLibraryEntry(*entry, &linkedNameAdjusted, &linkEstablished);
        QString targetSchemeId = linkedSchemeForImport ? linkedSchemeForImport->id : QString();
        if (linkedSchemeForImport && linkedNameAdjusted)
        {
            auto it = m_schemeItems.find(linkedSchemeForImport->id);
            if (it != m_schemeItems.end() && it.value())
                it.value()->setText(0, linkedSchemeForImport->name);
        }
        bool createdNewScheme = false;
        bool schemeRenamed = linkedNameAdjusted;
        bool linkNeedsPersist = linkEstablished;
        QString newSchemeDir;

        if (targetSchemeId.isEmpty())
        {
            QString schemeName = entryDisplayName();

            QString workingDir = makeUniqueWorkspaceSubdir(schemeName);
            if (workingDir.isEmpty())
            {
                QMessageBox::warning(this, tr("添加到工程"),
                                     tr("无法创建总成工作目录。"));
                return;
            }
            if (!ensureDirectoryExists(workingDir))
            {
                QMessageBox::warning(this, tr("添加到工程"),
                                     tr("无法创建工作目录：%1")
                                         .arg(QDir::toNativeSeparators(workingDir)));
                return;
            }

            const QString createdId = createScheme(schemeName, workingDir);
            if (createdId.isEmpty())
            {
                QDir(workingDir).removeRecursively();
                QMessageBox::warning(this, tr("添加到工程"),
                                     tr("无法创建总成记录。"));
                return;
            }

            if (SchemeRecord* scheme = schemeById(createdId))
            {
                scheme->libraryId = entry->id;
                scheme->name = schemeName;
            }

            targetSchemeId = createdId;
            createdNewScheme = true;
            newSchemeDir = workingDir;
        }
        else
        {
            if (SchemeRecord* scheme = schemeById(targetSchemeId))
            {
                const QString desiredName = entryDisplayName();
                if (scheme->name != desiredName)
                {
                    scheme->name = desiredName;
                    schemeRenamed = true;
                    auto it = m_schemeItems.find(scheme->id);
                    if (it != m_schemeItems.end() && it.value())
                        it.value()->setText(0, scheme->name);
                }
                if (!scheme->libraryId.isEmpty() &&
                    scheme->libraryId.compare(entry->id, Qt::CaseInsensitive) != 0 &&
                    !entry->id.trimmed().isEmpty())
                {
                    scheme->libraryId = entry->id;
                    linkNeedsPersist = true;
                }
                else if (scheme->libraryId.isEmpty() && !entry->id.trimmed().isEmpty())
                {
                    scheme->libraryId = entry->id;
                    linkNeedsPersist = true;
                }
            }
        }

        if (!targetSchemeId.isEmpty())
        {
            const QVector<QString> added =
                importModelsIntoScheme(targetSchemeId, modelDirectories);
            if (!added.isEmpty())
            {
                ui->stackedWidget->setCurrentWidget(ui->MainPage);
            }
            else if (createdNewScheme)
            {
                for (int i = 0; i < m_schemes.size(); ++i)
                {
                    if (m_schemes[i].id == targetSchemeId)
                    {
                        if (isPathWithinDirectory(m_schemes[i].thumbnailPath,
                                                  m_schemes[i].workingDirectory))
                            QFile::remove(m_schemes[i].thumbnailPath);
                        m_schemes.removeAt(i);
                        break;
                    }
                }
                if (!newSchemeDir.isEmpty())
                {
                    QDir dir(newSchemeDir);
                    if (dir.exists())
                        dir.removeRecursively();
                }
                persistSchemes();
                refreshNavigation();
            }
            else if (schemeRenamed || linkNeedsPersist)
            {
                persistSchemes();
            }
        }

        refreshEntryUi();
    };

    connect(addToProjectBtn, &QPushButton::clicked, this,
            [performAddToProject, listWidget]() {
                performAddToProject(listWidget->selectedItems());
            });

    connect(listWidget, &QListWidget::itemDoubleClicked, this,
            [performAddToProject](QListWidgetItem* item) {
                if (!item)
                    return;
                QList<QListWidgetItem*> items;
                items << item;
                performAddToProject(items);
            });

    m_currentDetailWidget = container;
    ui->settingWidget->layout()->addWidget(container);
    setVisualizationVisible(true);
    clearVtkScene();
    if (ui->previewTabs && ui->imagePreviewTab)
        ui->previewTabs->setCurrentWidget(ui->imagePreviewTab);
    updateSelectionInfo(entry->directory,
                        linkedScheme ? linkedScheme->remarks : QString());
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

MainWindow::SchemeRecord* MainWindow::schemeByLibraryId(const QString& libraryId)
{
    if (libraryId.trimmed().isEmpty())
        return nullptr;

    for (SchemeRecord& scheme : m_schemes)
    {
        if (scheme.libraryId.compare(libraryId, Qt::CaseInsensitive) == 0)
            return &scheme;
    }
    return nullptr;
}

const MainWindow::SchemeRecord* MainWindow::schemeByLibraryId(const QString& libraryId) const
{
    if (libraryId.trimmed().isEmpty())
        return nullptr;

    for (const SchemeRecord& scheme : m_schemes)
    {
        if (scheme.libraryId.compare(libraryId, Qt::CaseInsensitive) == 0)
            return &scheme;
    }
    return nullptr;
}

MainWindow::SchemeRecord* MainWindow::resolveSchemeForLibraryEntry(
    const SchemeLibraryEntry& entry, bool* schemeNameAdjusted, bool* linkEstablished)
{
    if (!entry.id.trimmed().isEmpty())
    {
        if (SchemeRecord* linked = schemeByLibraryId(entry.id))
            return linked;
    }

    QString entryName = entry.name.trimmed();
    if (entryName.isEmpty() && !entry.directory.trimmed().isEmpty())
    {
        QDir dir(entry.directory);
        entryName = dir.dirName().trimmed();
    }

    QString sanitizedName = entryName;
    sanitizedName.replace(QRegularExpression("\\s+"), "_");
    sanitizedName = sanitizedName.trimmed();

    if (entryName.isEmpty() && sanitizedName.isEmpty())
        return nullptr;

    const auto namesMatch = [&](const SchemeRecord& scheme) {
        if (!entryName.isEmpty() &&
            scheme.name.compare(entryName, Qt::CaseInsensitive) == 0)
            return true;

        QString schemeSanitized = scheme.name;
        schemeSanitized.replace(QRegularExpression("\\s+"), "_");
        schemeSanitized = schemeSanitized.trimmed();
        if (!schemeSanitized.isEmpty() && !sanitizedName.isEmpty() &&
            schemeSanitized.compare(sanitizedName, Qt::CaseInsensitive) == 0)
            return true;

        const QString dirName = QFileInfo(scheme.workingDirectory).fileName();
        if (!dirName.isEmpty() && !sanitizedName.isEmpty() &&
            dirName.compare(sanitizedName, Qt::CaseInsensitive) == 0)
            return true;

        return false;
    };

    const QString entryIdTrimmed = entry.id.trimmed();

    for (SchemeRecord& scheme : m_schemes)
    {
        if (!scheme.libraryId.isEmpty())
        {
            if (!entryIdTrimmed.isEmpty() &&
                scheme.libraryId.compare(entryIdTrimmed, Qt::CaseInsensitive) == 0)
            {
                return &scheme;
            }

            if (libraryEntryById(scheme.libraryId))
                continue;
        }

        if (!namesMatch(scheme))
            continue;

        if (!entryIdTrimmed.isEmpty())
        {
            scheme.libraryId = entryIdTrimmed;
            if (linkEstablished)
                *linkEstablished = true;
        }

        if (!entryName.isEmpty() && scheme.name != entryName)
        {
            scheme.name = entryName;
            if (schemeNameAdjusted)
                *schemeNameAdjusted = true;
        }

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
    scheme.name = makeUniqueSchemeName(trimmedName, scheme.id);
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
        existing->name = makeUniqueSchemeName(dir.dirName(), existing->id);
        existing->models = scanSchemeFolder(canonical);
        ensureUniqueModelNames(*existing);
        persistSchemes();
        refreshNavigation(existing->id);
        return existing->id;
    }

    SchemeRecord scheme;
    scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    scheme.name = makeUniqueSchemeName(dir.dirName(), scheme.id);
    scheme.workingDirectory = canonical;
    scheme.models = scanSchemeFolder(canonical);
    ensureUniqueModelNames(scheme);
    const QStringList covers = dir.entryList(QStringList() << QStringLiteral("scheme_cover.*"),
                                             QDir::Files | QDir::NoDotAndDotDot);
    if (!covers.isEmpty())
        scheme.thumbnailPath = QDir::cleanPath(dir.filePath(covers.first()));

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
                                 tr("无法创建总成工作目录：%1")
                                     .arg(QDir::toNativeSeparators(scheme->workingDirectory)));
        return addedIds;
    }

    QDir workingDir(scheme->workingDirectory);
    QSet<QString> existingPaths;
    QSet<QString> existingFingerprints;
    for (ModelRecord& model : scheme->models)
    {
        const QString canonicalDir = canonicalPathForDir(QDir(model.directory));
        if (!canonicalDir.isEmpty())
            existingPaths.insert(canonicalDir);

        QString fingerprint = model.fingerprint;
        if (fingerprint.isEmpty())
            fingerprint = computeModelFingerprint(model.jsonPath);
        if (!fingerprint.isEmpty())
        {
            existingFingerprints.insert(fingerprint);
            if (model.fingerprint.isEmpty())
                model.fingerprint = fingerprint;
        }
    }

    bool duplicateSkipped = false;

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
            QString sourceFingerprint = computeModelFingerprint(jsonPath);
            if (!sourceFingerprint.isEmpty() && existingFingerprints.contains(sourceFingerprint))
            {
                duplicateSkipped = true;
                continue;
            }

            QString destPath = uniqueChildPath(workingDir, src.dirName());
            if (!copyDirectoryRecursively(src.absolutePath(), destPath))
            {
                QDir(destPath).removeRecursively();
                if (showError)
                    QMessageBox::warning(this, tr("导入失败"),
                                         tr("无法复制模型文件夹：%1")
                                             .arg(QDir::toNativeSeparators(path)));
                continue;
            }

            QDir destDir(destPath);
            ModelRecord model;
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            model.name = makeUniqueModelName(*scheme, destDir.dirName());
            model.directory = canonicalPathForDir(destDir);
            const QString jsonName = QFileInfo(jsonPath).fileName();
            model.jsonPath = destDir.filePath(jsonName);
            const QString batName = QFileInfo(batPath).fileName();
            model.batPath = batName.isEmpty() ? QString() : destDir.filePath(batName);
            const QStringList covers = destDir.entryList(QStringList() << QStringLiteral("model_cover.*"),
                                                         QDir::Files | QDir::NoDotAndDotDot);
            if (!covers.isEmpty())
                model.thumbnailPath = QDir::cleanPath(destDir.filePath(covers.first()));
            if (model.directory.isEmpty() || existingPaths.contains(model.directory))
            {
                QDir(destPath).removeRecursively();
                continue;
            }

            model.fingerprint = sourceFingerprint.isEmpty()
                                     ? computeModelFingerprint(model.jsonPath)
                                     : sourceFingerprint;
            if (!model.fingerprint.isEmpty())
                existingFingerprints.insert(model.fingerprint);

            scheme->models.push_back(model);
            addedIds.push_back(model.id);
            existingPaths.insert(model.directory);
            continue;
        }

        const QString canonicalSource = canonicalPathForDir(src);
        QVector<ModelRecord> nested =
            scanSchemeFolder(canonicalSource.isEmpty() ? src.absolutePath() : canonicalSource);
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
            const QString sourceDir = model.directory;
            QString sourceFingerprint = model.fingerprint;

            if (!sourceFingerprint.isEmpty() && existingFingerprints.contains(sourceFingerprint))
            {
                duplicateSkipped = true;
                continue;
            }

            QString destPath = uniqueChildPath(workingDir, QFileInfo(model.directory).fileName());
            if (!copyDirectoryRecursively(sourceDir, destPath))
            {
                QDir(destPath).removeRecursively();
                if (showError)
                    QMessageBox::warning(this, tr("导入失败"),
                                         tr("无法复制模型文件夹：%1")
                                             .arg(QDir::toNativeSeparators(sourceDir)));
                continue;
            }

            QDir destDir(destPath);
            model.directory = canonicalPathForDir(destDir);
            const QString jsonName = QFileInfo(model.jsonPath).fileName();
            model.jsonPath = destDir.filePath(jsonName);
            const QString batName = QFileInfo(model.batPath).fileName();
            model.batPath = batName.isEmpty() ? QString() : destDir.filePath(batName);
            const QStringList covers = destDir.entryList(QStringList() << QStringLiteral("model_cover.*"),
                                                         QDir::Files | QDir::NoDotAndDotDot);
            if (!covers.isEmpty())
                model.thumbnailPath = QDir::cleanPath(destDir.filePath(covers.first()));
            model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            model.name = makeUniqueModelName(*scheme, model.name);

            if (model.directory.isEmpty() || existingPaths.contains(model.directory))
            {
                QDir(destPath).removeRecursively();
                continue;
            }

            if (sourceFingerprint.isEmpty())
                sourceFingerprint = computeModelFingerprint(model.jsonPath);
            model.fingerprint = sourceFingerprint;
            if (!model.fingerprint.isEmpty())
                existingFingerprints.insert(model.fingerprint);

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

    if (duplicateSkipped && showError)
    {
        QMessageBox::information(this, tr("导入模型"),
                                 tr("所选模型中部分已存在于当前总成，已跳过重复项。"));
    }

    return addedIds;
}

QStringList MainWindow::importModelsIntoLibraryEntry(SchemeLibraryEntry& entry,
                                                    const QStringList& paths,
                                                    bool showError)
{
    QStringList added;
    if (entry.directory.isEmpty())
    {
        if (showError)
            QMessageBox::warning(this, tr("导入模型"), tr("总成库目录不存在。"));
        return added;
    }

    if (!ensureDirectoryExists(entry.directory))
    {
        if (showError)
            QMessageBox::warning(this, tr("导入模型"),
                                 tr("无法创建或访问总成库目录：%1")
                                     .arg(QDir::toNativeSeparators(entry.directory)));
        return added;
    }

    QDir target(entry.directory);
    const QString targetCanonical = canonicalPathForDir(target);

    for (const QString& path : paths)
    {
        if (path.trimmed().isEmpty())
            continue;

        QDir src(path);
        if (!src.exists())
        {
            if (showError)
                QMessageBox::warning(this, tr("导入模型"),
                                     tr("路径不存在：%1")
                                         .arg(QDir::toNativeSeparators(path)));
            continue;
        }

        const QString canonicalSrc = canonicalPathForDir(src);
        if (!canonicalSrc.isEmpty())
        {
            if (canonicalSrc == targetCanonical ||
                isPathWithinDirectory(canonicalSrc, entry.directory))
            {
                if (showError)
                    QMessageBox::information(
                        this, tr("导入模型"),
                        tr("请选择总成库目录以外的模型文件夹。"));
                continue;
            }
        }

        QString jsonPath, batPath;
        if (isModelFolder(src, &jsonPath, &batPath))
        {
            QString destPath = uniqueChildPath(target, src.dirName());
            if (!copyDirectoryRecursively(src.absolutePath(), destPath))
            {
                QDir(destPath).removeRecursively();
                if (showError)
                    QMessageBox::warning(this, tr("导入模型"),
                                         tr("无法复制模型文件夹：%1")
                                             .arg(QDir::toNativeSeparators(path)));
                continue;
            }

            added << QDir::cleanPath(destPath);
            continue;
        }

        const QVector<ModelRecord> nested =
            scanSchemeFolder(canonicalSrc.isEmpty() ? src.absolutePath() : canonicalSrc);
        if (nested.isEmpty())
        {
            if (showError)
                QMessageBox::warning(this, tr("导入模型"),
                                     tr("%1 不是有效的模型文件夹。")
                                         .arg(QDir::toNativeSeparators(path)));
            continue;
        }

        for (const ModelRecord& model : nested)
        {
            const QString sourceDir = model.directory;
            if (sourceDir.isEmpty())
                continue;

            if (isPathWithinDirectory(sourceDir, entry.directory))
                continue;

            QString baseName = QFileInfo(sourceDir).fileName();
            if (baseName.isEmpty())
                baseName = model.name;

            QString destPath = uniqueChildPath(target, baseName);
            if (!copyDirectoryRecursively(sourceDir, destPath))
            {
                QDir(destPath).removeRecursively();
                if (showError)
                    QMessageBox::warning(this, tr("导入模型"),
                                         tr("无法复制模型文件夹：%1")
                                             .arg(QDir::toNativeSeparators(sourceDir)));
                continue;
            }

            added << QDir::cleanPath(destPath);
        }
    }

    return added;
}

bool MainWindow::isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const
{
    QDir copy(dir);
    const QStringList jsons = copy.entryList(QStringList() << "*.json",
                                             QDir::Files | QDir::NoDotAndDotDot);
    QString paraFile;
    for (const QString& file : jsons)
    {
        if (file.compare(QStringLiteral("para.json"), Qt::CaseInsensitive) == 0)
        {
            paraFile = file;
            break;
        }
    }
    if (paraFile.isEmpty())
        return false;

    const QStringList bats = copy.entryList(QStringList() << "*.bat",
                                            QDir::Files | QDir::NoDotAndDotDot);
    QString calcFile;
    for (const QString& file : bats)
    {
        if (file.compare(QStringLiteral("calculate.bat"), Qt::CaseInsensitive) == 0)
        {
            calcFile = file;
            break;
        }
    }
    if (calcFile.isEmpty())
        return false;

    if (jsonPath)
        *jsonPath = copy.absoluteFilePath(paraFile);
    if (batPath)
        *batPath = copy.absoluteFilePath(calcFile);
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
        model.fingerprint = computeModelFingerprint(model.jsonPath);
        const QStringList covers = child.entryList(QStringList() << QStringLiteral("model_cover.*"),
                                                   QDir::Files | QDir::NoDotAndDotDot);
        if (!covers.isEmpty())
            model.thumbnailPath = QDir::cleanPath(child.filePath(covers.first()));
        models.push_back(model);
    }

    QSet<QString> taken;
    for (ModelRecord& model : models)
        model.name = makeUniqueName(model.name, taken, tr("Untitled Model"));
    return models;
}

QString MainWindow::computeModelFingerprint(const QString& jsonPath) const
{
    if (jsonPath.trimmed().isEmpty())
        return QString();

    QFile file(jsonPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return QString();

    return QString::fromLatin1(hash.result().toHex());
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

QPixmap MainWindow::loadSchemeThumbnail(const SchemeRecord& scheme) const
{
    if (scheme.thumbnailPath.isEmpty())
        return QPixmap();

    QImageReader reader(scheme.thumbnailPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
        return QPixmap();
    return QPixmap::fromImage(image);
}

QString MainWindow::storeSchemeThumbnail(const QString& schemeDir,
                                         const QString& sourcePath) const
{
    if (schemeDir.isEmpty() || sourcePath.trimmed().isEmpty())
        return QString();

    QFileInfo srcInfo(sourcePath);
    if (!srcInfo.exists() || !srcInfo.isFile())
        return QString();

    if (!ensureDirectoryExists(schemeDir))
        return QString();

    QDir dir(schemeDir);
    const QString suffix = srcInfo.suffix().isEmpty()
                               ? QStringLiteral("png")
                               : srcInfo.suffix().toLower();
    const QString targetName = QStringLiteral("scheme_cover.%1").arg(suffix);
    const QString targetPath = dir.filePath(targetName);

    if (!QFileInfo(sourcePath).absoluteFilePath().compare(targetPath, Qt::CaseInsensitive))
        return QDir::cleanPath(targetPath);

    if (QFile::exists(targetPath))
        QFile::remove(targetPath);

    if (!QFile::copy(srcInfo.absoluteFilePath(), targetPath))
        return QString();

    const QStringList duplicates = dir.entryList(QStringList() << QStringLiteral("scheme_cover.*"),
                                                 QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& dup : duplicates)
    {
        const QString absoluteDup = dir.filePath(dup);
        if (absoluteDup.compare(targetPath, Qt::CaseInsensitive) == 0)
            continue;
        QFile::remove(absoluteDup);
    }

    return QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
}

bool MainWindow::isPathWithinDirectory(const QString& filePath,
                                       const QString& directory) const
{
    if (filePath.isEmpty() || directory.isEmpty())
        return false;

    QDir dir(directory);
    const QString fileAbsolute = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
    const QString relative = dir.relativeFilePath(fileAbsolute);
    if (relative.isEmpty())
        return true;
    if (relative.startsWith(QStringLiteral("..")))
        return false;
    if (QDir::isAbsolutePath(relative))
        return false;
    return true;
}

QString MainWindow::schemeLibraryRoot() const
{
    return m_schemeLibraryRoot;
}

QString MainWindow::makeUniqueLibrarySubdir(const QString& baseName) const
{
    const QString root = schemeLibraryRoot();
    if (root.isEmpty())
        return QString();

    QDir dir(root);
    if (!dir.exists())
        ensureDirectoryExists(root);

    QString sanitized = baseName.trimmed();
    if (sanitized.isEmpty())
        sanitized = QStringLiteral("Assembly");
    sanitized.replace(QRegularExpression("\\s+"), "_");

    QString candidate = dir.filePath(sanitized);
    int index = 1;
    while (QDir(candidate).exists())
        candidate = dir.filePath(QStringLiteral("%1_%2").arg(sanitized).arg(index++));
    return candidate;
}

MainWindow::SchemeLibraryEntry* MainWindow::libraryEntryById(const QString& id)
{
    for (SchemeLibraryEntry& entry : m_librarySchemes)
    {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

const MainWindow::SchemeLibraryEntry* MainWindow::libraryEntryById(const QString& id) const
{
    for (const SchemeLibraryEntry& entry : m_librarySchemes)
    {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

QPixmap MainWindow::loadLibraryThumbnail(const SchemeLibraryEntry& entry) const
{
    if (entry.thumbnailPath.isEmpty())
        return QPixmap();

    QImageReader reader(entry.thumbnailPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
        return QPixmap();
    return QPixmap::fromImage(image);
}

QPixmap MainWindow::loadModelThumbnail(const ModelRecord& model) const
{
    if (model.thumbnailPath.isEmpty())
        return QPixmap();

    QImageReader reader(model.thumbnailPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
        return QPixmap();
    return QPixmap::fromImage(image);
}

QString MainWindow::storeModelThumbnail(const QString& modelDir,
                                        const QString& sourcePath) const
{
    if (modelDir.isEmpty() || sourcePath.trimmed().isEmpty())
        return QString();

    QFileInfo srcInfo(sourcePath);
    if (!srcInfo.exists() || !srcInfo.isFile())
        return QString();

    if (!ensureDirectoryExists(modelDir))
        return QString();

    QDir dir(modelDir);
    const QString suffix = srcInfo.suffix().isEmpty()
                               ? QStringLiteral("png")
                               : srcInfo.suffix().toLower();
    const QString targetName = QStringLiteral("model_cover.%1").arg(suffix);
    const QString targetPath = dir.filePath(targetName);

    if (!QFileInfo(sourcePath).absoluteFilePath().compare(targetPath, Qt::CaseInsensitive))
        return QDir::cleanPath(targetPath);

    if (QFile::exists(targetPath))
        QFile::remove(targetPath);

    if (!QFile::copy(srcInfo.absoluteFilePath(), targetPath))
        return QString();

    const QStringList duplicates = dir.entryList(QStringList() << QStringLiteral("model_cover.*"),
                                                 QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& dup : duplicates)
    {
        const QString absoluteDup = dir.filePath(dup);
        if (absoluteDup.compare(targetPath, Qt::CaseInsensitive) == 0)
            continue;
        QFile::remove(absoluteDup);
    }

    return QDir::cleanPath(QFileInfo(targetPath).absoluteFilePath());
}

void MainWindow::applyLibraryThumbnail(SchemeLibraryEntry& entry,
                                       const QString& sourcePath)
{
    const QString trimmed = sourcePath.trimmed();
    if (trimmed.isEmpty())
    {
        if (isPathWithinDirectory(entry.thumbnailPath, entry.directory))
            QFile::remove(entry.thumbnailPath);
        entry.thumbnailPath.clear();
        return;
    }

    QString stored = storeSchemeThumbnail(entry.directory, trimmed);
    if (stored.isEmpty())
        stored = QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());

    if (!entry.thumbnailPath.isEmpty() && entry.thumbnailPath != stored &&
        isPathWithinDirectory(entry.thumbnailPath, entry.directory))
        QFile::remove(entry.thumbnailPath);

    entry.thumbnailPath = stored;
}

bool MainWindow::removeLibraryEntry(const QString& id)
{
    for (int i = 0; i < m_librarySchemes.size(); ++i)
    {
        if (m_librarySchemes[i].id == id)
        {
            SchemeLibraryEntry entry = m_librarySchemes.takeAt(i);
            if (entry.deletable && isPathWithinDirectory(entry.directory, m_schemeLibraryRoot))
            {
                QDir dir(entry.directory);
                dir.removeRecursively();
            }
            if (entry.deletable)
                saveSchemeLibrary();
            return true;
        }
    }
    return false;
}

bool MainWindow::hasActiveProject() const
{
    return !m_projectRoot.isEmpty();
}

void MainWindow::applySchemeThumbnail(SchemeRecord& scheme, const QString& sourcePath)
{
    const QString trimmed = sourcePath.trimmed();
    if (trimmed.isEmpty())
    {
        if (isPathWithinDirectory(scheme.thumbnailPath, scheme.workingDirectory))
            QFile::remove(scheme.thumbnailPath);
        scheme.thumbnailPath.clear();
        return;
    }

    QString stored = storeSchemeThumbnail(scheme.workingDirectory, trimmed);
    if (stored.isEmpty())
        stored = QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());

    if (!scheme.thumbnailPath.isEmpty() && scheme.thumbnailPath != stored &&
        isPathWithinDirectory(scheme.thumbnailPath, scheme.workingDirectory))
        QFile::remove(scheme.thumbnailPath);

    scheme.thumbnailPath = stored;
}

void MainWindow::applyModelThumbnail(ModelRecord& model, const QString& sourcePath)
{
    const QString trimmed = sourcePath.trimmed();
    if (trimmed.isEmpty())
    {
        if (isPathWithinDirectory(model.thumbnailPath, model.directory))
            QFile::remove(model.thumbnailPath);
        model.thumbnailPath.clear();
        return;
    }

    QString stored = storeModelThumbnail(model.directory, trimmed);
    if (stored.isEmpty())
        stored = QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());

    if (!model.thumbnailPath.isEmpty() && model.thumbnailPath != stored &&
        isPathWithinDirectory(model.thumbnailPath, model.directory))
        QFile::remove(model.thumbnailPath);

    model.thumbnailPath = stored;
}

void MainWindow::promptAddScheme()
{
    if (!hasActiveProject())
    {
        QMessageBox::information(this, tr("创建总成"), tr("请先新建或打开工程。"));
        return;
    }

    const QString defaultName = tr("NewAssembly%1").arg(m_schemes.size() + 1);
    SchemeSettingsDialog dlg(defaultName, QString(), false, this);
    dlg.setDirectoryHint(tr("工作目录将在工程中自动生成"));
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = dlg.schemeName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("创建总成"), tr("总成名称不能为空"));
        return;
    }

    QString directory = makeUniqueWorkspaceSubdir(name);
    if (directory.isEmpty())
    {
        QMessageBox::warning(this, tr("创建总成"), tr("无法创建总成工作目录"));
        return;
    }

    if (!ensureDirectoryExists(directory))
    {
        QMessageBox::warning(this, tr("创建总成"),
                             tr("无法创建工作目录：%1")
                                 .arg(QDir::toNativeSeparators(directory)));
        return;
    }

    const QString id = importSchemeFromDirectory(directory, false);
    if (!id.isEmpty())
    {
        if (SchemeRecord* scheme = schemeById(id))
        {
            scheme->name = makeUniqueSchemeName(name, scheme->id);
            applySchemeThumbnail(*scheme, dlg.thumbnailPath());
        }
        persistSchemes();
        refreshNavigation(id);
        ui->stackedWidget->setCurrentWidget(ui->MainPage);
        appendLogMessage(tr("已创建总成 %1").arg(name));
    }
    else
    {
        QDir(directory).removeRecursively();
    }
}

void MainWindow::promptAddModel(const QString& schemeId)
{
    if (!hasActiveProject())
    {
        QMessageBox::information(this, tr("导入模型"), tr("请先新建或打开工程。"));
        return;
    }

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

        bool thumbnailUpdated = false;
        QString lastDir = m_lastModelImageDir;
        for (const QString& modelId : added)
        {
            SchemeRecord* owner = nullptr;
            ModelRecord* model = modelById(modelId, &owner);
            if (!model)
                continue;

            QString initialDir = lastDir;
            if (initialDir.isEmpty())
            {
                if (!model->thumbnailPath.isEmpty())
                    initialDir = QFileInfo(model->thumbnailPath).absolutePath();
                else
                    initialDir = model->directory;
            }

            const QString caption = tr("选择模型图片 - %1").arg(
                model->name.isEmpty() ? tr("Untitled Model") : model->name);
            const QString file = QFileDialog::getOpenFileName(
                this, caption, initialDir,
                tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)"));
            if (file.isEmpty())
                continue;

            applyModelThumbnail(*model, file);
            lastDir = QFileInfo(file).absolutePath();
            thumbnailUpdated = true;
        }

        if (thumbnailUpdated)
        {
            persistSchemes();
            m_lastModelImageDir = lastDir;
            refreshNavigation(schemeId, added.first());
            refreshCurrentDetail();
        }
    }
}

void MainWindow::openSchemeSettings(const QString& schemeId)
{
    SchemeRecord* scheme = schemeById(schemeId);
    if (!scheme)
        return;

    SchemeSettingsDialog dlg(scheme->name, scheme->workingDirectory, false, this,
                             scheme->thumbnailPath);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString newName = dlg.schemeName().trimmed();
    if (!newName.isEmpty())
        scheme->name = makeUniqueSchemeName(newName, scheme->id);

    applySchemeThumbnail(*scheme, dlg.thumbnailPath());

    bool libraryUpdated = false;
    if (!scheme->libraryId.isEmpty())
    {
        if (SchemeLibraryEntry* entry = libraryEntryById(scheme->libraryId))
        {
            const QString previousThumbnail = entry->thumbnailPath;
            applyLibraryThumbnail(*entry, dlg.thumbnailPath());
            if (entry->thumbnailPath != previousThumbnail)
                libraryUpdated = true;
        }
    }

    if (libraryUpdated)
        saveSchemeLibrary();

    persistSchemes();
    refreshNavigation(schemeId, m_activeModelId);
}

bool MainWindow::confirmSchemeDeletion(const SchemeRecord& scheme)
{
    const QString schemeName = scheme.name.isEmpty()
                                  ? tr("Untitled Assembly")
                                  : scheme.name;
    const QString text =
        tr("确定要移除总成“%1”吗？此操作将删除总成下的所有模型及相关文件。")
            .arg(schemeName);
    return QMessageBox::question(this, tr("移除总成"), text,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

bool MainWindow::confirmModelDeletion(const ModelRecord& model,
                                      const SchemeRecord& owner)
{
    const QString modelName = model.name.isEmpty()
                                  ? tr("Untitled Model")
                                  : model.name;
    const QString schemeName = owner.name.isEmpty()
                                   ? tr("Untitled Assembly")
                                   : owner.name;
    const QString text =
        tr("确定要从总成“%1”中移除模型“%2”吗？相关文件将被删除。")
            .arg(schemeName, modelName);
    return QMessageBox::question(this, tr("移除模型"), text,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

void MainWindow::removeSchemeById(const QString& id)
{
    for (int i = 0; i < m_schemes.size(); ++i)
    {
        if (m_schemes[i].id == id)
        {
            SchemeRecord scheme = m_schemes.takeAt(i);
            if (isPathWithinDirectory(scheme.thumbnailPath, scheme.workingDirectory))
                QFile::remove(scheme.thumbnailPath);
            if (m_activeSchemeId == id)
            {
                m_activeSchemeId.clear();
                m_activeModelId.clear();
            }
            updateModelImagePreview(nullptr);
            persistSchemes();

            const QString workingDir = scheme.workingDirectory;
            if (!workingDir.isEmpty())
            {
                const bool withinProject =
                    isPathWithinDirectory(workingDir, workspaceRoot()) ||
                    isPathWithinDirectory(workingDir, m_projectRoot);
                if (withinProject)
                {
                    QDir dir(workingDir);
                    if (dir.exists() && !dir.removeRecursively())
                    {
                        QMessageBox::warning(
                            this, tr("移除总成"),
                            tr("无法删除总成目录：%1")
                                .arg(QDir::toNativeSeparators(workingDir)));
                    }
                }
            }

            refreshNavigation();
            appendLogMessage(tr("已移除总成"));
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
                ModelRecord model = scheme.models.takeAt(j);
                if (m_activeModelId == id)
                    m_activeModelId.clear();
                persistSchemes();
                refreshNavigation(scheme.id);
                updateModelImagePreview(nullptr);
                const QString modelDir = model.directory;
                if (!modelDir.isEmpty())
                {
                    const bool withinProject =
                        isPathWithinDirectory(modelDir, scheme.workingDirectory) ||
                        isPathWithinDirectory(modelDir, m_projectRoot);
                    if (withinProject)
                    {
                        QDir dir(modelDir);
                        if (dir.exists() && !dir.removeRecursively())
                        {
                            QMessageBox::warning(
                                this, tr("移除模型"),
                                tr("无法删除模型目录：%1")
                                    .arg(QDir::toNativeSeparators(modelDir)));
                        }
                    }
                }
                appendLogMessage(tr("已移除模型"));
                return;
            }
        }
    }
}

bool MainWindow::removeModelsByFingerprint(SchemeRecord& scheme,
                                           const QSet<QString>& fingerprints)
{
    if (fingerprints.isEmpty())
        return false;

    bool removedAny = false;
    for (int i = scheme.models.size() - 1; i >= 0; --i)
    {
        ModelRecord& model = scheme.models[i];
        QString fingerprint = model.fingerprint;
        if (fingerprint.isEmpty())
            fingerprint = computeModelFingerprint(model.jsonPath);
        if (fingerprint.isEmpty() || !fingerprints.contains(fingerprint))
            continue;

        if (m_activeModelId == model.id)
            m_activeModelId.clear();
        scheme.models.removeAt(i);
        removedAny = true;
    }

    return removedAny;
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
    QList<QTreeWidgetItem*> schemeItems;
    if (m_projectRootItem)
    {
        const int childCount = m_projectRootItem->childCount();
        for (int i = 0; i < childCount; ++i)
        {
            if (QTreeWidgetItem* child = m_projectRootItem->child(i))
            {
                if (child->data(0, TypeRole).toInt() == SchemeItem)
                    schemeItems.append(child);
            }
        }
    }
    else
    {
        const int topCount = ui->treeModels->topLevelItemCount();
        for (int i = 0; i < topCount; ++i)
        {
            QTreeWidgetItem* schemeItem = ui->treeModels->topLevelItem(i);
            if (!schemeItem)
                continue;
            if (schemeItem->data(0, TypeRole).toInt() != SchemeItem)
                continue;
            schemeItems.append(schemeItem);
        }
    }

    for (QTreeWidgetItem* schemeItem : schemeItems)
    {
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

QString MainWindow::projectDisplayName() const
{
    if (!hasActiveProject())
        return tr("Untitled Project");
    return projectDisplayName(m_projectRoot);
}

QString MainWindow::projectDisplayName(const QString& projectPath) const
{
    const QString trimmed = projectPath.trimmed();
    if (trimmed.isEmpty())
        return tr("Untitled Project");

    QFileInfo info(trimmed);
    QString name = info.fileName();
    if (name.isEmpty())
    {
        QDir dir(trimmed);
        name = dir.dirName();
    }
    if (name.isEmpty())
        name = QDir::toNativeSeparators(trimmed);
    return name;
}

QString MainWindow::makeUniqueName(const QString& desired, QSet<QString>& taken,
                                   const QString& fallback) const
{
    QString base = desired.trimmed();
    if (base.isEmpty())
        base = fallback;

    QString candidate = base;
    QString key = candidate.trimmed().toLower();
    int index = 2;
    while (taken.contains(key))
    {
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(index++);
        key = candidate.trimmed().toLower();
    }
    taken.insert(key);
    return candidate;
}

QString MainWindow::makeUniqueSchemeName(const QString& desired,
                                         const QString& excludeId) const
{
    QSet<QString> taken;
    for (const SchemeRecord& scheme : m_schemes)
    {
        if (scheme.id == excludeId)
            continue;
        taken.insert(scheme.name.trimmed().toLower());
    }
    return makeUniqueName(desired, taken, tr("Untitled Assembly"));
}

QString MainWindow::makeUniqueModelName(const SchemeRecord& scheme,
                                        const QString& desired,
                                        const QString& excludeId) const
{
    QSet<QString> taken;
    for (const ModelRecord& model : scheme.models)
    {
        if (model.id == excludeId)
            continue;
        taken.insert(model.name.trimmed().toLower());
    }
    return makeUniqueName(desired, taken, tr("Untitled Model"));
}

void MainWindow::ensureUniqueModelNames(SchemeRecord& scheme) const
{
    QSet<QString> taken;
    for (ModelRecord& model : scheme.models)
        model.name = makeUniqueName(model.name, taken, tr("Untitled Model"));
}

void MainWindow::ensureUniqueSchemeAndModelNames()
{
    QSet<QString> taken;
    for (SchemeRecord& scheme : m_schemes)
    {
        scheme.name = makeUniqueName(scheme.name, taken, tr("Untitled Assembly"));
        ensureUniqueModelNames(scheme);
    }
}

void MainWindow::updateToolbarState()
{
    if (ui->treeModels)
        ui->treeModels->setEnabled(true);
}

void MainWindow::setVisualizationVisible(bool visible)
{
    if (!ui->vtkPanel || !ui->logPanel || !ui->logTextEdit || !ui->contentSplitter)
        return;

    if (m_visualizationVisible == visible)
        return;

    m_visualizationVisible = visible;

    if (visible)
    {
        ui->vtkPanel->setVisible(true);
        ui->logPanel->setVisible(true);
        ui->logTitle->setVisible(true);
        ui->logTextEdit->setVisible(true);

        if (!m_lastSplitterSizes.isEmpty())
        {
            ui->contentSplitter->setSizes(m_lastSplitterSizes);
        }
        else
        {
            QList<int> sizes = ui->contentSplitter->sizes();
            if (sizes.size() < 2 || (sizes.at(0) == 0 && sizes.at(1) == 0))
            {
                sizes.clear();
                sizes << 1 << 1;
            }
            ui->contentSplitter->setSizes(sizes);
        }

        if (ui->visualizationSplitter)
        {
            QList<int> vizSizes = ui->visualizationSplitter->sizes();
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
                ui->visualizationSplitter->setSizes(vizSizes);
            }
        }
    }
    else
    {
        m_lastSplitterSizes = ui->contentSplitter->sizes();

        ui->vtkPanel->setVisible(false);
        ui->logPanel->setVisible(false);
        ui->logTitle->setVisible(false);
        ui->logTextEdit->setVisible(false);

        QList<int> sizes = ui->contentSplitter->sizes();
        if (sizes.size() >= 2)
        {
            const int total = std::max(1, sizes.value(0) + sizes.value(1));
            sizes[0] = total;
            sizes[1] = 0;
            ui->contentSplitter->setSizes(sizes);
        }
    }
}

void MainWindow::updateSelectionInfo(const QString& path, const QString& remark)
{
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

void MainWindow::updateModelImagePreview(const ModelRecord* model)
{
    if (!ui->modelImageLabel)
        return;

    QPixmap pixmap;
    QString message = tr("尚未选择模型图片");

    if (!model)
    {
        message = tr("请选择模型以查看图片");
    }
    else
    {
        pixmap = loadModelThumbnail(*model);
        if (!pixmap.isNull())
            message.clear();
        else
            message = tr("此模型尚未设置图片");
    }

    m_currentModelThumbnail = pixmap;

    if (pixmap.isNull())
    {
        ui->modelImageLabel->setPixmap(QPixmap());
        ui->modelImageLabel->setText(message);
        ui->modelImageLabel->setToolTip(QString());
    }
    else
    {
        ui->modelImageLabel->setText(QString());
        ui->modelImageLabel->setToolTip(model && !model->thumbnailPath.isEmpty()
                                            ? QDir::toNativeSeparators(model->thumbnailPath)
                                            : QString());
        refreshModelImagePreview();
        if (ui->previewTabs && ui->imagePreviewTab)
            ui->previewTabs->setCurrentWidget(ui->imagePreviewTab);
    }
}

void MainWindow::refreshModelImagePreview()
{
    if (!ui->modelImageLabel)
        return;

    if (m_currentModelThumbnail.isNull())
        return;

    const QSize labelSize = ui->modelImageLabel->size();
    if (labelSize.width() <= 0 || labelSize.height() <= 0)
    {
        ui->modelImageLabel->setPixmap(m_currentModelThumbnail);
        return;
    }

    const qreal ratio = ui->modelImageLabel->devicePixelRatioF();
    QSize targetSize(qMax(1, int(labelSize.width() * ratio)),
                     qMax(1, int(labelSize.height() * ratio)));
    QPixmap scaled = m_currentModelThumbnail.scaled(targetSize, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(ratio);
    ui->modelImageLabel->setPixmap(scaled);
}

void MainWindow::displayResultFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return;

    QFileInfo info(filePath);
    if (!info.exists())
    {
        appendLogMessage(tr("未找到结果文件：%1")
                             .arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    vtkSmartPointer<vtkActor> actor;
    const QString suffix = info.suffix().toLower();

    if (suffix == QStringLiteral("obj"))
    {
        auto reader = vtkSmartPointer<vtkOBJReader>::New();
        reader->SetFileName(info.absoluteFilePath().toUtf8().constData());
        reader->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(reader->GetOutputPort());

        actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.9, 0.9, 0.9); // 浅灰色
    }
    else if (suffix == QStringLiteral("stl"))
    {
        auto reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(info.absoluteFilePath().toUtf8().constData());
        reader->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(reader->GetOutputPort());

        actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.9, 0.9, 0.9);
    }
    else
    {
        appendLogMessage(tr("不支持的结果文件类型：%1")
                             .arg(QDir::toNativeSeparators(info.absoluteFilePath())));
        return;
    }

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
    QVector<SchemeRecord> loaded;
    QString remarks;
    QDateTime createdAt;
    QDateTime updatedAt;
    QString workspace;

    if (!readProjectStorage(m_projectRoot, m_storageFilePath, &loaded,
                            &remarks, &createdAt, &updatedAt, &workspace))
        return false;

    m_projectRemarks = remarks;
    m_projectCreatedAt = createdAt;
    m_projectUpdatedAt = updatedAt;
    m_workspaceRoot = workspace;

    if (m_workspaceRoot.isEmpty() && !m_projectRoot.isEmpty())
    {
        QDir projectDir(m_projectRoot);
        const QString fallback = projectDir.filePath(QStringLiteral("workspaces"));
        ensureDirectoryExists(fallback);
        m_workspaceRoot = canonicalPathForDir(QDir(fallback));
        if (m_workspaceRoot.isEmpty())
            m_workspaceRoot = QDir::cleanPath(fallback);
    }

    if (!m_workspaceRoot.isEmpty())
        ensureDirectoryExists(m_workspaceRoot);

    m_schemes = loaded;
    ensureUniqueSchemeAndModelNames();
    return true;
}

bool MainWindow::readProjectStorage(const QString& projectRoot,
                                    const QString& storageFile,
                                    QVector<SchemeRecord>* schemes,
                                    QString* remarks,
                                    QDateTime* createdAt,
                                    QDateTime* updatedAt,
                                    QString* workspaceRoot) const
{
    if (storageFile.isEmpty())
        return false;

    QFile file(storageFile);
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
    const QJsonObject projectObj = root.value(QStringLiteral("project")).toObject();

    if (remarks)
        *remarks = projectObj.value(QStringLiteral("remarks")).toString();

    const auto parseIsoDate = [](const QString& value) -> QDateTime {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty())
            return QDateTime();
        QDateTime dt = QDateTime::fromString(trimmed, Qt::ISODateWithMs);
        if (!dt.isValid())
            dt = QDateTime::fromString(trimmed, Qt::ISODate);
        return dt;
    };

    QDateTime created =
        parseIsoDate(projectObj.value(QStringLiteral("createdAt")).toString());
    QDateTime updated =
        parseIsoDate(projectObj.value(QStringLiteral("updatedAt")).toString());

    QFileInfo storageInfo(storageFile);
    if (!created.isValid())
    {
        QDateTime birth = storageInfo.birthTime();
        if (!birth.isValid())
            birth = storageInfo.created();
        if (!birth.isValid())
            birth = storageInfo.lastModified();
        created = birth;
    }
    if (!updated.isValid())
        updated = storageInfo.lastModified();

    if (createdAt)
        *createdAt = created;
    if (updatedAt)
        *updatedAt = updated;

    QString workspace;
    const QString storedRoot = root.value(QStringLiteral("workspaceRoot")).toString().trimmed();
    if (!storedRoot.isEmpty())
    {
        QDir rootDir(storedRoot);
        if (rootDir.isAbsolute())
        {
            workspace = canonicalPathForDir(rootDir);
            if (workspace.isEmpty())
                workspace = QDir::cleanPath(storedRoot);
        }
        else if (!projectRoot.isEmpty())
        {
            QDir projectDir(projectRoot);
            const QString absolute = projectDir.filePath(storedRoot);
            workspace = canonicalPathForDir(QDir(absolute));
            if (workspace.isEmpty())
                workspace = QDir::cleanPath(absolute);
        }
        else
        {
            workspace = canonicalPathForDir(QDir(storedRoot));
            if (workspace.isEmpty())
                workspace = QDir::cleanPath(storedRoot);
        }
    }

    if (workspaceRoot)
        *workspaceRoot = workspace;

    QVector<SchemeRecord> parsedSchemes;
    const QJsonArray schemeArray = root.value(QStringLiteral("schemes")).toArray();
    for (const QJsonValue& value : schemeArray)
    {
        const QJsonObject obj = value.toObject();
        SchemeRecord scheme;
        scheme.id = obj.value(QStringLiteral("id")).toString();
        if (scheme.id.isEmpty())
            scheme.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        scheme.name = obj.value(QStringLiteral("name")).toString();
        scheme.libraryId = obj.value(QStringLiteral("libraryId")).toString().trimmed();
        scheme.workingDirectory = canonicalPathForDir(
            QDir(obj.value(QStringLiteral("workingDirectory")).toString()));
        if (scheme.workingDirectory.isEmpty())
            continue;

        const QString storedThumb = obj.value(QStringLiteral("thumbnailPath")).toString().trimmed();
        if (!storedThumb.isEmpty())
            scheme.thumbnailPath = QDir::cleanPath(QFileInfo(storedThumb).absoluteFilePath());
        scheme.remarks = obj.value(QStringLiteral("remarks")).toString();

        const QJsonArray modelArray = obj.value(QStringLiteral("models")).toArray();
        for (const QJsonValue& mv : modelArray)
        {
            const QJsonObject mo = mv.toObject();
            ModelRecord model;
            model.id = mo.value(QStringLiteral("id")).toString();
            if (model.id.isEmpty())
                model.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            model.name = mo.value(QStringLiteral("name")).toString();
            model.directory = canonicalPathForDir(
                QDir(mo.value(QStringLiteral("directory")).toString()));
            model.jsonPath = QDir::cleanPath(mo.value(QStringLiteral("jsonPath")).toString());
            model.batPath = QDir::cleanPath(mo.value(QStringLiteral("batPath")).toString());
            model.remarks = mo.value(QStringLiteral("remarks")).toString();
            const QString storedModelThumb = mo.value(QStringLiteral("thumbnailPath")).toString().trimmed();
            if (!storedModelThumb.isEmpty())
                model.thumbnailPath = QDir::cleanPath(QFileInfo(storedModelThumb).absoluteFilePath());
            model.fingerprint = mo.value(QStringLiteral("fingerprint")).toString();
            if (model.fingerprint.isEmpty())
                model.fingerprint = computeModelFingerprint(model.jsonPath);
            if (model.directory.isEmpty() || model.jsonPath.isEmpty())
                continue;
            scheme.models.push_back(model);
        }

        parsedSchemes.push_back(scheme);
    }

    if (schemes)
        *schemes = parsedSchemes;

    return true;
}

QVector<MainWindow::SchemeRecord> MainWindow::loadProjectPreviewSchemes(const QString& projectPath) const
{
    QVector<SchemeRecord> result;
    const QString trimmed = projectPath.trimmed();
    if (trimmed.isEmpty())
        return result;

    QDir projectDir(trimmed);
    const QString storageFile = projectDir.filePath(QStringLiteral("schemes.json"));
    if (!readProjectStorage(trimmed, storageFile, &result, nullptr, nullptr, nullptr, nullptr))
        result.clear();
    return result;
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
        obj.insert(QStringLiteral("libraryId"), scheme.libraryId);
        obj.insert(QStringLiteral("workingDirectory"), scheme.workingDirectory);
        obj.insert(QStringLiteral("thumbnailPath"), scheme.thumbnailPath);
        obj.insert(QStringLiteral("remarks"), scheme.remarks);

        QJsonArray modelArray;
        for (const ModelRecord& model : scheme.models)
        {
            QJsonObject mo;
            mo.insert(QStringLiteral("id"), model.id);
            mo.insert(QStringLiteral("name"), model.name);
            mo.insert(QStringLiteral("directory"), model.directory);
            mo.insert(QStringLiteral("jsonPath"), model.jsonPath);
            mo.insert(QStringLiteral("batPath"), model.batPath);
            QString fingerprint = model.fingerprint.isEmpty()
                                     ? computeModelFingerprint(model.jsonPath)
                                     : model.fingerprint;
            mo.insert(QStringLiteral("fingerprint"), fingerprint);
            mo.insert(QStringLiteral("remarks"), model.remarks);
            mo.insert(QStringLiteral("thumbnailPath"), model.thumbnailPath);
            modelArray.append(mo);
        }
        obj.insert(QStringLiteral("models"), modelArray);
        schemeArray.append(obj);
    }

    QJsonObject root;
    QString workspaceToStore = m_workspaceRoot;
    if (!m_projectRoot.isEmpty())
    {
        QDir projectDir(m_projectRoot);
        const QString relative = projectDir.relativeFilePath(m_workspaceRoot);
        if (!relative.startsWith(QStringLiteral("..")) && !relative.startsWith(QLatin1Char('/')))
            workspaceToStore = relative;
    }
    QJsonObject projectObj;
    projectObj.insert(QStringLiteral("remarks"), m_projectRemarks);
    if (m_projectCreatedAt.isValid())
        projectObj.insert(QStringLiteral("createdAt"),
                          m_projectCreatedAt.toUTC().toString(Qt::ISODateWithMs));
    if (m_projectUpdatedAt.isValid())
        projectObj.insert(QStringLiteral("updatedAt"),
                          m_projectUpdatedAt.toUTC().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("workspaceRoot"), workspaceToStore);
    root.insert(QStringLiteral("project"), projectObj);
    root.insert(QStringLiteral("schemes"), schemeArray);

    QFile file(m_storageFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::persistSchemes()
{
    if (hasActiveProject())
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (!m_projectCreatedAt.isValid())
            m_projectCreatedAt = now;
        m_projectUpdatedAt = now;
    }
    saveSchemesToStorage();
}

bool MainWindow::renameLibrarySchemesInProjects(const QString& libraryId,
                                               const QString& newName)
{
    const QString trimmedId = libraryId.trimmed();
    const QString trimmedName = newName.trimmed();
    if (trimmedId.isEmpty() || trimmedName.isEmpty())
        return false;

    const QString activeRoot = m_projectRoot;
    bool anyUpdated = false;

    for (const QString& projectPath : m_recentProjects)
    {
        const QString trimmedPath = projectPath.trimmed();
        if (trimmedPath.isEmpty())
            continue;

        QString canonical = canonicalPathForDir(QDir(trimmedPath));
        if (canonical.isEmpty())
            canonical = QDir::cleanPath(trimmedPath);

        if (!activeRoot.isEmpty() &&
            (canonical.compare(activeRoot, Qt::CaseInsensitive) == 0))
        {
            continue;
        }

        QDir projectDir(canonical);
        if (!projectDir.exists())
            continue;

        const QString storageFile = projectDir.filePath(QStringLiteral("schemes.json"));
        QFile file(storageFile);
        if (!file.exists())
            continue;
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        const QByteArray data = file.readAll();
        file.close();

        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        QJsonObject root = doc.object();
        QJsonArray schemeArray = root.value(QStringLiteral("schemes")).toArray();
        bool modified = false;
        for (int i = 0; i < schemeArray.size(); ++i)
        {
            QJsonObject schemeObj = schemeArray.at(i).toObject();
            const QString storedId =
                schemeObj.value(QStringLiteral("libraryId")).toString().trimmed();
            if (storedId.compare(trimmedId, Qt::CaseInsensitive) != 0)
                continue;

            if (schemeObj.value(QStringLiteral("name")).toString() == trimmedName)
                continue;

            schemeObj.insert(QStringLiteral("name"), trimmedName);
            schemeArray.replace(i, schemeObj);
            modified = true;
        }

        if (!modified)
            continue;

        root.insert(QStringLiteral("schemes"), schemeArray);

        QJsonObject projectObj = root.value(QStringLiteral("project")).toObject();
        projectObj.insert(QStringLiteral("updatedAt"),
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        root.insert(QStringLiteral("project"), projectObj);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            continue;

        QJsonDocument outDoc(root);
        file.write(outDoc.toJson(QJsonDocument::Indented));
        file.close();

        anyUpdated = true;
    }

    return anyUpdated;
}

QString MainWindow::makeUniqueWorkspaceSubdir(const QString& baseName) const
{
    const QString root = workspaceRoot();
    if (root.isEmpty())
        return QString();

    QDir base(root);
    if (!base.exists())
        ensureDirectoryExists(root);
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
