#pragma once

#include <QWidget>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QDateTime>

class QProgressDialog;

class JsonPageBuilder : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a JSON-driven page builder for a given file.
     * @param jsonPath Path to the JSON configuration file.
     * @param parent Optional parent widget.
     */
    explicit JsonPageBuilder(const QString& jsonPath,
                             QWidget* parent = nullptr);

signals:
    /**
     * @brief Emitted when a log message is produced by the builder.
     * @param message Text of the log message.
     */
    void logMessage(const QString& message);

    /**
     * @brief Emitted when a calculation finishes successfully.
     * @param stlPath Path to the generated STL file.
     */
    void calculationFinished(const QString& stlPath);

private slots:
    /**
     * @brief Starts calculation when the calculate button is clicked.
     */
    void onCalculateButtonClicked();

    /**
     * @brief Handles completion of the external calculation process.
     * @param exitCode Process exit code.
     * @param status Process exit status.
     */
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);

    /**
     * @brief Handles errors from the external calculation process.
     * @param error Reported process error.
     */
    void handleProcessError(QProcess::ProcessError error);

    /**
     * @brief Reads incremental output from the calculation process.
     */
    void handleProcessOutput();

private:
    /**
     * @brief Builds the UI based on JSON section definitions.
     * @param sections Sections parsed from the JSON.
     */
    void buildUiFromJson(const QJsonArray& sections);

    /**
     * @brief Loads JSON data from disk.
     * @param path File path to load.
     * @param outSections Output sections array.
     * @return True if the JSON was loaded successfully.
     */
    bool loadJson(const QString& path, QJsonArray& outSections);

    /**
     * @brief Saves JSON data back to disk.
     * @param path Destination path.
     * @return True if the file was written successfully.
     */
    bool saveJson(const QString& path);

    /**
     * @brief Applies edited values to the JSON document.
     * @param sections JSON sections to modify.
     * @param title Section title to update.
     * @param cnName Chinese label associated with the field.
     * @param valueText New value text.
     */
    void applyEditToJson(QJsonArray& sections,
                         const QString& title,
                         const QString& cnName,
                         const QString& valueText);

    /**
     * @brief Converts user text to a strictly typed JSON value.
     * @param text Input text to convert.
     * @return Converted JSON value.
     */
    static QJsonValue strictConvert(const QString& text);

    /**
     * @brief Reads an entire file into a string.
     * @param path File path to read.
     * @return Contents of the file.
     */
    static QString readWholeFile(const QString& path);

    /**
     * @brief Extracts an error message from an Abaqus .msg file.
     * @param content Message file contents.
     * @return Extracted error description.
     */
    static QString extractErrorMsgFromMsg(const QString& content);

    /**
     * @brief Extracts an error message from an Abaqus .dat file.
     * @param content Dat file contents.
     * @return Extracted error description.
     */
    static QString extractErrorMsgFromDat(const QString& content);

    /**
     * @brief Cleans text by removing undesired characters.
     * @param s Text to sanitize.
     * @return Cleaned text value.
     */
    static QString cleanText(QString s);

    /**
     * @brief Finalizes calculation handling success or failure paths.
     * @param exitCode Process exit code.
     * @param finishedSuccessfully Whether the run succeeded.
     * @param failureReason Optional failure description.
     */
    void finalizeCalculation(int exitCode, bool finishedSuccessfully,
                             const QString& failureReason = QString());

    /**
     * @brief Resets state after a calculation completes or is cancelled.
     */
    void resetCalculationState();

    /**
     * @brief Ensures the progress dialog exists and is configured.
     */
    void ensureProgressDialog();

private:
    // 对应 Python 中的三个列表
    QVector<QPushButton*> m_titleButtons;                // 每个分组标题按钮
    QVector<QVector<QLabel*>> m_labelNameWidgets;        // 每组内的标签（中文名）
    QVector<QVector<QLineEdit*>> m_labelDataWidgets;     // 每组内的输入框（值）

    QPushButton* m_calculateButton = nullptr;

    QString m_jsonPath;                                   // para.json
    QString m_modelDirectory;                             // 模型目录
    QString m_batPath;                                    // calculate.bat
    QString m_datPath = QStringLiteral("Job-2.dat");
    QString m_msgPath = QStringLiteral("Job-2.msg");
    QProcess* m_process = nullptr;
    QPointer<QProgressDialog> m_progressDialog;
    QString m_pendingStdOut;
    QString m_pendingStdErr;
    QString m_pendingWorkingDirectory;
    QString m_previousResultPath;
    QDateTime m_previousResultModified;
    QString m_calculationTimestamp;
};
