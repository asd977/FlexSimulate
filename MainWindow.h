#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QHash>
#include <QPixmap>
#include <QUrl>
#include <QDir>
#include <QPair>
#include <QList>
#include <QSet>
#include <QDateTime>
#include <memory>

#include "core/SchemeTypes.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTreeWidgetItem;
class QWidget;
class QShortcut;
class SchemeGalleryWidget;
class JsonPageBuilder;
class PlanPage;
class WelcomePage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the main window and sets up the UI.
     * @param parent Optional parent widget.
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Destroys the main window and releases resources.
     */
    ~MainWindow();

private slots:
    /**
     * @brief Handles selection changes in the navigation tree.
     * @param current Newly selected item.
     * @param previous Previously selected item.
     */
    void handleTreeSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    /**
     * @brief Responds to tree item edits.
     * @param item Edited tree item.
     * @param column Column index being edited.
     */
    void onTreeItemChanged(QTreeWidgetItem* item, int column);

    /**
     * @brief Opens the tree context menu for the given position.
     * @param pos Position that triggered the menu.
     */
    void onTreeContextMenuRequested(const QPoint& pos);

    /**
     * @brief Handles reordering of tree items.
     */
    void onTreeItemsReordered();

    /**
     * @brief Processes external file drops onto the tree widget.
     * @param urls Dropped URLs.
     * @param target Target tree item.
     */
    void onExternalDrop(const QList<QUrl>& urls, QTreeWidgetItem* target);

    /**
     * @brief Opens a scheme from the gallery when requested.
     * @param id Identifier of the scheme to open.
     */
    void onGalleryOpenRequested(const QString& id);

    /**
     * @brief Handles gallery deletion requests.
     * @param id Identifier of the scheme to delete.
     */
    void onGalleryDeleteRequested(const QString& id);

    /**
     * @brief Shows gallery details for the requested scheme.
     * @param id Identifier of the scheme whose details to show.
     */
    void onGalleryDetailsRequested(const QString& id);

    /**
     * @brief Deletes the currently selected tree item.
     */
    void deleteCurrentTreeItem();

    /**
     * @brief Launches the new project workflow.
     */
    void onNewProjectTriggered();

    /**
     * @brief Opens the project selection dialog.
     */
    void onOpenProjectTriggered();

    /**
     * @brief Initiates addition of a library scheme.
     */
    void onAddLibraryScheme();

private:
    using ModelRecord = FlexSimulate::ModelRecord;
    using SchemeLibraryEntry = FlexSimulate::SchemeLibraryEntry;
    using SchemeRecord = FlexSimulate::SchemeRecord;

    enum TreeRoles {
        TypeRole = Qt::UserRole,
        IdRole,
        SchemeRole
    };

    enum TreeItemType {
        LibraryItem = 0,
        ProjectItem,
        SchemeItem,
        ModelItem
    };

    /**
     * @brief Connects UI signals and slots for the window.
     */
    void setupConnections();

    /**
     * @brief Loads bundled schemes during startup.
     */
    void loadInitialSchemes();

    /**
     * @brief Restores application state from disk.
     */
    void loadApplicationState();

    /**
     * @brief Saves application state to disk.
     */
    void saveApplicationState() const;

    /**
     * @brief Puts the UI into a projectless state.
     */
    void enterProjectlessState();

    /**
     * @brief Opens a project at the specified path.
     * @param path Project directory to open.
     * @param silent Whether to suppress error dialogs.
     * @return True if the project was opened successfully.
     */
    bool openProjectAt(const QString& path, bool silent = false);

    /**
     * @brief Ensures the project directory contains required structure.
     * @param rootPath Project root path.
     * @return True when the structure exists or was created.
     */
    bool ensureProjectStructure(const QString& rootPath);

    /**
     * @brief Updates the window title to reflect current context.
     */
    void updateWindowTitle();

    /**
     * @brief Refreshes tree navigation with optional selection targets.
     * @param schemeToSelect Scheme identifier to select.
     * @param modelToSelect Model identifier to select.
     */
    void refreshNavigation(const QString& schemeToSelect = QString(),
                           const QString& modelToSelect = QString());

    /**
     * @brief Rebuilds the entire navigation tree.
     */
    void rebuildTree();

    /**
     * @brief Updates the gallery display to reflect current data.
     */
    void updateGallery();

    /**
     * @brief Selects the specified tree item based on scheme and model.
     * @param schemeId Scheme identifier.
     * @param modelId Model identifier.
     */
    void selectTreeItem(const QString& schemeId, const QString& modelId);

    /**
     * @brief Clears the detail widget from the plan page.
     */
    void clearDetailWidget();

    /**
     * @brief Shows settings UI for the specified scheme.
     * @param schemeId Identifier of the scheme to configure.
     */
    void showSchemeSettings(const QString& schemeId);

    /**
     * @brief Shows settings UI for the specified model.
     * @param modelId Identifier of the model to configure.
     */
    void showModelSettings(const QString& modelId);

    /**
     * @brief Displays project metadata in the detail view.
     */
    void showProjectInfo();

    /**
     * @brief Builds a scheme settings widget for the given record.
     * @param scheme Scheme record used for the widget.
     * @return Newly created widget.
     */
    QWidget* buildSchemeSettingsWidget(const SchemeRecord& scheme);

    /**
     * @brief Builds a model settings widget for the given record.
     * @param model Model record used for the widget.
     * @return Newly created widget.
     */
    QWidget* buildModelSettingsWidget(const ModelRecord& model);

    /**
     * @brief Builds a widget summarizing project information.
     * @return Newly created project info widget.
     */
    QWidget* buildProjectInfoWidget();

    /**
     * @brief Shows details for a library scheme, optionally linked to a project scheme.
     * @param entryId Library entry identifier.
     * @param projectSchemeId Optional related project scheme identifier.
     */
    void showLibrarySchemeDetail(const QString& entryId,
                                 const QString& projectSchemeId = QString());

    /**
     * @brief Refreshes whichever detail widget is currently active.
     */
    void refreshCurrentDetail();

    /**
     * @brief Updates toolbar controls to reflect current selection state.
     */
    void updateToolbarState();

    /**
     * @brief Toggles visualization visibility in the plan page.
     * @param visible Whether the visualization should be shown.
     */
    void setVisualizationVisible(bool visible);

    /**
     * @brief Updates selection info text shown on the plan page.
     * @param path Highlighted path value.
     * @param remark Additional remark text.
     */
    void updateSelectionInfo(const QString& path = QString(),
                             const QString& remark = QString());

    /**
     * @brief Appends a log message to the run log.
     * @param message Message text to append.
     */
    void appendLogMessage(const QString& message);

    /**
     * @brief Displays a result file within the visualization viewer.
     * @param filePath Path to the result file.
     */
    void displayResultFile(const QString& filePath);

    /**
     * @brief Clears the visualization scene.
     */
    void clearVtkScene();

    /**
     * @brief Computes the human-readable project display name.
     * @return Project name shown in the UI.
     */
    QString projectDisplayName() const;

    /**
     * @brief Finds a scheme record by identifier.
     * @param id Scheme identifier.
     * @return Pointer to the scheme record or nullptr.
     */
    SchemeRecord* schemeById(const QString& id);

    /**
     * @brief Retrieves a constant scheme record by identifier.
     * @param id Scheme identifier.
     * @return Const pointer to the scheme record or nullptr.
     */
    const SchemeRecord* schemeById(const QString& id) const;

    /**
     * @brief Finds a scheme by its associated library identifier.
     * @param libraryId Library entry identifier.
     * @return Pointer to the scheme record or nullptr.
     */
    SchemeRecord* schemeByLibraryId(const QString& libraryId);

    /**
     * @brief Retrieves a constant scheme by library identifier.
     * @param libraryId Library entry identifier.
     * @return Const pointer to the scheme record or nullptr.
     */
    const SchemeRecord* schemeByLibraryId(const QString& libraryId) const;

    /**
     * @brief Resolves or creates a project scheme for the given library entry.
     * @param entry Library entry to map.
     * @param schemeNameAdjusted Optional output flag when name changes.
     * @param linkEstablished Optional output flag when link created.
     * @return Pointer to the resolved scheme record.
     */
    SchemeRecord* resolveSchemeForLibraryEntry(const SchemeLibraryEntry& entry,
                                              bool* schemeNameAdjusted = nullptr,
                                              bool* linkEstablished = nullptr);

    /**
     * @brief Finds a scheme by its working directory path.
     * @param canonicalPath Canonical working directory.
     * @return Pointer to the scheme record or nullptr.
     */
    SchemeRecord* schemeByWorkingDirectory(const QString& canonicalPath);

    /**
     * @brief Finds a model by identifier and optionally returns its owner scheme.
     * @param id Model identifier to locate.
     * @param owner Optional output for owning scheme pointer.
     * @return Pointer to the model record or nullptr.
     */
    ModelRecord* modelById(const QString& id, SchemeRecord** owner = nullptr);

    /**
     * @brief Retrieves a const model record by identifier.
     * @param id Model identifier to locate.
     * @param owner Optional output for owning scheme pointer.
     * @return Const pointer to the model record or nullptr.
     */
    const ModelRecord* modelById(const QString& id, const SchemeRecord** owner = nullptr) const;

    /**
     * @brief Creates a new scheme record with the given name and working directory.
     * @param name Desired scheme name.
     * @param workingDir Working directory to use.
     * @return Identifier of the created scheme.
     */
    QString createScheme(const QString& name, const QString& workingDir);

    /**
     * @brief Imports an existing scheme from a directory.
     * @param dirPath Directory containing the scheme.
     * @param showError Whether to show error dialogs on failure.
     * @return Identifier of the imported scheme or empty on failure.
     */
    QString importSchemeFromDirectory(const QString& dirPath, bool showError = true);

    /**
     * @brief Imports models into a scheme from provided file paths.
     * @param schemeId Scheme identifier to receive models.
     * @param paths Paths to import.
     * @param showError Whether to display errors.
     * @return Identifiers of imported models.
     */
    QVector<QString> importModelsIntoScheme(const QString& schemeId,
                                            const QStringList& paths,
                                            bool showError = true);

    /**
     * @brief Imports models into a library entry.
     * @param entry Library entry to update.
     * @param paths Paths to import.
     * @param showError Whether to display errors.
     * @return Names of imported models.
     */
    QStringList importModelsIntoLibraryEntry(SchemeLibraryEntry& entry,
                                            const QStringList& paths,
                                            bool showError = true);

    /**
     * @brief Checks whether a directory represents a model folder.
     * @param dir Directory to inspect.
     * @param jsonPath Output path to para.json.
     * @param batPath Output path to calculate.bat.
     * @return True if the directory is a model folder.
     */
    bool isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const;

    /**
     * @brief Scans a scheme directory for models.
     * @param schemeDir Path to the scheme directory.
     * @return List of discovered model records.
     */
    QVector<ModelRecord> scanSchemeFolder(const QString& schemeDir) const;

    /**
     * @brief Computes a fingerprint for a model based on its JSON data.
     * @param jsonPath Path to the JSON file.
     * @return Fingerprint string.
     */
    QString computeModelFingerprint(const QString& jsonPath) const;

    /**
     * @brief Generates a placeholder pixmap for a scheme.
     * @param name Scheme name to display.
     * @return Placeholder image.
     */
    QPixmap makeSchemePlaceholder(const QString& name) const;

    /**
     * @brief Loads the thumbnail image for a scheme.
     * @param scheme Scheme record to read from.
     * @return Thumbnail pixmap.
     */
    QPixmap loadSchemeThumbnail(const SchemeRecord& scheme) const;

    /**
     * @brief Applies a thumbnail to a scheme record.
     * @param scheme Scheme record to update.
     * @param sourcePath Source image path.
     */
    void applySchemeThumbnail(SchemeRecord& scheme, const QString& sourcePath);

    /**
     * @brief Stores a scheme thumbnail alongside its data.
     * @param schemeDir Directory of the scheme.
     * @param sourcePath Source image path.
     * @return Stored image path.
     */
    QString storeSchemeThumbnail(const QString& schemeDir, const QString& sourcePath) const;

    /**
     * @brief Determines if a path is within a directory.
     * @param filePath Path to check.
     * @param directory Directory to compare against.
     * @return True if the file resides within the directory.
     */
    bool isPathWithinDirectory(const QString& filePath, const QString& directory) const;

    /**
     * @brief Checks whether a project is currently active.
     * @return True if a project is active.
     */
    bool hasActiveProject() const;

    /**
     * @brief Loads the scheme library from storage.
     */
    void loadSchemeLibrary();

    /**
     * @brief Persists the scheme library to storage.
     */
    void saveSchemeLibrary() const;

    /**
     * @brief Returns the root path for the scheme library.
     * @return Library root path.
     */
    QString schemeLibraryRoot() const;

    /**
     * @brief Generates a unique subdirectory name under the library root.
     * @param baseName Desired base name.
     * @return Unique directory name.
     */
    QString makeUniqueLibrarySubdir(const QString& baseName) const;

    /**
     * @brief Looks up a library entry by identifier.
     * @param id Entry identifier.
     * @return Pointer to the entry or nullptr.
     */
    SchemeLibraryEntry* libraryEntryById(const QString& id);

    /**
     * @brief Retrieves a const library entry by identifier.
     * @param id Entry identifier.
     * @return Const pointer to the entry or nullptr.
     */
    const SchemeLibraryEntry* libraryEntryById(const QString& id) const;

    /**
     * @brief Loads a thumbnail image for a library entry.
     * @param entry Library entry source.
     * @return Thumbnail pixmap.
     */
    QPixmap loadLibraryThumbnail(const SchemeLibraryEntry& entry) const;

    /**
     * @brief Applies a thumbnail image to a library entry.
     * @param entry Entry to update.
     * @param sourcePath Source image path.
     */
    void applyLibraryThumbnail(SchemeLibraryEntry& entry, const QString& sourcePath);

    /**
     * @brief Removes a library entry by identifier.
     * @param id Entry identifier to remove.
     * @return True if the entry was removed.
     */
    bool removeLibraryEntry(const QString& id);

    /**
     * @brief Prompts the user to add a new scheme.
     */
    void promptAddScheme();

    /**
     * @brief Prompts the user to add a new model to the given scheme.
     * @param schemeId Target scheme identifier.
     */
    void promptAddModel(const QString& schemeId);

    /**
     * @brief Opens scheme settings dialog for the specified scheme.
     * @param schemeId Scheme identifier to edit.
     */
    void openSchemeSettings(const QString& schemeId);

    /**
     * @brief Removes a scheme by identifier.
     * @param id Scheme identifier to remove.
     */
    void removeSchemeById(const QString& id);

    /**
     * @brief Removes a model by identifier.
     * @param id Model identifier to remove.
     */
    void removeModelById(const QString& id);

    /**
     * @brief Removes models with matching fingerprints from a scheme.
     * @param scheme Scheme to modify.
     * @param fingerprints Set of fingerprints to remove.
     * @return True if any models were removed.
     */
    bool removeModelsByFingerprint(SchemeRecord& scheme,
                                   const QSet<QString>& fingerprints);

    /**
     * @brief Prompts the user to confirm scheme deletion.
     * @param scheme Scheme to delete.
     * @return True if deletion should proceed.
     */
    bool confirmSchemeDeletion(const SchemeRecord& scheme);

    /**
     * @brief Prompts the user to confirm model deletion.
     * @param model Model to delete.
     * @param owner Owning scheme for context.
     * @return True if deletion should proceed.
     */
    bool confirmModelDeletion(const ModelRecord& model, const SchemeRecord& owner);

    /**
     * @brief Synchronizes data model with current tree widget state.
     */
    void syncDataFromTree();

    /**
     * @brief Generates a unique name within a set of taken names.
     * @param desired Requested name.
     * @param taken Set of already used names.
     * @param fallback Default fallback name.
     * @return Unique name.
     */
    QString makeUniqueName(const QString& desired, QSet<QString>& taken,
                           const QString& fallback) const;

    /**
     * @brief Generates a unique scheme name.
     * @param desired Requested name.
     * @param excludeId Optional scheme identifier to ignore.
     * @return Unique scheme name.
     */
    QString makeUniqueSchemeName(const QString& desired,
                                 const QString& excludeId = QString()) const;

    /**
     * @brief Generates a unique model name within a scheme.
     * @param scheme Scheme containing existing models.
     * @param desired Requested name.
     * @param excludeId Optional model identifier to ignore.
     * @return Unique model name.
     */
    QString makeUniqueModelName(const SchemeRecord& scheme, const QString& desired,
                                const QString& excludeId = QString()) const;

    /**
     * @brief Ensures all models in a scheme have unique names.
     * @param scheme Scheme to normalize.
     */
    void ensureUniqueModelNames(SchemeRecord& scheme) const;

    /**
     * @brief Ensures all schemes and their models have unique names.
     */
    void ensureUniqueSchemeAndModelNames();

    /**
     * @brief Loads scheme data from persistent storage.
     * @return True if the load succeeded.
     */
    bool loadSchemesFromStorage();

    /**
     * @brief Saves scheme data to persistent storage.
     */
    void saveSchemesToStorage() const;

    /**
     * @brief Persists schemes after modifications.
     */
    void persistSchemes();

    /**
     * @brief Creates a unique workspace subdirectory name.
     * @param baseName Desired base name.
     * @return Unique directory name.
     */
    QString makeUniqueWorkspaceSubdir(const QString& baseName) const;

    /**
     * @brief Returns the root path of the active workspace.
     * @return Workspace root path.
     */
    QString workspaceRoot() const;

    Ui::MainWindow *ui;
    std::unique_ptr<PlanPage> m_planPage;
    std::unique_ptr<WelcomePage> m_welcomePage;
    QVector<SchemeLibraryEntry> m_librarySchemes;
    QVector<SchemeRecord> m_schemes;
    QHash<QString, QTreeWidgetItem*> m_schemeItems;
    QHash<QString, QTreeWidgetItem*> m_modelItems;
    QTreeWidgetItem* m_libraryRootItem = nullptr;
    QTreeWidgetItem* m_projectRootItem = nullptr;
    QString m_activeSchemeId;
    QString m_activeModelId;
    bool m_blockTreeSignals = false;
    QString m_appStateFilePath;
    QString m_projectRoot;
    QString m_storageFilePath;
    QString m_workspaceRoot;
    QString m_schemeLibraryRoot;
    QString m_projectRemarks;
    QDateTime m_projectCreatedAt;
    QDateTime m_projectUpdatedAt;
    QString m_baseWindowTitle;
};
