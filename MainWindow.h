#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QHash>
#include <QPixmap>
#include <QUrl>
#include <QDir>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTreeWidgetItem;
class QWidget;
class QShortcut;
class SchemeGalleryWidget;

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
    void onGalleryAddRequested();
    void onGalleryOpenRequested(const QString& id);
    void onGalleryDeleteRequested(const QString& id);
    void deleteCurrentTreeItem();

private:
    struct ModelRecord {
        QString id;
        QString name;
        QString directory;
        QString jsonPath;
        QString batPath;
    };

    struct SchemeRecord {
        QString id;
        QString name;
        QString directory;
        QVector<ModelRecord> models;
    };

    enum TreeRoles {
        TypeRole = Qt::UserRole,
        IdRole,
        SchemeRole
    };

    enum TreeItemType {
        SchemeItem = 1,
        ModelItem
    };

    void setupUiHelpers();
    void setupConnections();
    void loadInitialSchemes();

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

    SchemeRecord* schemeById(const QString& id);
    const SchemeRecord* schemeById(const QString& id) const;
    SchemeRecord* schemeByDirectory(const QString& canonicalPath);
    ModelRecord* modelById(const QString& id, SchemeRecord** owner = nullptr);
    const ModelRecord* modelById(const QString& id, const SchemeRecord** owner = nullptr) const;

    QString importSchemeFromDirectory(const QString& dirPath, bool showError = true);
    QVector<QString> importModelsIntoScheme(const QString& schemeId,
                                            const QStringList& paths,
                                            bool showError = true);
    bool isModelFolder(const QDir& dir, QString* jsonPath, QString* batPath) const;
    QVector<ModelRecord> scanSchemeFolder(const QString& schemeDir) const;
    QPixmap makeSchemePlaceholder(const QString& name) const;
    void promptAddScheme();
    void promptAddModel(const QString& schemeId);
    void removeSchemeById(const QString& id);
    void removeModelById(const QString& id);
    void syncDataFromTree();

    Ui::MainWindow *ui;
    SchemeGalleryWidget* m_galleryWidget = nullptr;
    QWidget* m_currentDetailWidget = nullptr;
    QVector<SchemeRecord> m_schemes;
    QHash<QString, QTreeWidgetItem*> m_schemeItems;
    QHash<QString, QTreeWidgetItem*> m_modelItems;
    QString m_activeSchemeId;
    QString m_activeModelId;
    bool m_blockTreeSignals = false;
};
