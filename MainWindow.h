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
     * @brief 构造主窗口并完成 UI 初始化。
     * @param parent 可选的父级窗口。
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief 销毁主窗口并释放资源。
     */
    ~MainWindow();

private slots:
    /**
     * @brief 处理导航树中的选中项变化。
     * @param current 新选中的树项。
     * @param previous 之前选中的树项。
     */
    void handleTreeSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    /**
     * @brief 响应树节点的编辑操作。
     * @param item 被编辑的树节点。
     * @param column 正在编辑的列索引。
     */
    void onTreeItemChanged(QTreeWidgetItem* item, int column);

    /**
     * @brief 在给定位置打开树形控件的上下文菜单。
     * @param pos 触发菜单的坐标。
     */
    void onTreeContextMenuRequested(const QPoint& pos);

    /**
     * @brief 处理树节点的重新排序。
     */
    void onTreeItemsReordered();

    /**
     * @brief 处理外部文件拖拽到树形控件的事件。
     * @param urls 被拖入的 URL 列表。
     * @param target 接收拖拽的目标树节点。
     */
    void onExternalDrop(const QList<QUrl>& urls, QTreeWidgetItem* target);

    /**
     * @brief 响应画廊的打开请求。
     * @param id 需要打开的方案标识符。
     */
    void onGalleryOpenRequested(const QString& id);

    /**
     * @brief 处理画廊中的删除请求。
     * @param id 需要删除的方案标识符。
     */
    void onGalleryDeleteRequested(const QString& id);

    /**
     * @brief 展示指定方案的画廊详情。
     * @param id 需要查看详情的方案标识符。
     */
    void onGalleryDetailsRequested(const QString& id);

    /**
     * @brief 删除当前选中的树节点。
     */
    void deleteCurrentTreeItem();

    /**
     * @brief 启动新建项目流程。
     */
    void onNewProjectTriggered();

    /**
     * @brief 打开项目选择对话框。
     */
    void onOpenProjectTriggered();

    /**
     * @brief 开始向库中添加方案。
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
     * @brief 连接窗口所需的 UI 信号与槽。
     */
    void setupConnections();

    /**
     * @brief 在启动时加载内置方案。
     */
    void loadInitialSchemes();

    /**
     * @brief 从磁盘恢复应用状态。
     */
    void loadApplicationState();

    /**
     * @brief 将应用状态保存到磁盘。
     */
    void saveApplicationState() const;

    /**
     * @brief 将界面切换到无项目状态。
     */
    void enterProjectlessState();

    /**
     * @brief 打开指定路径的项目。
     * @param path 需要打开的项目目录。
     * @param silent 是否隐藏错误弹窗。
     * @return 成功打开返回 true。
     */
    bool openProjectAt(const QString& path, bool silent = false);

    /**
     * @brief 确保项目目录具备所需的目录结构。
     * @param rootPath 项目根路径。
     * @return 若结构存在或创建成功则返回 true。
     */
    bool ensureProjectStructure(const QString& rootPath);

    /**
     * @brief 根据当前上下文刷新窗口标题。
     */
    void updateWindowTitle();

    /**
     * @brief 刷新树形导航，可指定需要选中的方案或模型。
     * @param schemeToSelect 要选中的方案标识符。
     * @param modelToSelect 要选中的模型标识符。
     */
    void refreshNavigation(const QString& schemeToSelect = QString(),
                           const QString& modelToSelect = QString());

    /**
     * @brief 重新构建整个导航树。
     */
    void rebuildTree();

    /**
     * @brief 更新画廊展示以反映最新数据。
     */
    void updateGallery();

    /**
     * @brief 根据方案与模型标识选择树节点。
     * @param schemeId 方案标识符。
     * @param modelId 模型标识符。
     */
    void selectTreeItem(const QString& schemeId, const QString& modelId);

    /**
     * @brief 清空计划页面中的详情组件。
     */
    void clearDetailWidget();

    /**
     * @brief 打开指定方案的设置界面。
     * @param schemeId 需要配置的方案标识符。
     */
    void showSchemeSettings(const QString& schemeId);

    /**
     * @brief 打开指定模型的设置界面。
     * @param modelId 需要配置的模型标识符。
     */
    void showModelSettings(const QString& modelId);

    /**
     * @brief 在详情视图中展示项目元数据。
     */
    void showProjectInfo();

    /**
     * @brief 基于方案记录构建方案设置组件。
     * @param scheme 方案数据记录。
     * @return 新创建的设置组件。
     */
    QWidget* buildSchemeSettingsWidget(const SchemeRecord& scheme);

    /**
     * @brief 基于模型记录构建模型设置组件。
     * @param model 模型数据记录。
     * @return 新创建的设置组件。
     */
    QWidget* buildModelSettingsWidget(const ModelRecord& model);

    /**
     * @brief 构建展示项目概览信息的组件。
     * @return 新创建的项目信息组件。
     */
    QWidget* buildProjectInfoWidget();

    /**
     * @brief 展示库中方案的详情，可选关联项目内方案。
     * @param entryId 库方案的标识符。
     * @param projectSchemeId 可选的关联项目方案标识符。
     */
    void showLibrarySchemeDetail(const QString& entryId,
                                 const QString& projectSchemeId = QString());

    /**
     * @brief 刷新当前激活的详情组件。
     */
    void refreshCurrentDetail();

    /**
     * @brief 根据当前选择状态刷新工具栏。
     */
    void updateToolbarState();

    /**
     * @brief 切换计划页面中可视化区域的可见性。
     * @param visible 是否显示可视化。
     */
    void setVisualizationVisible(bool visible);

    /**
     * @brief 更新计划页面展示的选中信息。
     * @param path 高亮显示的路径。
     * @param remark 附加的备注文本。
     */
    void updateSelectionInfo(const QString& path = QString(),
                             const QString& remark = QString());

    /**
     * @brief 将日志消息追加到运行日志。
     * @param message 需要追加的消息内容。
     */
    void appendLogMessage(const QString& message);

    /**
     * @brief 在可视化视图中展示结果文件。
     * @param filePath 结果文件的路径。
     */
    void displayResultFile(const QString& filePath);

    /**
     * @brief 清空可视化场景。
     */
    void clearVtkScene();

    /**
     * @brief 计算在人机界面中显示的项目名称。
     * @return 用于显示的项目名称。
     */
    QString projectDisplayName() const;

    /**
     * @brief 根据标识符查找方案记录。
     * @param id 方案标识符。
     * @return 返回方案记录指针，找不到则为 nullptr。
     */
    SchemeRecord* schemeById(const QString& id);

    /**
     * @brief 根据标识符获取常量方案记录。
     * @param id 方案标识符。
     * @return 返回常量方案记录指针，找不到则为 nullptr。
     */
    const SchemeRecord* schemeById(const QString& id) const;

    /**
    /**
     * @brief 通过关联的库标识符查找方案。
     * @param libraryId 库条目标识符。
     * @return 返回方案记录指针，找不到则为 nullptr。
     */
    SchemeRecord* schemeByLibraryId(const QString& libraryId);

    /**
     * @brief 根据库标识符获取常量方案记录。
     * @param libraryId 库条目标识符。
     * @return 返回常量方案记录指针，找不到则为 nullptr。
     */
    const SchemeRecord* schemeByLibraryId(const QString& libraryId) const;

    /**
     * @brief 为给定库条目查找或创建对应的项目方案。
     * @param entry 需要映射的库条目。
     * @param schemeNameAdjusted 若方案名称被调整则输出 true。
     * @param linkEstablished 若建立了关联关系则输出 true。
     * @return 返回匹配到的方案记录指针。
     */
    SchemeRecord* resolveSchemeForLibraryEntry(const SchemeLibraryEntry& entry,
                                              bool* schemeNameAdjusted = nullptr,
                                              bool* linkEstablished = nullptr);

    /**
     * @brief 根据工作目录的规范路径查找方案。
     * @param canonicalPath 工作目录的规范路径。
     * @return 返回方案记录指针，找不到则为 nullptr。
     */
    SchemeRecord* schemeByWorkingDirectory(const QString& canonicalPath);

    /**
     * @brief 根据标识符查找模型，可选返回所属方案。
     * @param id 要查找的模型标识符。
     * @param owner 可选输出参数，返回所属方案指针。
     * @return 返回模型记录指针，找不到则为 nullptr。
     */
    ModelRecord* modelById(const QString& id, SchemeRecord** owner = nullptr);

    /**
     * @brief 根据标识符获取常量模型记录。
     * @param id 要查找的模型标识符。
     * @param owner 可选输出参数，返回所属方案指针。
     * @return 返回常量模型记录指针，找不到则为 nullptr。
     */
    const ModelRecord* modelById(const QString& id, const SchemeRecord** owner = nullptr) const;

    /**
     * @brief 使用指定名称与工作目录创建新的方案记录。
     * @param name 目标方案名称。
     * @param workingDir 方案的工作目录。
     * @return 返回新建方案的标识符。
     */
    QString createScheme(const QString& name, const QString& workingDir);

    /**
     * @brief 从目录导入已有方案。
     * @param dirPath 包含方案内容的目录。
     * @param showError 失败时是否显示错误对话框。
     * @return 成功时返回导入方案的标识符，失败返回空字符串。
     */
    QString importSchemeFromDirectory(const QString& dirPath, bool showError = true);

    /**
     * @brief 将文件导入为指定方案的模型。
     * @param schemeId 接收模型的方案标识符。
     * @param paths 需要导入的路径集合。
     * @param showError 是否显示错误信息。
     * @return 返回导入模型的标识符列表。
     */
    QVector<QString> importModelsIntoScheme(const QString& schemeId,
                                            const QStringList& paths,
                                            bool showError = true);

    /**
     * @brief 将文件导入为库条目的模型。
     * @param entry 需要更新的库条目。
     * @param paths 需要导入的路径集合。
     * @param showError 是否显示错误信息。
     * @return 返回导入模型的名称列表。
     */
    QStringList importModelsIntoLibraryEntry(SchemeLibraryEntry& entry,
                                            const QStringList& paths,
                                            bool showError = true);

    /**
     * @brief 判断目录是否为模型文件夹。
     * @param dir 待检查的目录。
     * @param jsonPath 输出 para.json 的路径。
     * @param batPath 输出 calculate.bat 的路径。
     * @return 若目录符合模型结构则返回 true。
     */
    bool isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const;

    /**
     * @brief 扫描方案目录中的模型。
     * @param schemeDir 方案目录路径。
     * @return 返回发现的模型记录列表。
     */
    QVector<ModelRecord> scanSchemeFolder(const QString& schemeDir) const;

    /**
     * @brief 根据模型 JSON 数据计算指纹。
     * @param jsonPath JSON 文件路径。
     * @return 返回指纹字符串。
     */
    QString computeModelFingerprint(const QString& jsonPath) const;

    /**
     * @brief 为方案生成占位缩略图。
     * @param name 用于显示的方案名称。
     * @return 返回占位图像。
     */
    QPixmap makeSchemePlaceholder(const QString& name) const;

    /**
     * @brief 读取方案的缩略图。
     * @param scheme 方案记录。
     * @return 返回缩略图像。
     */
    QPixmap loadSchemeThumbnail(const SchemeRecord& scheme) const;

    /**
     * @brief 为方案记录应用新的缩略图。
     * @param scheme 需要更新的方案记录。
     * @param sourcePath 缩略图的源文件路径。
     */
    void applySchemeThumbnail(SchemeRecord& scheme, const QString& sourcePath);

    /**
     * @brief 将方案缩略图与方案数据一并存储。
     * @param schemeDir 方案所在的目录。
     * @param sourcePath 缩略图的源文件路径。
     * @return 返回保存后的缩略图路径。
     */
    QString storeSchemeThumbnail(const QString& schemeDir, const QString& sourcePath) const;

    /**
     * @brief 判断路径是否位于指定目录内。
     * @param filePath 待检查的文件路径。
     * @param directory 用于比较的目录路径。
     * @return 若文件位于目录内则返回 true。
     */
    bool isPathWithinDirectory(const QString& filePath, const QString& directory) const;

    /**
     * @brief 检查当前是否存在激活的项目。
     * @return 若存在激活项目则返回 true。
     */
    bool hasActiveProject() const;

    /**
     * @brief 从存储加载方案库。
     */
    void loadSchemeLibrary();

    /**
     * @brief 将方案库保存到存储中。
     */
    void saveSchemeLibrary() const;

    /**
     * @brief 获取方案库的根目录路径。
     * @return 返回库根路径。
     */
    QString schemeLibraryRoot() const;

    /**
     * @brief 在库根路径下生成唯一的子目录名。
     * @param baseName 希望使用的基础名称。
     * @return 返回唯一的目录名称。
     */
    QString makeUniqueLibrarySubdir(const QString& baseName) const;

    /**
     * @brief 根据标识符查找库条目。
     * @param id 条目标识符。
     * @return 返回条目指针，找不到则为 nullptr。
     */
    SchemeLibraryEntry* libraryEntryById(const QString& id);

    /**
     * @brief 根据标识符获取常量库条目。
     * @param id 条目标识符。
     * @return 返回常量条目指针，找不到则为 nullptr。
     */
    const SchemeLibraryEntry* libraryEntryById(const QString& id) const;

    /**
     * @brief 读取库条目的缩略图。
     * @param entry 缩略图来源的库条目。
     * @return 返回缩略图图像。
     */
    QPixmap loadLibraryThumbnail(const SchemeLibraryEntry& entry) const;

    /**
     * @brief 为库条目应用新的缩略图。
     * @param entry 需要更新的条目。
     * @param sourcePath 缩略图的源文件路径。
     */
    void applyLibraryThumbnail(SchemeLibraryEntry& entry, const QString& sourcePath);

    /**
     * @brief 根据标识符移除库条目。
     * @param id 需要移除的条目标识符。
     * @return 移除成功返回 true。
     */
    bool removeLibraryEntry(const QString& id);

    /**
     * @brief 提示用户添加新的方案。
     */
    void promptAddScheme();

    /**
     * @brief 提示用户向指定方案添加新模型。
     * @param schemeId 目标方案标识符。
     */
    void promptAddModel(const QString& schemeId);

    /**
     * @brief 打开指定方案的设置对话框。
     * @param schemeId 需要编辑的方案标识符。
     */
    void openSchemeSettings(const QString& schemeId);

    /**
     * @brief 根据标识符移除方案。
     * @param id 需要删除的方案标识符。
     */
    void removeSchemeById(const QString& id);

    /**
     * @brief 根据标识符移除模型。
     * @param id 需要删除的模型标识符。
     */
    void removeModelById(const QString& id);

    /**
     * @brief 按指纹批量移除方案中的模型。
     * @param scheme 被修改的方案。
     * @param fingerprints 需要移除的指纹集合。
     * @return 若删除了任意模型则返回 true。
     */
    bool removeModelsByFingerprint(SchemeRecord& scheme,
                                   const QSet<QString>& fingerprints);

    /**
     * @brief 提示用户确认删除方案。
     * @param scheme 待删除的方案。
     * @return 若确认删除则返回 true。
     */
    bool confirmSchemeDeletion(const SchemeRecord& scheme);

    /**
     * @brief 提示用户确认删除模型。
     * @param model 待删除的模型。
     * @param owner 模型所属的方案。
     * @return 若确认删除则返回 true。
     */
    bool confirmModelDeletion(const ModelRecord& model, const SchemeRecord& owner);

    /**
     * @brief 将数据模型与当前树形控件状态同步。
     */
    void syncDataFromTree();

    /**
     * @brief 在已有名称集合中生成唯一名称。
     * @param desired 希望使用的名称。
     * @param taken 已占用名称的集合。
     * @param fallback 默认的备用名称。
     * @return 返回唯一的名称。
     */
    QString makeUniqueName(const QString& desired, QSet<QString>& taken,
                           const QString& fallback) const;

    /**
     * @brief 生成唯一的方案名称。
     * @param desired 希望使用的名称。
     * @param excludeId 可选的忽略方案标识符。
     * @return 返回唯一的方案名称。
     */
    QString makeUniqueSchemeName(const QString& desired,
                                 const QString& excludeId = QString()) const;

    /**
     * @brief 在方案内生成唯一的模型名称。
     * @param scheme 包含现有模型的方案。
     * @param desired 希望使用的名称。
     * @param excludeId 可选的忽略模型标识符。
     * @return 返回唯一的模型名称。
     */
    QString makeUniqueModelName(const SchemeRecord& scheme, const QString& desired,
                                const QString& excludeId = QString()) const;

    /**
     * @brief 确保方案中的所有模型名称唯一。
     * @param scheme 需要规范化的方案。
     */
    void ensureUniqueModelNames(SchemeRecord& scheme) const;

    /**
     * @brief 确保所有方案及其模型的名称均唯一。
     */
    void ensureUniqueSchemeAndModelNames();

    /**
     * @brief 从持久化存储加载方案数据。
     * @return 加载成功返回 true。
     */
    bool loadSchemesFromStorage();

    /**
     * @brief 将方案数据写入持久化存储。
     */
    void saveSchemesToStorage() const;

    /**
     * @brief 在修改后保存方案数据。
     */
    void persistSchemes();

    /**
     * @brief 生成唯一的工作区子目录名称。
     * @param baseName 希望使用的基础名称。
     * @return 返回唯一的目录名称。
     */
    QString makeUniqueWorkspaceSubdir(const QString& baseName) const;

    /**
     * @brief 返回当前工作区的根路径。
     * @return 工作区根路径。
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
