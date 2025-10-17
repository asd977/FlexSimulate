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
    explicit PlanPage(Ui::MainWindow* ui, QObject* parent = nullptr);

    void initialize();

    SchemeGalleryWidget* gallery() const { return m_galleryWidget; }
    QWidget* currentDetailWidget() const { return m_currentDetailWidget; }
    void setDetailWidget(QWidget* widget);
    void clearDetailWidget();

    void setVisualizationVisible(bool visible);
    bool isVisualizationVisible() const { return m_visualizationVisible; }
    void updateSelectionInfo(const QString& path = QString(),
                             const QString& remark = QString());
    void appendLogMessage(const QString& message);
    void displayResultFile(const QString& filePath);
    void clearVtkScene();

    vtkRenderer* renderer() const { return m_renderer; }
    vtkGenericOpenGLRenderWindow* renderWindow() const { return m_renderWindow; }
    vtkActor* currentActor() const { return m_currentActor; }
    void setCurrentActor(vtkActor* actor);
    QList<int> lastSplitterSizes() const { return m_lastSplitterSizes; }
    void setLastSplitterSizes(const QList<int>& sizes);

private:
    void setupUiStyles();
    void setupSplitterStyles();
    void initializeVisualization();

    Ui::MainWindow* m_ui = nullptr;
    SchemeGalleryWidget* m_galleryWidget = nullptr;
    QWidget* m_currentDetailWidget = nullptr;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkActor> m_currentActor;
    QList<int> m_lastSplitterSizes;
    bool m_visualizationVisible = false;
};

