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
     * @brief Constructs a scheme settings dialog with initial values.
     * @param schemeName Initial scheme name.
     * @param workingDirectory Initial working directory.
     * @param allowDirectoryChange Whether the directory can be changed.
     * @param parent Optional parent widget.
     * @param thumbnailPath Path to an existing thumbnail image.
     */
    explicit SchemeSettingsDialog(const QString& schemeName,
                                  const QString& workingDirectory = QString(),
                                  bool allowDirectoryChange = true,
                                  QWidget* parent = nullptr,
                                  const QString& thumbnailPath = QString());

    /**
     * @brief Returns the scheme name entered by the user.
     * @return Scheme name value.
     */
    QString schemeName() const;

    /**
     * @brief Returns the working directory selected by the user.
     * @return Directory path.
     */
    QString workingDirectory() const;

    /**
     * @brief Returns the chosen thumbnail path.
     * @return Thumbnail file path.
     */
    QString thumbnailPath() const;

    /**
     * @brief Sets the scheme name field value.
     * @param name Scheme name to display.
     */
    void setSchemeName(const QString& name);

    /**
     * @brief Sets the working directory field value.
     * @param directory Directory path to display.
     */
    void setWorkingDirectory(const QString& directory);

    /**
     * @brief Sets the thumbnail path and updates preview.
     * @param path Thumbnail path to assign.
     */
    void setThumbnailPath(const QString& path);

    /**
     * @brief Updates the hint label for the directory selection.
     * @param hint Hint text to display.
     */
    void setDirectoryHint(const QString& hint);

private slots:
    /**
     * @brief Opens a directory picker for working directory selection.
     */
    void browseForDirectory();

    /**
     * @brief Opens a file picker to select a thumbnail image.
     */
    void browseForThumbnail();

    /**
     * @brief Clears the currently selected thumbnail image.
     */
    void clearThumbnail();

protected:
    /**
     * @brief Handles resize events to maintain layout.
     * @param event Resize event information.
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief Updates the thumbnail preview widget from the current path.
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
