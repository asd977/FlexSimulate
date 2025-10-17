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
     * @brief Constructs a plan page controller bound to the given UI instance.
     */
    explicit PlanPage(Ui::MainWindow* ui, QObject* parent = nullptr);

    /**
     * @brief Sets up styles, layouts, and visualization resources for the plan page.
     */
    void initialize();

    /**
     * @brief Returns the gallery widget shown on the plan page.
     */
    SchemeGalleryWidget* gallery() const { return m_galleryWidget; }

    /**
     * @brief Returns the currently displayed detail widget.
     */
    QWidget* currentDetailWidget() const { return m_currentDetailWidget; }

    /**
     * @brief Displays the provided widget inside the detail panel.
     */
    void setDetailWidget(QWidget* widget);

    /**
     * @brief Removes and deletes the current detail widget.
     */
    void clearDetailWidget();

    /**
     * @brief Shows or hides the visualization panel and log panel.
     */
    void setVisualizationVisible(bool visible);

    /**
     * @brief Indicates whether the visualization panel is visible.
     */
    bool isVisualizationVisible() const { return m_visualizationVisible; }

    /**
     * @brief Updates the selection information label with the provided path and remark.
     */
    void updateSelectionInfo(const QString& path = QString(),
                             const QString& remark = QString());

    /**
     * @brief Appends a new message to the run log.
     */
    void appendLogMessage(const QString& message);

    /**
     * @brief Loads and renders the specified result file inside the VTK viewer.
     */
    void displayResultFile(const QString& filePath);

    /**
     * @brief Clears the VTK scene and removes the current actor.
     */
    void clearVtkScene();

    /**
     * @brief Returns the renderer associated with the plan page visualization.
     */
    vtkSmartPointer<vtkRenderer> renderer() const { return m_renderer; }

    /**
     * @brief Returns the render window attached to the embedded VTK widget.
     */
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow() const { return m_renderWindow; }

    /**
     * @brief Returns the actor currently displayed in the VTK scene.
     */
    vtkSmartPointer<vtkActor> currentActor() const { return m_currentActor; }

    /**
     * @brief Sets the actor that represents the current visualization result.
     */
    void setCurrentActor(const vtkSmartPointer<vtkActor>& actor);

    /**
     * @brief Returns the cached splitter sizes used when hiding the visualization panel.
     */
    QList<int> lastSplitterSizes() const { return m_lastSplitterSizes; }

    /**
     * @brief Stores the splitter sizes that should be restored when showing the panel again.
     */
    void setLastSplitterSizes(const QList<int>& sizes);

private:
    void setupUiStyles();
    void setupSplitterStyles();
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

