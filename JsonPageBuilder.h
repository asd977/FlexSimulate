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
    explicit JsonPageBuilder(const QString& jsonPath,
                             QWidget* parent = nullptr);

signals:
    void logMessage(const QString& message);
    void calculationFinished(const QString& resultPath);

private slots:
    void onCalculateButtonClicked();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessOutput();

private:
    void buildUiFromJson(const QJsonArray& sections);
    bool loadJson(const QString& path, QJsonArray& outSections);
    bool saveJson(const QString& path);
    void applyEditToJson(QJsonArray& sections,
                         const QString& title,
                         const QString& cnName,
                         const QString& valueText);
    static QJsonValue strictConvert(const QString& text);
    static QString readWholeFile(const QString& path);
    static QString extractErrorMsgFromMsg(const QString& content);
    static QString extractErrorMsgFromDat(const QString& content);
    static QString cleanText(QString s);
    void finalizeCalculation(int exitCode, bool finishedSuccessfully,
                             const QString& failureReason = QString());
    void resetCalculationState();
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
