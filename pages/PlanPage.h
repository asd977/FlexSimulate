#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <vtkSmartPointer.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QLabel;
class QWidget;
class QSplitter;
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

class SchemeGalleryWidget;
class vtkActor;
class vtkGenericOpenGLRenderWindow;
class vtkRenderer;

class PlanPage : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造与指定 UI 实例绑定的计划页面控制器。
     */
    explicit PlanPage(Ui::MainWindow* ui, QObject* parent = nullptr);

    /**
     * @brief 初始化计划页面的样式、布局以及可视化资源。
     */
    void initialize();

    /**
     * @brief 返回计划页面展示的方案画廊组件。
     */
    SchemeGalleryWidget* gallery() const { return m_galleryWidget; }

    /**
     * @brief 返回当前显示的详情组件。
     */
    QWidget* currentDetailWidget() const { return m_currentDetailWidget; }

    /**
     * @brief 在详情面板中显示传入的组件。
     */
    void setDetailWidget(QWidget* widget);

    /**
     * @brief 移除并删除当前的详情组件。
     */
    void clearDetailWidget();

    /**
     * @brief 控制可视化面板和日志面板的显示或隐藏。
     */
    void setVisualizationVisible(bool visible);

    /**
     * @brief 指示可视化面板当前是否可见。
     */
    bool isVisualizationVisible() const { return m_visualizationVisible; }

    /**
     * @brief 使用提供的路径和备注更新选中信息标签。
     */
    void updateSelectionInfo(const QString& path = QString(),
                             const QString& remark = QString());

    /**
     * @brief 将新的消息追加到运行日志。
     */
    void appendLogMessage(const QString& message);

    /**
     * @brief 在 VTK 视图中加载并渲染指定的结果文件。
     */
    void displayResultFile(const QString& filePath);

    /**
     * @brief 清理 VTK 场景并移除当前的 actor。
     */
    void clearVtkScene();

    /**
     * @brief 返回计划页面使用的渲染器。
     */
    vtkSmartPointer<vtkRenderer> renderer() const { return m_renderer; }

    /**
     * @brief 返回嵌入式 VTK 组件关联的渲染窗口。
     */
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow() const { return m_renderWindow; }

    /**
     * @brief 返回 VTK 场景中当前显示的 actor。
     */
    vtkSmartPointer<vtkActor> currentActor() const { return m_currentActor; }

    /**
     * @brief 设置代表当前可视化结果的 actor。
     */
    void setCurrentActor(const vtkSmartPointer<vtkActor>& actor);

    /**
     * @brief 返回隐藏可视化面板时缓存的分割条尺寸。
     */
    QList<int> lastSplitterSizes() const { return m_lastSplitterSizes; }

    /**
     * @brief 记录重新显示面板时需要恢复的分割条尺寸。
     */
    void setLastSplitterSizes(const QList<int>& sizes);

private:
    /**
     * @brief 对计划页面的组件应用样式与布局调整。
     */
    void setupUiStyles();

    /**
     * @brief 配置计划页面相关分割条的样式和行为。
     */
    void setupSplitterStyles();

    /**
     * @brief 初始化嵌入式 VTK 渲染管线与组件。
     */
    void initializeVisualization();

    Ui::MainWindow* m_ui = nullptr;
    SchemeGalleryWidget* m_galleryWidget = nullptr;
    QWidget* m_currentDetailWidget = nullptr;
    QLabel* m_selectionInfo = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkActor> m_currentActor;
    QList<int> m_lastSplitterSizes;
    bool m_visualizationVisible = false;
};

