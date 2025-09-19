#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QHash>
#include <QPixmap>
#include <QUrl>
#include <QDir>
#include <QPair>
#include <vtkSmartPointer.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTreeWidgetItem;
class QWidget;
class QShortcut;
class SchemeGalleryWidget;
class JsonPageBuilder;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkActor;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_showPlanPushButton_clicked();
    void handleTreeSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onTreeContextMenuRequested(const QPoint& pos);
    void onTreeItemsReordered();
    void onExternalDrop(const QList<QUrl>& urls, QTreeWidgetItem* target);
    void onGalleryOpenRequested(const QString& id);
    void onGalleryAddRequested(const QString& id);
    void onGalleryDeleteRequested(const QString& id);
    void deleteCurrentTreeItem();
    void onNewProjectTriggered();
    void onOpenProjectTriggered();
    void onAddLibraryScheme();

private:
    struct ModelRecord {
        QString id;
        QString name;
        QString directory;
        QString jsonPath;
        QString batPath;
    };

    struct SchemeLibraryEntry {
        QString id;
        QString name;
        QString directory;
        QString thumbnailPath;
        bool deletable = false;
    };

    struct SchemeRecord {
        QString id;
        QString name;
        QString workingDirectory;
        QString thumbnailPath;
        QVector<ModelRecord> models;
    };

    enum TreeRoles {
        TypeRole = Qt::UserRole,
        IdRole,
        SchemeRole
    };

    enum TreeItemType {
        ProjectItem = 0,
        SchemeItem,
        ModelItem
    };

    void setupUiHelpers();
    void setupConnections();
    void loadInitialSchemes();
    void loadApplicationState();
    void saveApplicationState() const;
    void enterProjectlessState();
    bool openProjectAt(const QString& path, bool silent = false);
    bool ensureProjectStructure(const QString& rootPath);
    void updateWindowTitle();

    void refreshNavigation(const QString& schemeToSelect = QString(),
                           const QString& modelToSelect = QString());
    void rebuildTree();
    void updateGallery();
    void selectTreeItem(const QString& schemeId, const QString& modelId);
    void clearDetailWidget();
    void showSchemeSettings(const QString& schemeId);
    void showModelSettings(const QString& modelId);
    QWidget* buildSchemeSettingsWidget(const SchemeRecord& scheme);
    QWidget* buildModelSettingsWidget(const ModelRecord& model);
    void refreshCurrentDetail();
    void updateToolbarState();
    void appendLogMessage(const QString& message);
    void displayStlFile(const QString& filePath);
    void clearVtkScene();
    QString projectDisplayName() const;

    SchemeRecord* schemeById(const QString& id);
    const SchemeRecord* schemeById(const QString& id) const;
    SchemeRecord* schemeByWorkingDirectory(const QString& canonicalPath);
    ModelRecord* modelById(const QString& id, SchemeRecord** owner = nullptr);
    const ModelRecord* modelById(const QString& id, const SchemeRecord** owner = nullptr) const;

    QString createScheme(const QString& name, const QString& workingDir);
    QString importSchemeFromDirectory(const QString& dirPath, bool showError = true);
    QVector<QString> importModelsIntoScheme(const QString& schemeId,
                                            const QStringList& paths,
                                            bool showError = true);
    bool isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const;
    QVector<ModelRecord> scanSchemeFolder(const QString& schemeDir) const;
    QPixmap makeSchemePlaceholder(const QString& name) const;
    QPixmap loadSchemeThumbnail(const SchemeRecord& scheme) const;
    void applySchemeThumbnail(SchemeRecord& scheme, const QString& sourcePath);
    QString storeSchemeThumbnail(const QString& schemeDir, const QString& sourcePath) const;
    bool isPathWithinDirectory(const QString& filePath, const QString& directory) const;
    QVector<QPair<QString, QString>> availableSchemeTemplates() const;
    QStringList templateSearchRoots() const;
    bool hasActiveProject() const;
    void loadSchemeLibrary();
    void saveSchemeLibrary() const;
    QString schemeLibraryRoot() const;
    QString makeUniqueLibrarySubdir(const QString& baseName) const;
    SchemeLibraryEntry* libraryEntryById(const QString& id);
    const SchemeLibraryEntry* libraryEntryById(const QString& id) const;
    QPixmap loadLibraryThumbnail(const SchemeLibraryEntry& entry) const;
    void applyLibraryThumbnail(SchemeLibraryEntry& entry, const QString& sourcePath);
    bool removeLibraryEntry(const QString& id);
    void promptAddScheme();
    void promptAddModel(const QString& schemeId);
    void openSchemeSettings(const QString& schemeId);
    void removeSchemeById(const QString& id);
    void removeModelById(const QString& id);
    bool confirmSchemeDeletion(const SchemeRecord& scheme);
    bool confirmModelDeletion(const ModelRecord& model, const SchemeRecord& owner);
    void syncDataFromTree();
    QString makeUniqueName(const QString& desired, QSet<QString>& taken,
                           const QString& fallback) const;
    QString makeUniqueSchemeName(const QString& desired,
                                 const QString& excludeId = QString()) const;
    QString makeUniqueModelName(const SchemeRecord& scheme, const QString& desired,
                                const QString& excludeId = QString()) const;
    void ensureUniqueModelNames(SchemeRecord& scheme) const;
    void ensureUniqueSchemeAndModelNames();
    bool loadSchemesFromStorage();
    void saveSchemesToStorage() const;
    void persistSchemes() const;
    QString makeUniqueWorkspaceSubdir(const QString& baseName) const;
    QString workspaceRoot() const;

    Ui::MainWindow *ui;
    SchemeGalleryWidget* m_galleryWidget = nullptr;
    QWidget* m_currentDetailWidget = nullptr;
    QVector<SchemeLibraryEntry> m_librarySchemes;
    QVector<SchemeRecord> m_schemes;
    QHash<QString, QTreeWidgetItem*> m_schemeItems;
    QHash<QString, QTreeWidgetItem*> m_modelItems;
    QTreeWidgetItem* m_projectRootItem = nullptr;
    QString m_activeSchemeId;
    QString m_activeModelId;
    bool m_blockTreeSignals = false;
    QString m_appStateFilePath;
    QString m_projectRoot;
    QString m_storageFilePath;
    QString m_workspaceRoot;
    QString m_schemeLibraryRoot;
    QString m_baseWindowTitle;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkActor> m_currentActor;
};
