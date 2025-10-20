#pragma once
#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;

class SchemeSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief 构造带初始值的方案设置对话框。
     * @param schemeName 初始方案名称。
     * @param workingDirectory 初始工作目录。
     * @param allowDirectoryChange 是否允许更改目录。
     * @param parent 可选的父组件。
     * @param thumbnailPath 已有缩略图的路径。
     */
    explicit SchemeSettingsDialog(const QString& schemeName,
                                  const QString& workingDirectory = QString(),
                                  bool allowDirectoryChange = true,
                                  QWidget* parent = nullptr,
                                  const QString& thumbnailPath = QString());

    /**
     * @brief 获取用户输入的方案名称。
     * @return 方案名称。
     */
    QString schemeName() const;

    /**
     * @brief 获取用户选择的工作目录。
     * @return 目录路径。
     */
    QString workingDirectory() const;

    /**
     * @brief 获取当前选择的缩略图路径。
     * @return 缩略图文件路径。
     */
    QString thumbnailPath() const;

    /**
     * @brief 设置方案名称输入框的值。
     * @param name 需要显示的方案名称。
     */
    void setSchemeName(const QString& name);

    /**
     * @brief 设置工作目录输入框的值。
     * @param directory 需要显示的目录路径。
     */
    void setWorkingDirectory(const QString& directory);

    /**
     * @brief 设置缩略图路径并更新预览。
     * @param path 需要应用的缩略图路径。
     */
    void setThumbnailPath(const QString& path);

    /**
     * @brief 更新目录选择区域的提示文本。
     * @param hint 需要显示的提示内容。
     */
    void setDirectoryHint(const QString& hint);

private slots:
    /**
     * @brief 打开目录选择器以设置工作目录。
     */
    void browseForDirectory();

    /**
     * @brief 打开文件选择器以挑选缩略图。
     */
    void browseForThumbnail();

    /**
     * @brief 清除当前选择的缩略图。
     */
    void clearThumbnail();

protected:
    /**
     * @brief 处理调整大小事件以保持布局。
     * @param event 调整大小事件信息。
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief 根据当前路径更新缩略图预览。
     */
    void updateThumbnailPreview();

    QLabel* m_title = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_directoryEdit = nullptr;
    QPushButton* m_browseButton = nullptr;
    QLabel* m_thumbnailPreview = nullptr;
    QPushButton* m_thumbnailButton = nullptr;
    QPushButton* m_clearThumbnailButton = nullptr;
    QString m_thumbnailPath;
    bool m_directoryEditable = true;
};
