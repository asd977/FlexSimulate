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
     * @brief 构造基于 JSON 的页面生成器。
     * @param jsonPath JSON 配置文件路径。
     * @param parent 可选的父组件。
     */
    explicit JsonPageBuilder(const QString& jsonPath,
                             QWidget* parent = nullptr);

signals:
    /**
     * @brief 当生成器产生日志信息时发出。
     * @param message 日志文本内容。
     */
    void logMessage(const QString& message);

    /**
     * @brief 计算成功结束时发出。
     * @param stlPath 生成的 STL 文件路径。
     */
    void calculationFinished(const QString& stlPath);

private slots:
    /**
     * @brief 在点击计算按钮后启动计算。
     */
    void onCalculateButtonClicked();

    /**
     * @brief 处理外部计算进程结束。
     * @param exitCode 进程退出码。
     * @param status 进程退出状态。
     */
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);

    /**
     * @brief 处理外部计算进程的错误。
     * @param error 进程错误类型。
     */
    void handleProcessError(QProcess::ProcessError error);

    /**
     * @brief 读取计算进程产生的增量输出。
     */
    void handleProcessOutput();

private:
    /**
     * @brief 根据 JSON 定义构建界面。
     * @param sections 解析得到的分组数据。
     */
    void buildUiFromJson(const QJsonArray& sections);

    /**
     * @brief 从磁盘加载 JSON 数据。
     * @param path 需要读取的文件路径。
     * @param outSections 输出的分组数组。
     * @return 读取成功返回 true。
     */
    bool loadJson(const QString& path, QJsonArray& outSections);

    /**
     * @brief 将 JSON 数据写回磁盘。
     * @param path 目标文件路径。
     * @return 写入成功返回 true。
     */
    bool saveJson(const QString& path);

    /**
     * @brief 将编辑后的值写入 JSON 文档。
     * @param sections 需要修改的 JSON 分组。
     * @param title 待更新的分组标题。
     * @param cnName 字段对应的中文名称。
     * @param valueText 新的值文本。
     */
    void applyEditToJson(QJsonArray& sections,
                         const QString& title,
                         const QString& cnName,
                         const QString& valueText);

    /**
     * @brief 将用户输入转换为严格类型的 JSON 值。
     * @param text 待转换的文本。
     * @return 转换后的 JSON 值。
     */
    static QJsonValue strictConvert(const QString& text);

    /**
     * @brief 读取文件全部内容并返回字符串。
     * @param path 需要读取的文件路径。
     * @return 文件内容字符串。
     */
    static QString readWholeFile(const QString& path);

    /**
     * @brief 从 Abaqus 的 .msg 文件中提取错误信息。
     * @param content 消息文件的内容。
     * @return 提取出的错误描述。
     */
    static QString extractErrorMsgFromMsg(const QString& content);

    /**
     * @brief 从 Abaqus 的 .dat 文件中提取错误信息。
     * @param content Dat 文件内容。
     * @return 提取出的错误描述。
     */
    static QString extractErrorMsgFromDat(const QString& content);

    /**
     * @brief 清理文本中不需要的字符。
     * @param s 待处理的文本。
     * @return 清理后的文本。
     */
    static QString cleanText(QString s);

    /**
     * @brief 在计算完成后执行成功或失败的收尾逻辑。
     * @param exitCode 进程退出码。
     * @param finishedSuccessfully 是否成功完成。
     * @param failureReason 可选的失败原因描述。
     */
    void finalizeCalculation(int exitCode, bool finishedSuccessfully,
                             const QString& failureReason = QString());

    /**
     * @brief 在计算结束或取消后重置界面状态。
     */
    void resetCalculationState();

    /**
     * @brief 确保进度对话框存在并完成配置。
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
