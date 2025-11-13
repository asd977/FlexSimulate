#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QStringList>
#include <QHash>
#include <QPixmap>
#include <QUrl>
#include <QDir>
#include <QPair>
#include <QList>
#include <QSet>
#include <QDateTime>
#include <QMap>
#include <QSqlDatabase>
#include <QJsonObject>
#include <vtkSmartPointer.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTreeWidgetItem;
class QWidget;
class QShortcut;
class SchemeGalleryWidget;
class JsonPageBuilder;
class QListWidget;
class QLabel;
class QPushButton;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;
class vtkActor;
class QListWidgetItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void handleTreeSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onTreeContextMenuRequested(const QPoint& pos);
    void onTreeItemsReordered();
    void onExternalDrop(const QList<QUrl>& urls, QTreeWidgetItem* target);
    void onTreeItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onGalleryOpenRequested(const QString& id);
    void onGalleryDeleteRequested(const QString& id);
    void onGalleryDetailsRequested(const QString& id);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void deleteCurrentTreeItem();
    void onNewProjectTriggered();
    void onOpenProjectTriggered();
    void onAddLibraryScheme();

    void on_selectModelButton_clicked();

    void on_loadModelButton_clicked();
    void on_syncMaterialsButton_clicked();
    void on_materialsListWidget_currentItemChanged(QListWidgetItem* current,
                                                   QListWidgetItem* previous);

private:
    struct ModelRecord {
        QString id;
        QString name;
        QString directory;
        QString jsonPath;
        QString batPath;
        QString remarks;
        QString fingerprint;
        QString thumbnailPath;
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
        QString libraryId;
        QString workingDirectory;
        QString thumbnailPath;
        QString remarks;
        QVector<ModelRecord> models;
    };

    struct MaterialProperty {
        QString id;
        QString name;
        QString value;
        QString unit;
    };

    struct MaterialRecord {
        QString baseId;
        QString materialKey;
        QString materialId;
        QString materialName;
        QString materialType;
        QString materialTypeCode;
        QString materialTypeValue;
        QString materialStatus;
        QString supplierOptionValue;
        QString supplierOptionCode;
        QString supplierCode;
        QString supplierProduceCode;
        QString materialTrademark;
        QString gacMaterialTrademark;
        QString authenticationStatusValue;
        QString standardType;
        QString standardCode;
        QString creationDate;
        QString lastUpdateDate;
        QString status;
        QString formId;
        QStringList specs;
        QVector<MaterialProperty> properties;
    };

    enum TreeRoles {
        TypeRole = Qt::UserRole,
        IdRole,
        SchemeRole,
        ActiveRole
    };

    enum TreeItemType {
        LibraryItem = 0,
        ProjectItem,
        SchemeItem,
        ModelItem,
        MaterialLibraryItem
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
    void showMaterialsSettings();
    void showProjectInfo();
    QWidget* buildSchemeSettingsWidget(const SchemeRecord& scheme);
    QWidget* buildModelSettingsWidget(const ModelRecord& model);
    QWidget* buildProjectInfoWidget();
    QWidget* buildMaterialsSettingsWidget();
    void showLibrarySchemeDetail(const QString& entryId,
                                 const QString& projectSchemeId = QString());
    void refreshCurrentDetail();
    void updateToolbarState();
    void setVisualizationVisible(bool visible);
    void updateSelectionInfo(const QString& path = QString(),
                             const QString& remark = QString());
    void appendLogMessage(const QString& message);
    void displayResultFile(const QString& filePath);
    void clearVtkScene();
    void updateModelImagePreview(const ModelRecord* model);
    void refreshModelImagePreview();
    QString projectDisplayName() const;
    QString projectDisplayName(const QString& projectPath) const;

    SchemeRecord* schemeById(const QString& id);
    const SchemeRecord* schemeById(const QString& id) const;
    SchemeRecord* schemeByLibraryId(const QString& libraryId);
    const SchemeRecord* schemeByLibraryId(const QString& libraryId) const;
    SchemeRecord* resolveSchemeForLibraryEntry(const SchemeLibraryEntry& entry,
                                              bool* schemeNameAdjusted = nullptr,
                                              bool* linkEstablished = nullptr);
    SchemeRecord* schemeByWorkingDirectory(const QString& canonicalPath);
    ModelRecord* modelById(const QString& id, SchemeRecord** owner = nullptr);
    const ModelRecord* modelById(const QString& id, const SchemeRecord** owner = nullptr) const;

    QString createScheme(const QString& name, const QString& workingDir);
    QString importSchemeFromDirectory(const QString& dirPath, bool showError = true);
    QVector<QString> importModelsIntoScheme(const QString& schemeId,
                                            const QStringList& paths,
                                            bool showError = true);
    QStringList importModelsIntoLibraryEntry(SchemeLibraryEntry& entry,
                                            const QStringList& paths,
                                            bool showError = true);
    bool isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const;
    QVector<ModelRecord> scanSchemeFolder(const QString& schemeDir) const;
    QString computeModelFingerprint(const QString& jsonPath) const;
    QPixmap makeSchemePlaceholder(const QString& name) const;
    QPixmap loadSchemeThumbnail(const SchemeRecord& scheme) const;
    void applySchemeThumbnail(SchemeRecord& scheme, const QString& sourcePath);
    QString storeSchemeThumbnail(const QString& schemeDir, const QString& sourcePath) const;
    QPixmap loadModelThumbnail(const ModelRecord& model) const;
    void applyModelThumbnail(ModelRecord& model, const QString& sourcePath);
    QString storeModelThumbnail(const QString& modelDir, const QString& sourcePath) const;
    bool isPathWithinDirectory(const QString& filePath, const QString& directory) const;
    bool hasActiveProject() const;
    void updateRecentProjects(const QString& canonicalPath);
    bool removeProjectFromRecents(const QString& projectPath);
    void closeProject(const QString& projectPath);
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
    bool removeModelsByFingerprint(SchemeRecord& scheme,
                                   const QSet<QString>& fingerprints);
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
    bool readProjectStorage(const QString& projectRoot,
                            const QString& storageFile,
                            QVector<SchemeRecord>* schemes,
                            QString* remarks,
                            QDateTime* createdAt,
                            QDateTime* updatedAt,
                            QString* workspaceRoot) const;
    void saveSchemesToStorage() const;
    void persistSchemes();
    bool renameLibrarySchemesInProjects(const QString& libraryId,
                                        const QString& newName);
    QString makeUniqueWorkspaceSubdir(const QString& baseName) const;
    QString workspaceRoot() const;
    QVector<SchemeRecord> loadProjectPreviewSchemes(const QString& projectPath) const;
    void initializeMaterialsDatabase();
    void loadMaterialsFromDatabase();
    void saveMaterialsToDatabase(const QVector<MaterialRecord>& materials);
    void refreshMaterialsUi();
    void displayMaterialDetails(const MaterialRecord* material);
    const MaterialRecord* materialByKey(const QString& key) const;
    QString materialDisplayName(const MaterialRecord& material) const;
    QVector<MaterialRecord> fetchMaterialsFromRemote(QString* errorMessage);
    bool parseMaterialsPage(const QJsonObject& root,
                            QVector<MaterialRecord>* outRecords,
                            int* totalOut = nullptr,
                            QString* errorMessage = nullptr) const;
    bool applyMaterialDetail(MaterialRecord& record,
                             const QJsonObject& detailRoot,
                             QString* errorMessage = nullptr) const;
    QByteArray performGetRequest(const QUrl& url,
                                 const QMap<QString, QString>& headers,
                                 QString* errorMessage) const;
    QByteArray performPostRequest(const QUrl& url,
                                  const QByteArray& body,
                                  const QMap<QString, QString>& headers,
                                  QString* errorMessage) const;

    Ui::MainWindow *ui;
    SchemeGalleryWidget* m_galleryWidget = nullptr;
    QWidget* m_currentDetailWidget = nullptr;
    QVector<SchemeLibraryEntry> m_librarySchemes;
    QVector<SchemeRecord> m_schemes;
    QHash<QString, QTreeWidgetItem*> m_schemeItems;
    QHash<QString, QTreeWidgetItem*> m_modelItems;
    QTreeWidgetItem* m_libraryRootItem = nullptr;
    QTreeWidgetItem* m_projectRootItem = nullptr;
    QTreeWidgetItem* m_materialsRootItem = nullptr;
    QString m_activeSchemeId;
    QString m_activeModelId;
    bool m_viewingMaterials = false;
    bool m_blockTreeSignals = false;
    QString m_appStateFilePath;
    QString m_projectRoot;
    QString m_storageFilePath;
    QString m_workspaceRoot;
    QString m_schemeLibraryRoot;
    QString m_projectRemarks;
    QDateTime m_projectCreatedAt;
    QDateTime m_projectUpdatedAt;
    QStringList m_recentProjects;
    QString m_baseWindowTitle;
    QPixmap m_currentModelThumbnail;
    ModelRecord m_libraryPreviewModel;
    QString m_lastModelImageDir;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkActor> m_currentActor;
    QList<int> m_lastSplitterSizes;
    bool m_visualizationVisible = false;
    QString m_materialsDbPath;
    QSqlDatabase m_materialsDb;
    QVector<MaterialRecord> m_materials;
    QListWidget* m_materialsSettingsList = nullptr;
    QLabel* m_materialsSettingsStatusLabel = nullptr;
    QPushButton* m_materialsSettingsSyncButton = nullptr;
    QString m_activeMaterialKey;
};
