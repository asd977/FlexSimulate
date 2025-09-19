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
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QScrollArea>
#include <QSet>
#include <QShortcut>
#include <QSplitter>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

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
    m_baseWindowTitle = windowTitle();

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dataDir(dataRoot);
    if (!dataDir.exists())
        dataDir.mkpath(QStringLiteral("."));
    m_appStateFilePath = dataDir.filePath(QStringLiteral("app_state.json"));

    setupUiHelpers();
    setupConnections();
    loadSchemeLibrary();
    loadInitialSchemes();
}

MainWindow::~MainWindow()
{
    saveSchemesToStorage();
    saveSchemeLibrary();
    saveApplicationState();
    delete ui;
}

void MainWindow::setupUiHelpers()
{
    m_galleryWidget = new SchemeGalleryWidget(this);
    ui->planPageLayout->addWidget(m_galleryWidget);

    auto* detailLayout = new QVBoxLayout(ui->settingWidget);
    detailLayout->setContentsMargins(12, 12, 12, 12);
    detailLayout->setSpacing(12);

    const QString sectionTitleStyle = QStringLiteral(
        "font-size:15px;font-weight:600;color:#0f172a;"
        "background:#e2e8f0;border-radius:8px;padding:6px 12px;");
    const auto applySectionStyle = [&](QLabel* label) {
        if (label)
            label->setStyleSheet(sectionTitleStyle);
    };
    applySectionStyle(ui->navigationTitle);
    applySectionStyle(ui->detailTitle);
    applySectionStyle(ui->vtkTitle);
    applySectionStyle(ui->logTitle);

    if (ui->projectTitleLabel)
        ui->projectTitleLabel->setText(tr("未打开工程"));
    if (ui->projectBadge)
        ui->projectBadge->setToolTip(tr("请选择或创建工程"));

    ui->treeModels->header()->setStretchLastSection(true);
    ui->treeModels->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->treeModels->setEditTriggers(QAbstractItemView::EditKeyPressed |
                                    QAbstractItemView::SelectedClicked);

    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    ui->contentSplitter->setStretchFactor(0, 0);
    ui->contentSplitter->setStretchFactor(1, 1);
    ui->contentSplitter->setCollapsible(1, true);
    if (ui->visualizationSplitter)
    {
        ui->visualizationSplitter->setStretchFactor(0, 3);
        ui->visualizationSplitter->setStretchFactor(1, 1);
        ui->visualizationSplitter->setHandleWidth(6);
    }

    ui->logTextEdit->setStyleSheet(
        "QPlainTextEdit{background:#0f172a;color:#f8fafc;border-radius:6px;padding:6px;}"
    );

    setVisualizationVisible(false);
    updateSelectionInfo();

    auto colors = vtkSmartPointer<vtkNamedColors>::New();
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderer->SetBackground(colors->GetColor3d("AliceBlue").GetData());
    m_renderWindow->AddRenderer(m_renderer);
    ui->vtkWidget->setRenderWindow(m_renderWindow);
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
    connect(m_galleryWidget, &SchemeGalleryWidget::schemeAddRequested,
            this, &MainWindow::onGalleryAddRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::schemeDeleteRequested,
            this, &MainWindow::onGalleryDeleteRequested);
    connect(m_galleryWidget, &SchemeGalleryWidget::createSchemeRequested,
            this, &MainWindow::onAddLibraryScheme);

    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, ui->treeModels);
    connect(deleteShortcut, &QShortcut::activated,
            this, &MainWindow::deleteCurrentTreeItem);
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

    QSet<QString> seen;
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
                if (canonical.isEmpty() || seen.contains(canonical))
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

                m_librarySchemes.push_back(entry);
                seen.insert(canonical);
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
            if (canonical.isEmpty() || seen.contains(canonical))
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

            m_librarySchemes.push_back(entry);
            seen.insert(canonical);
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
        }
    }

    if (!lastProject.isEmpty() && openProjectAt(lastProject, /*silent*/true))
        return;

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

    QFile file(m_appStateFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void MainWindow::enterProjectlessState()
{
    m_projectRoot.clear();
    m_workspaceRoot.clear();
    m_storageFilePath.clear();
    m_activeSchemeId.clear();
    m_activeModelId.clear();
    m_schemes.clear();
    m_schemeItems.clear();
    m_modelItems.clear();
    m_projectRootItem = nullptr;
    m_libraryRootItem = nullptr;

    if (ui->treeModels)
        ui->treeModels->clear();
    if (m_galleryWidget)
        m_galleryWidget->clearSchemes();

    clearDetailWidget();
    clearVtkScene();
    setVisualizationVisible(false);
    updateSelectionInfo();

    if (ui->stackedWidget && ui->welcomePage)
        ui->stackedWidget->setCurrentWidget(ui->welcomePage);

    updateWindowTitle();
    updateToolbarState();
    updateGallery();
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

    if (canonicalProject == m_projectRoot)
    {
        refreshNavigation();
        ui->stackedWidget->setCurrentWidget(ui->planPage);
        updateToolbarState();
        updateWindowTitle();
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

    m_schemes.clear();
    if (!loadSchemesFromStorage())
    {
        m_schemes.clear();
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
    const QString base = m_baseWindowTitle.isEmpty()
                             ? tr("柔性仿真软件")
                             : m_baseWindowTitle;
    if (!hasActiveProject())
    {
        setWindowTitle(base);
        if (ui->projectTitleLabel)
            ui->projectTitleLabel->setText(tr("未打开工程"));
        if (ui->projectBadge)
            ui->projectBadge->setToolTip(tr("请选择或创建工程"));
        return;
    }

    const QString projectName = projectDisplayName();
    setWindowTitle(QStringLiteral("%1 - %2").arg(base, projectName));
    if (ui->projectTitleLabel)
        ui->projectTitleLabel->setText(projectName);
    if (ui->projectBadge)
        ui->projectBadge->setToolTip(QDir::toNativeSeparators(m_projectRoot));
}

void MainWindow::onNewProjectTriggered()
{
    const QString baseDir = QFileDialog::getExistingDirectory(
        this, tr("选择工程位置"), QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (baseDir.isEmpty())
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("新建工程"), tr("工程名称："), QLineEdit::Normal,
        tr("新工程"), &ok);
    if (!ok)
        return;

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty())
    {
        QMessageBox::warning(this, tr("新建工程"), tr("工程名称不能为空。"));
        return;
    }

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

    if (!ensureProjectStructure(projectPath))
    {
        QMessageBox::warning(this, tr("新建工程"),
                             tr("无法创建工程目录：%1")
                                 .arg(QDir::toNativeSeparators(projectPath)));
        return;
    }

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
    const QString defaultName = tr("新方案%1").arg(m_librarySchemes.size() + 1);
    SchemeSettingsDialog dlg(defaultName, QString(), false, this);
    dlg.setDirectoryHint(tr("方案库目录将在软件运行目录中自动生成"));
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = dlg.schemeName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("创建方案库"), tr("方案名称不能为空"));
        return;
    }

    QString directory = makeUniqueLibrarySubdir(name);
    if (directory.isEmpty())
    {
        QMessageBox::warning(this, tr("创建方案库"), tr("无法创建方案库目录"));
        return;
    }

    if (!ensureDirectoryExists(directory))
    {
        QMessageBox::warning(this, tr("创建方案库"),
                             tr("无法创建目录：%1")
                                 .arg(QDir::toNativeSeparators(directory)));
        return;
    }

    QString templatePath;
    const QVector<QPair<QString, QString>> templates = availableSchemeTemplates();
    if (!templates.isEmpty())
    {
        QStringList options;
        options << tr("空白方案");
        for (const auto& info : templates)
            options << info.first;

        bool ok = true;
        const QString choice = QInputDialog::getItem(
            this, tr("方案模板"), tr("请选择通用方案模板："), options, 0, false, &ok);
        if (!ok)
        {
            QDir(directory).removeRecursively();
            return;
        }
        if (choice != options.first())
        {
            for (const auto& info : templates)
            {
                if (info.first == choice)
                {
                    templatePath = info.second;
                    break;
                }
            }
        }
    }

    if (!templatePath.isEmpty())
    {
        if (!copyDirectoryRecursively(templatePath, directory))
        {
            QDir(directory).removeRecursively();
            QMessageBox::warning(this, tr("创建方案库"),
                                 tr("无法复制模板目录：%1")
                                     .arg(QDir::toNativeSeparators(templatePath)));
            return;
        }
    }

    const QString canonical = canonicalPathForDir(QDir(directory));
    if (canonical.isEmpty())
    {
        QDir(directory).removeRecursively();
        QMessageBox::warning(this, tr("创建方案库"), tr("无法解析方案库目录"));
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
    appendLogMessage(tr("已创建方案库 %1").arg(name));
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
        showSchemeSettings(schemeId);
        clearVtkScene();
        setVisualizationVisible(false);

        if (SchemeRecord* scheme = schemeById(schemeId))
            updateSelectionInfo(scheme->workingDirectory, scheme->remarks);
        else
            updateSelectionInfo();
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
        m_activeSchemeId.clear();
        m_activeModelId.clear();
        clearDetailWidget();
        clearVtkScene();
        setVisualizationVisible(false);
        updateSelectionInfo(m_projectRoot, QString());
    }
    else
    {
        updateSelectionInfo();
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
            const QString trimmed = item->text(0).trimmed();
            if (trimmed.isEmpty())
            {
                QMessageBox::warning(this, tr("重命名方案"), tr("方案名称不能为空。"));
                restoreText(scheme->name);
                return;
            }

            const QString unique = makeUniqueSchemeName(trimmed, scheme->id);
            if (unique.compare(trimmed, Qt::CaseSensitive) != 0)
            {
                QMessageBox::warning(this, tr("重命名方案"), tr("已存在同名方案，请输入其他名称。"));
                restoreText(scheme->name);
                return;
            }

            if (item->text(0) != trimmed)
            {
                QScopedValueRollback<bool> guard(m_blockTreeSignals, true);
                item->setText(0, trimmed);
            }

            scheme->name = trimmed;
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
                QMessageBox::warning(this, tr("重命名模型"), tr("该方案下已存在同名模型。"));
                restoreText(model->name);
                return;
            }

            if (item->text(0) != trimmed)
            {
                QScopedValueRollback<bool> guard(m_blockTreeSignals, true);
                item->setText(0, trimmed);
            }

            model->name = trimmed;
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
        if (type == LibraryItem)
        {
            menu.addAction(tr("查看方案库"), this, [this]() {
                ui->stackedWidget->setCurrentWidget(ui->planPage);
                updateGallery();
            });
            if (hasActiveProject())
            {
                menu.addSeparator();
                menu.addAction(tr("导入方案"), this, &MainWindow::promptAddScheme);
            }
        }
        else if (type == ProjectItem)
        {
            if (!m_projectRoot.isEmpty())
            {
                menu.addAction(tr("打开工程目录"), this, [this]() {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(m_projectRoot));
                });
            }
            menu.addAction(tr("导入方案"), this, &MainWindow::promptAddScheme);
        }
        else if (type == SchemeItem)
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
                if (SchemeRecord* scheme = schemeById(schemeId))
                {
                    if (confirmSchemeDeletion(*scheme))
                        removeSchemeById(schemeId);
                }
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
                SchemeRecord* owner = nullptr;
                if (ModelRecord* model = modelById(modelId, &owner))
                {
                    if (owner && confirmModelDeletion(*model, *owner))
                        removeModelById(modelId);
                }
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
        else if (type == ProjectItem || type == LibraryItem)
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

void MainWindow::onGalleryAddRequested(const QString& id)
{
    const SchemeLibraryEntry* entry = libraryEntryById(id);
    if (!entry)
        return;

    if (!hasActiveProject())
    {
        QMessageBox::information(this, tr("添加方案"), tr("请先新建或打开工程。"));
        return;
    }

    if (entry->directory.isEmpty() || !QDir(entry->directory).exists())
    {
        QMessageBox::warning(this, tr("添加方案"), tr("方案库目录不存在或不可访问。"));
        return;
    }

    const QString entryName = entry->name.isEmpty() ? tr("未命名方案") : entry->name;

    QString targetDir = makeUniqueWorkspaceSubdir(entryName);
    if (targetDir.isEmpty())
    {
        QMessageBox::warning(this, tr("添加方案"), tr("无法创建方案工作目录。"));
        return;
    }

    if (!copyDirectoryRecursively(entry->directory, targetDir))
    {
        QDir(targetDir).removeRecursively();
        QMessageBox::warning(this, tr("添加方案"),
                             tr("无法复制方案目录：%1")
                                 .arg(QDir::toNativeSeparators(entry->directory)));
        return;
    }

    const QString importedId = importSchemeFromDirectory(targetDir, false);
    if (importedId.isEmpty())
    {
        QDir(targetDir).removeRecursively();
        QMessageBox::warning(this, tr("添加方案"), tr("无法导入方案。"));
        return;
    }

    if (SchemeRecord* scheme = schemeById(importedId))
    {
        scheme->name = makeUniqueSchemeName(entryName, scheme->id);
        persistSchemes();
        refreshNavigation(importedId);
    }

    appendLogMessage(tr("已从方案库添加方案 %1").arg(entryName));
    ui->stackedWidget->setCurrentWidget(ui->MainPage);
    selectTreeItem(importedId, QString());
}

void MainWindow::onGalleryDeleteRequested(const QString& id)
{
    if (SchemeLibraryEntry* entry = libraryEntryById(id))
    {
        if (!entry->deletable)
        {
            QMessageBox::information(this, tr("删除方案库"), tr("此方案属于内置模板，无法删除。"));
            return;
        }
        const QString entryName = entry->name.isEmpty() ? tr("未命名方案") : entry->name;
        const QString text = tr("确定要从方案库中删除“%1”吗？").arg(entryName);
        if (QMessageBox::question(this, tr("删除方案库"), text,
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) == QMessageBox::Yes)
        {
            if (removeLibraryEntry(id))
            {
                updateGallery();
                appendLogMessage(tr("已删除方案库 %1").arg(entryName));
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

    const QIcon libraryIcon(QStringLiteral(":/icons/icons/gallery.svg"));
    const QIcon schemeIcon(QStringLiteral(":/icons/icons/plan.svg"));
    const QIcon modelIcon(QStringLiteral(":/icons/icons/model.svg"));

    m_libraryRootItem = new QTreeWidgetItem(ui->treeModels);
    m_libraryRootItem->setText(0, tr("方案库"));
    m_libraryRootItem->setIcon(0, libraryIcon);
    m_libraryRootItem->setData(0, TypeRole, LibraryItem);
    m_libraryRootItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    QTreeWidgetItem* schemeParent = ui->treeModels->invisibleRootItem();
    if (hasActiveProject())
    {
        m_projectRootItem = new QTreeWidgetItem(ui->treeModels);
        m_projectRootItem->setText(0, projectDisplayName());
        m_projectRootItem->setIcon(0, QIcon(QStringLiteral(":/icons/icons/project_logo.svg")));
        m_projectRootItem->setData(0, TypeRole, ProjectItem);
        m_projectRootItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
                                    Qt::ItemIsDropEnabled);
        schemeParent = m_projectRootItem;
    }

    for (const SchemeRecord& scheme : m_schemes)
    {
        QTreeWidgetItem* parentItem = schemeParent;
        if (!parentItem)
            parentItem = ui->treeModels->invisibleRootItem();
        auto* schemeItem = new QTreeWidgetItem(parentItem);
        schemeItem->setText(0, scheme.name);
        schemeItem->setIcon(0, schemeIcon);
        schemeItem->setData(0, TypeRole, SchemeItem);
        schemeItem->setData(0, IdRole, scheme.id);
        schemeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                             Qt::ItemIsDragEnabled | Qt::ItemIsEditable |
                             Qt::ItemIsDropEnabled);
        m_schemeItems.insert(scheme.id, schemeItem);

        ui->treeModels->expandItem(schemeItem);

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

    if (m_projectRootItem)
    {
        ui->treeModels->expandItem(m_projectRootItem);
    }
    else
    {
        ui->treeModels->expandAll();
    }
    m_blockTreeSignals = false;
}

void MainWindow::updateGallery()
{
    if (!m_galleryWidget)
        return;

    m_galleryWidget->clearSchemes();
    const bool hasProject = hasActiveProject();

    for (const SchemeLibraryEntry& entry : m_librarySchemes)
    {
        QPixmap thumb = loadLibraryThumbnail(entry);
        if (thumb.isNull())
            thumb = makeSchemePlaceholder(entry.name);

        SchemeGalleryWidget::CardOptions options;
        options.showAddButton = true;
        options.enableAddButton = hasProject;
        options.addToolTip = hasProject ? tr("添加到当前工程")
                                        : tr("请先新建或打开工程。");
        options.showDeleteButton = entry.deletable;
        options.enableDeleteButton = entry.deletable;
        options.deleteToolTip = entry.deletable
                                     ? tr("从方案库中删除此方案")
                                     : tr("内置模板不可删除");
        options.showOpenButton = true;
        options.enableOpenButton = true;
        options.openToolTip = tr("打开方案所在目录");
        options.hintText = tr("双击卡片添加到当前工程");

        m_galleryWidget->addScheme(entry.id, entry.name, thumb, options);
    }

    for (const SchemeRecord& scheme : m_schemes)
    {
        QPixmap thumb = loadSchemeThumbnail(scheme);
        if (thumb.isNull())
            thumb = makeSchemePlaceholder(scheme.name);

        SchemeGalleryWidget::CardOptions options;
        options.showAddButton = false;
        options.showOpenButton = true;
        options.openToolTip = tr("查看方案详情");
        options.hintText = tr("双击卡片查看详情");

        m_galleryWidget->addScheme(scheme.id, scheme.name, thumb, options);
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
    setVisualizationVisible(false);
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
        return;
    }

    clearDetailWidget();
    m_currentDetailWidget = buildModelSettingsWidget(*model);
    ui->settingWidget->layout()->addWidget(m_currentDetailWidget);
    setVisualizationVisible(true);
    updateSelectionInfo(model->directory, model->remarks);

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
    layout->setSpacing(12);

    auto* title = new QLabel(tr("方案：%1").arg(scheme.name), container);
    title->setStyleSheet("font-size:18px;font-weight:600;color:#0f172a;");
    layout->addWidget(title);

    auto* infoFrame = new QFrame(container);
    infoFrame->setObjectName("schemeInfoFrame");
    infoFrame->setStyleSheet(
        "QFrame#schemeInfoFrame{background:#f8fafc;border:1px solid #d0d5dd;border-radius:10px;}"
        "QLabel.infoCaption{color:#64748b;font-size:12px;}"
        "QLabel.infoValue{color:#0f172a;font-weight:500;}");
    auto* infoLayout = new QGridLayout(infoFrame);
    infoLayout->setContentsMargins(12, 12, 12, 12);
    infoLayout->setHorizontalSpacing(16);
    infoLayout->setVerticalSpacing(8);

    auto* pathCaption = new QLabel(tr("工作目录"), infoFrame);
    pathCaption->setObjectName("infoCaption");
    auto* pathValue = new QLabel(QDir::toNativeSeparators(scheme.workingDirectory), infoFrame);
    pathValue->setObjectName("infoValue");
    pathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathValue->setWordWrap(true);
    infoLayout->addWidget(pathCaption, 0, 0, Qt::AlignTop);
    infoLayout->addWidget(pathValue, 0, 1);

    auto* countCaption = new QLabel(tr("模型数量"), infoFrame);
    countCaption->setObjectName("infoCaption");
    auto* countValue = new QLabel(tr("%1 个").arg(scheme.models.size()), infoFrame);
    countValue->setObjectName("infoValue");
    infoLayout->addWidget(countCaption, 1, 0, Qt::AlignTop);
    infoLayout->addWidget(countValue, 1, 1, Qt::AlignTop);
    auto* remarkCaption = new QLabel(tr("备注"), infoFrame);
    remarkCaption->setObjectName("infoCaption");
    QString remarkText = scheme.remarks.trimmed();
    if (remarkText.isEmpty())
        remarkText = tr("暂无备注");
    auto* remarkValue = new QLabel(remarkText, infoFrame);
    remarkValue->setObjectName("infoValue");
    remarkValue->setWordWrap(true);
    remarkValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(remarkCaption, 2, 0, Qt::AlignTop);
    infoLayout->addWidget(remarkValue, 2, 1, Qt::AlignTop);
    infoLayout->setColumnStretch(1, 1);

    layout->addWidget(infoFrame);

    auto* hint = new QLabel(tr("可将模型文件夹拖到左侧树中，或使用下方按钮导入模型。"), container);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#64748b;");
    layout->addWidget(hint);

    auto* listFrame = new QFrame(container);
    listFrame->setObjectName("modelListFrame");
    listFrame->setStyleSheet(
        "QFrame#modelListFrame{background:#ffffff;border:1px solid #d0d5dd;border-radius:10px;}"
        "QListWidget#modelList{border:none;background:transparent;}"
        "QListWidget#modelList::item{padding:10px;border-radius:8px;}"
        "QListWidget#modelList::item:hover{background:rgba(23,135,255,0.08);}");
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
    list->setSelectionMode(QAbstractItemView::NoSelection);
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
            list->addItem(item);
        }
    }

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

    auto* openBtn = new QPushButton(tr("打开方案目录"), container);
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
    layout->setSpacing(8);

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
    openBtn->setCursor(Qt::PointingHandCursor);
    openBtn->setStyleSheet(
        "QPushButton{padding:8px 18px;border-radius:18px;"
        "border:1px solid #cbd5f5;background:#f8faff;color:#1d4ed8;}"
        "QPushButton:hover{background:#e0e7ff;}"
        "QPushButton:pressed{background:#bfdbfe;}"
    );
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
            model.name = makeUniqueModelName(*scheme, destDir.dirName());
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
            model.name = makeUniqueModelName(*scheme, model.name);

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

    QSet<QString> taken;
    for (ModelRecord& model : models)
        model.name = makeUniqueName(model.name, taken, tr("未命名模型"));
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

QVector<QPair<QString, QString>> MainWindow::availableSchemeTemplates() const
{
    QVector<QPair<QString, QString>> templates;
    QSet<QString> seen;
    for (const QString& rootPath : templateSearchRoots())
    {
        QDir root(rootPath);
        if (!root.exists())
            continue;

        const QFileInfoList entries =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& info : entries)
        {
            const QString canonical = canonicalPathForDir(QDir(info.absoluteFilePath()));
            if (canonical.isEmpty() || seen.contains(canonical))
                continue;
            templates.append({info.fileName(), canonical});
            seen.insert(canonical);
        }
    }
    for (const SchemeLibraryEntry& entry : m_librarySchemes)
    {
        if (entry.directory.isEmpty())
            continue;
        if (seen.contains(entry.directory))
            continue;
        templates.append({entry.name, entry.directory});
        seen.insert(entry.directory);
    }
    return templates;
}

QStringList MainWindow::templateSearchRoots() const
{
    return {
        QDir::current().absoluteFilePath(QStringLiteral("sample_data")),
        QCoreApplication::applicationDirPath() + QStringLiteral("/sample_data"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../sample_data")
    };
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
        sanitized = QStringLiteral("Scheme");
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

void MainWindow::promptAddScheme()
{
    if (!hasActiveProject())
    {
        QMessageBox::information(this, tr("创建方案"), tr("请先新建或打开工程。"));
        return;
    }

    const QString defaultName = tr("新方案%1").arg(m_schemes.size() + 1);
    SchemeSettingsDialog dlg(defaultName, QString(), false, this);
    dlg.setDirectoryHint(tr("工作目录将在工程中自动生成"));
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = dlg.schemeName();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("创建方案"), tr("方案名称不能为空"));
        return;
    }

    QString directory = makeUniqueWorkspaceSubdir(name);
    if (directory.isEmpty())
    {
        QMessageBox::warning(this, tr("创建方案"), tr("无法创建方案工作目录"));
        return;
    }

    if (!ensureDirectoryExists(directory))
    {
        QMessageBox::warning(this, tr("创建方案"),
                             tr("无法创建工作目录：%1")
                                 .arg(QDir::toNativeSeparators(directory)));
        return;
    }

    QString templatePath;
    const QVector<QPair<QString, QString>> templates = availableSchemeTemplates();
    if (!templates.isEmpty())
    {
        QStringList options;
        options << tr("空白方案");
        for (const auto& info : templates)
            options << info.first;

        bool ok = true;
        const QString choice = QInputDialog::getItem(
            this, tr("方案模板"), tr("请选择通用方案模板："), options, 0, false, &ok);
        if (!ok)
        {
            QDir(directory).removeRecursively();
            return;
        }
        if (choice != options.first())
        {
            for (const auto& info : templates)
            {
                if (info.first == choice)
                {
                    templatePath = info.second;
                    break;
                }
            }
        }
    }

    if (!templatePath.isEmpty())
    {
        if (!copyDirectoryRecursively(templatePath, directory))
        {
            QDir(directory).removeRecursively();
            QMessageBox::warning(this, tr("创建方案"),
                                 tr("无法复制模板目录：%1")
                                     .arg(QDir::toNativeSeparators(templatePath)));
            return;
        }
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
        appendLogMessage(tr("已创建方案 %1").arg(name));
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
    persistSchemes();
    refreshNavigation(schemeId, m_activeModelId);
}

bool MainWindow::confirmSchemeDeletion(const SchemeRecord& scheme)
{
    const QString schemeName = scheme.name.isEmpty()
                                  ? tr("未命名方案")
                                  : scheme.name;
    const QString text =
        tr("确定要删除方案“%1”吗？此操作将删除方案下的所有模型。")
            .arg(schemeName);
    return QMessageBox::question(this, tr("删除方案"), text,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

bool MainWindow::confirmModelDeletion(const ModelRecord& model,
                                      const SchemeRecord& owner)
{
    const QString modelName = model.name.isEmpty()
                                  ? tr("未命名模型")
                                  : model.name;
    const QString schemeName = owner.name.isEmpty()
                                   ? tr("未命名方案")
                                   : owner.name;
    const QString text =
        tr("确定要从方案“%1”中删除模型“%2”吗？")
            .arg(schemeName, modelName);
    return QMessageBox::question(this, tr("删除模型"), text,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

void MainWindow::removeSchemeById(const QString& id)
{
    for (int i = 0; i < m_schemes.size(); ++i)
    {
        if (m_schemes[i].id == id)
        {
            if (isPathWithinDirectory(m_schemes[i].thumbnailPath,
                                      m_schemes[i].workingDirectory))
                QFile::remove(m_schemes[i].thumbnailPath);
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
        return tr("未命名工程");

    QFileInfo info(m_projectRoot);
    QString name = info.fileName();
    if (name.isEmpty())
    {
        QDir dir(m_projectRoot);
        name = dir.dirName();
    }
    if (name.isEmpty())
        name = QDir::toNativeSeparators(m_projectRoot);
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
    return makeUniqueName(desired, taken, tr("未命名方案"));
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
    return makeUniqueName(desired, taken, tr("未命名模型"));
}

void MainWindow::ensureUniqueModelNames(SchemeRecord& scheme) const
{
    QSet<QString> taken;
    for (ModelRecord& model : scheme.models)
        model.name = makeUniqueName(model.name, taken, tr("未命名模型"));
}

void MainWindow::ensureUniqueSchemeAndModelNames()
{
    QSet<QString> taken;
    for (SchemeRecord& scheme : m_schemes)
    {
        scheme.name = makeUniqueName(scheme.name, taken, tr("未命名方案"));
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
    if (m_storageFilePath.isEmpty())
        return false;

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
    const QString storedRoot = root.value(QStringLiteral("workspaceRoot")).toString().trimmed();
    if (!storedRoot.isEmpty())
    {
        QDir rootDir(storedRoot);
        if (rootDir.isAbsolute())
        {
            m_workspaceRoot = canonicalPathForDir(rootDir);
            if (m_workspaceRoot.isEmpty())
                m_workspaceRoot = QDir::cleanPath(storedRoot);
        }
        else if (!m_projectRoot.isEmpty())
        {
            QDir projectDir(m_projectRoot);
            const QString absolute = projectDir.filePath(storedRoot);
            m_workspaceRoot = canonicalPathForDir(QDir(absolute));
            if (m_workspaceRoot.isEmpty())
                m_workspaceRoot = QDir::cleanPath(absolute);
        }
        else
        {
            m_workspaceRoot = canonicalPathForDir(QDir(storedRoot));
            if (m_workspaceRoot.isEmpty())
                m_workspaceRoot = QDir::cleanPath(storedRoot);
        }
    }

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
            model.directory = canonicalPathForDir(QDir(mo.value(QStringLiteral("directory")).toString()));
            model.jsonPath = QDir::cleanPath(mo.value(QStringLiteral("jsonPath")).toString());
            model.batPath = QDir::cleanPath(mo.value(QStringLiteral("batPath")).toString());
            model.remarks = mo.value(QStringLiteral("remarks")).toString();
            if (model.directory.isEmpty() || model.jsonPath.isEmpty())
                continue;
            scheme.models.push_back(model);
        }
        loaded.push_back(scheme);
    }

    m_schemes = loaded;
    ensureUniqueSchemeAndModelNames();
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
            mo.insert(QStringLiteral("remarks"), model.remarks);
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
    root.insert(QStringLiteral("workspaceRoot"), workspaceToStore);
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
