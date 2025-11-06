#include "JsonPageBuilder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>
#include <QDateTime>
#include <QTextStream>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStringList>
#include <QDir>
#include <QProgressDialog>

namespace
{
QFileInfo latestResultInfo(const QDir& dir)
{
    const QStringList objPatterns{ QStringLiteral("*.obj"), QStringLiteral("*.OBJ") };
    QFileInfoList files = dir.entryInfoList(objPatterns, QDir::Files,
                                            QDir::Time | QDir::IgnoreCase);
    if (!files.isEmpty())
        return files.first();
    return QFileInfo();
}
}

static const char* kBtnQss =
    "QPushButton {"
    "  background-color: #e0e9f4;"
    "  color: black;"
    "  border: none;"
    "  text-align: left;"
    "  font-size: 15pt;"
    "}";

JsonPageBuilder::JsonPageBuilder(const QString& jsonPath, QWidget* parent)
    : QWidget(parent)
    , m_jsonPath(QFileInfo(jsonPath).absoluteFilePath())
{
    QFileInfo info(m_jsonPath);
    QDir parentDir = info.exists() ? info.dir() : QDir(info.absolutePath());

    // 始终使用 para.json 作为参数文件
    const QFileInfo paraInfo(parentDir.filePath(QStringLiteral("para.json")));
    if (!info.exists() || info.fileName().compare(QStringLiteral("para.json"), Qt::CaseInsensitive) != 0)
    {
        if (paraInfo.exists())
        {
            m_jsonPath = paraInfo.absoluteFilePath();
            info = paraInfo;
        }
    }

    m_modelDirectory = parentDir.absolutePath();

    if (info.exists())
    {
        m_datPath = info.dir().filePath(QStringLiteral("Job-2.dat"));
        m_msgPath = info.dir().filePath(QStringLiteral("Job-2.msg"));
    }
    else
    {
        m_datPath = parentDir.filePath(QStringLiteral("Job-2.dat"));
        m_msgPath = parentDir.filePath(QStringLiteral("Job-2.msg"));
    }

    const QFileInfo batInfo(parentDir.filePath(QStringLiteral("calculate.bat")));
    if (batInfo.exists())
        m_batPath = batInfo.absoluteFilePath();

    QJsonArray sections;
    if (!loadJson(m_jsonPath, sections))
    {
        QMessageBox::critical(this, tr("错误"),
                              tr("无法读取 JSON：%1").arg(m_jsonPath));

        sections = QJsonArray{};
    }
    buildUiFromJson(sections);
}

void JsonPageBuilder::buildUiFromJson(const QJsonArray& sections)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    for (int i = 0; i < sections.size(); ++i)
    {
        const QJsonObject sec = sections.at(i).toObject();
        const QString title = sec.value("title").toString();

        // 标题按钮
        auto* titleBtn = new QPushButton(title, this);
        titleBtn->setMinimumHeight(40);
        titleBtn->setStyleSheet(kBtnQss);
        mainLayout->addWidget(titleBtn);
        m_titleButtons.push_back(titleBtn);

        // 本分组的网格布局：左列标签，右列输入框
        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(8);
        grid->setContentsMargins(6, 0, 6, 6);
        grid->setColumnStretch(0, 0); // label 列不拉伸
        grid->setColumnStretch(1, 1); // edit 列自适应拉伸

        QVector<QLabel*> nameLabels;
        QVector<QLineEdit*> edits;

        const QJsonArray dataList = sec.value("data").toArray();
        for (int row = 0; row < dataList.size(); ++row) {
            const QJsonObject item = dataList.at(row).toObject();
            const QString cnName = item.value("cn_name").toString();
            const QJsonValue val = item.value("value");

            auto* lab = new QLabel(cnName + QString("："), this);
            lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            auto* edit = new QLineEdit(this);
            // JSON 值回显为字符串
            if (val.isDouble()) {
                edit->setText(QString::number(val.toDouble(), 'g', 15));
            } else if (val.isString()) {
                edit->setText(val.toString());
            } else if (val.isBool()) {
                edit->setText(val.toBool() ? QString("1") : QString("0"));
            } else if (val.isArray()) {
                edit->setText(QString::fromUtf8(QJsonDocument(val.toArray())
                                                .toJson(QJsonDocument::Compact)));
            } else if (val.isObject()) {
                edit->setText(QString::fromUtf8(QJsonDocument(val.toObject())
                                                .toJson(QJsonDocument::Compact)));
            } else { // null / undefined
                edit->setText(QString());
            }

            grid->addWidget(lab,  row, 0);
            grid->addWidget(edit, row, 1);

            nameLabels.push_back(lab);
            edits.push_back(edit);
        }

        m_labelNameWidgets.push_back(nameLabels);
        m_labelDataWidgets.push_back(edits);

        mainLayout->addLayout(grid);
    }

    // 计算按钮
    m_calculateButton = new QPushButton(QString("计算"), this);
    m_calculateButton->setMinimumHeight(40);
    connect(m_calculateButton, &QPushButton::clicked,
            this, &JsonPageBuilder::onCalculateButtonClicked);

    mainLayout->addWidget(m_calculateButton);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}


bool JsonPageBuilder::loadJson(const QString& path, QJsonArray& outSections)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray all = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(all, &err);
    if (err.error != QJsonParseError::NoError)
    {
        return false;
    }

    if (doc.isArray())
    {
        outSections = doc.array();
        return true;
    }
    else if (doc.isObject())
    {
        const QJsonArray arr = doc.object().value("data").toArray();
        outSections = arr;
        return true;
    }
    return false;
}

bool JsonPageBuilder::saveJson(const QString& path)
{
    QJsonArray sections;
    if (!loadJson(path, sections))
        return false;

    // 将界面数据写回
    for (int i = 0; i < m_titleButtons.size(); ++i) {
        const QString title = m_titleButtons[i]->text();
        const auto& nameLabs = m_labelNameWidgets[i];
        const auto& edits = m_labelDataWidgets[i];

        for (int j = 0; j < nameLabs.size() && j < edits.size(); ++j) {
            QString cn = nameLabs[j]->text();
            if (cn.endsWith(("：")))
                cn.chop(1);
            const QString valText = edits[j]->text();

            applyEditToJson(sections, title, cn, valText);
        }
    }

    // 保存
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonDocument doc(sections);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    qInfo("成功修改json内容");
    return true;
}

void JsonPageBuilder::applyEditToJson(QJsonArray& sections,
                                    const QString& title,
                                    const QString& cnName,
                                    const QString& valueText)
{
    for (int i = 0; i < sections.size(); ++i) {
        QJsonObject sec = sections[i].toObject();
        if (sec.value("title").toString() == title) {
            QJsonArray dataArr = sec.value("data").toArray();
            for (int j = 0; j < dataArr.size(); ++j) {
                QJsonObject item = dataArr[j].toObject();
                if (item.value("cn_name").toString() == cnName) {
                    item["value"] = strictConvert(valueText);
                    dataArr[j] = item;
                    sec["data"] = dataArr;
                    sections[i] = sec;
                    return;
                }
            }
        }
    }
}

QJsonValue JsonPageBuilder::strictConvert(const QString& text)
{
    bool okInt = false;
    const int i = text.toInt(&okInt);
    if (okInt) return i;

    bool okDbl = false;
    const double d = text.toDouble(&okDbl);
    if (okDbl) return d;

    // 如果既不是 int 也不是 double，就按字符串保存（避免崩溃）
    return text;
}

QString JsonPageBuilder::readWholeFile(const QString& path)
{
    QFile f(path);
    if (!f.exists()) return QString();
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    const QString s = ts.readAll();
    return s;
}

QString JsonPageBuilder::cleanText(QString s)
{
    // 去掉重复空格
    s.replace(QRegularExpression("\\s+"), " ");
    s = s.trimmed();
    return s;
}

QString JsonPageBuilder::extractErrorMsgFromMsg(const QString& content)
{
    // 匹配 ERROR: 到 ANALYSIS SUMMARY 之间的内容（非贪婪）
    QRegularExpression re("ERROR:(.*?)(ANALYSIS SUMMARY|$)",
                          QRegularExpression::DotMatchesEverythingOption);
    auto it = re.globalMatch(content);
    QString last;
    while (it.hasNext()) {
        auto m = it.next();
        last = m.captured(1);
    }
    return cleanText(last);
}

QString JsonPageBuilder::extractErrorMsgFromDat(const QString& content)
{
    // 匹配 ERROR: 到 NOTE 之间的内容（非贪婪）
    QRegularExpression re("ERROR:(.*?)(NOTE|$)",
                          QRegularExpression::DotMatchesEverythingOption);
    auto it = re.globalMatch(content);
    QString last;
    while (it.hasNext()) {
        auto m = it.next();
        last = m.captured(1);
    }
    return cleanText(last);
}

void JsonPageBuilder::onCalculateButtonClicked()
{
    if (m_process)
        return;

    if (m_calculateButton)
        m_calculateButton->setEnabled(false);

    m_calculationTimestamp = QDateTime::currentDateTime()
                                 .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QFileInfo jsonInfo(m_jsonPath);
    if (!jsonInfo.exists())
    {
        const QString warn = tr("未找到参数文件：%1")
                                 .arg(QDir::toNativeSeparators(m_jsonPath));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
        if (m_calculateButton)
            m_calculateButton->setEnabled(true);
        return;
    }

    QDir workingDir;
    if (!m_modelDirectory.isEmpty())
        workingDir = QDir(m_modelDirectory);
    else
        workingDir = jsonInfo.dir();

    if (!workingDir.exists())
    {
        const QString warn = tr("模型目录不存在：%1")
                                 .arg(QDir::toNativeSeparators(workingDir.absolutePath()));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
        if (m_calculateButton)
            m_calculateButton->setEnabled(true);
        return;
    }

    QFileInfo batInfo;
    if (!m_batPath.isEmpty())
        batInfo.setFile(m_batPath);
    const QString expectedBatPath = workingDir.absoluteFilePath(QStringLiteral("calculate.bat"));
    if (!batInfo.exists())
        batInfo.setFile(expectedBatPath);

    if (!batInfo.exists())
    {
        const QString warn = tr("未找到计算脚本：%1")
                                 .arg(QDir::toNativeSeparators(expectedBatPath));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
        if (m_calculateButton)
            m_calculateButton->setEnabled(true);
        return;
    }

    m_batPath = batInfo.absoluteFilePath();

    QFileInfo previousResult = latestResultInfo(workingDir);
    m_previousResultPath = previousResult.exists() ? previousResult.absoluteFilePath() : QString();
    m_previousResultModified = previousResult.exists() ? previousResult.lastModified()
                                                       : QDateTime();
    m_pendingWorkingDirectory = workingDir.absolutePath();
    m_pendingStdOut.clear();
    m_pendingStdErr.clear();

    emit logMessage(tr("开始计算，保存参数到 %1")
                        .arg(QDir::toNativeSeparators(m_jsonPath)));

    if (!saveJson(m_jsonPath))
    {
        const QString warn = tr("保存 JSON 失败：%1")
                                 .arg(QDir::toNativeSeparators(m_jsonPath));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
        m_pendingWorkingDirectory.clear();
        m_previousResultPath.clear();
        m_previousResultModified = QDateTime();
        if (m_calculateButton)
            m_calculateButton->setEnabled(true);
        return;
    }

    emit logMessage(tr("已保存参数，开始执行计算脚本"));

    ensureProgressDialog();
    if (m_progressDialog)
    {
        m_progressDialog->setLabelText(tr("正在计算，请稍候..."));
        m_progressDialog->setRange(0, 0);
        m_progressDialog->show();
    }

    resetCalculationState();

    m_process = new QProcess(this);
    if (workingDir.exists())
        m_process->setWorkingDirectory(workingDir.absolutePath());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &JsonPageBuilder::handleProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &JsonPageBuilder::handleProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &JsonPageBuilder::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &JsonPageBuilder::handleProcessError);

    m_process->start(QStringLiteral("cmd"),
                     QStringList() << QStringLiteral("/c")
                                   << QDir::toNativeSeparators(m_batPath));
}

void JsonPageBuilder::handleProcessOutput()
{
    if (!m_process)
        return;

    const QByteArray out = m_process->readAllStandardOutput();
    if (!out.isEmpty())
        m_pendingStdOut.append(QString::fromLocal8Bit(out));

    const QByteArray err = m_process->readAllStandardError();
    if (!err.isEmpty())
        m_pendingStdErr.append(QString::fromLocal8Bit(err));
}

void JsonPageBuilder::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    handleProcessOutput();
    QString failureReason;
    if (status != QProcess::NormalExit && m_process)
        failureReason = m_process->errorString();
    finalizeCalculation(exitCode, status == QProcess::NormalExit, failureReason);
}

void JsonPageBuilder::handleProcessError(QProcess::ProcessError error)
{
    if (!m_process)
        return;

    if (error == QProcess::FailedToStart)
    {
        handleProcessOutput();
        const QString failure = tr("计算脚本启动失败：%1").arg(m_process->errorString());
        emit logMessage(failure);
        finalizeCalculation(-1, false, failure);
    }
}

void JsonPageBuilder::finalizeCalculation(int exitCode, bool finishedSuccessfully,
                                          const QString& failureReason)
{
    if (m_progressDialog)
    {
        m_progressDialog->hide();
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
    }

    QString stdoutText = m_pendingStdOut;
    QString stderrText = m_pendingStdErr;
    m_pendingStdOut.clear();
    m_pendingStdErr.clear();

    if (!stdoutText.trimmed().isEmpty())
        emit logMessage(tr("输出：%1").arg(stdoutText.trimmed()));
    if (!stderrText.trimmed().isEmpty())
        emit logMessage(tr("错误：%1").arg(stderrText.trimmed()));

    QString timestamp = m_calculationTimestamp;
    if (timestamp.isEmpty())
        timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_calculationTimestamp.clear();

    QString message;
    if (finishedSuccessfully && exitCode == 0)
        message = tr("计算成功，时间：%1").arg(timestamp);

    if (!failureReason.trimmed().isEmpty())
        message = failureReason;

    if (finishedSuccessfully)
    {
        if (QFile::exists(m_msgPath))
        {
            const QString all = readWholeFile(m_msgPath);
            const QString err = extractErrorMsgFromMsg(all);
            if (!err.isEmpty())
                message = tr("错误信息：%1 时间：%2").arg(err, timestamp);
        }
        else if (QFile::exists(m_datPath))
        {
            const QString all = readWholeFile(m_datPath);
            const QString err = extractErrorMsgFromDat(all);
            if (!err.isEmpty())
                message = tr("错误信息：%1 时间：%2").arg(err, timestamp);
        }
    }

    if (message.isEmpty())
    {
        const QString base = finishedSuccessfully
                                 ? tr("计算结束，退出码 %1 时间：%2")
                                 : tr("计算脚本执行异常，退出码 %1 时间：%2");
        message = base.arg(exitCode).arg(timestamp);
        if (!stderrText.trimmed().isEmpty())
            message += QStringLiteral("\n%1").arg(stderrText.trimmed());
    }

    QString newResultPath;
    if (!m_pendingWorkingDirectory.isEmpty())
    {
        QDir workingDir(m_pendingWorkingDirectory);
        QFileInfo latestResult = latestResultInfo(workingDir);
        if (latestResult.exists())
        {
            const bool isNewFile = m_previousResultPath.isEmpty() ||
                                   latestResult.absoluteFilePath() != m_previousResultPath;
            const bool isUpdated = !m_previousResultPath.isEmpty() &&
                                   latestResult.absoluteFilePath() == m_previousResultPath &&
                                   latestResult.lastModified() > m_previousResultModified;
            if (isNewFile || isUpdated)
            {
                newResultPath = latestResult.absoluteFilePath();
                emit logMessage(tr("检测到新的 OBJ 输出：%1")
                                    .arg(QDir::toNativeSeparators(newResultPath)));
            }
        }
    }

    emit logMessage(message);
    emit calculationFinished(newResultPath);
    QMessageBox::information(this, tr("提示框"), message, QMessageBox::Ok);

    resetCalculationState();

    if (m_calculateButton)
        m_calculateButton->setEnabled(true);

    m_pendingWorkingDirectory.clear();
    m_previousResultPath.clear();
    m_previousResultModified = QDateTime();
}

void JsonPageBuilder::resetCalculationState()
{
    if (!m_process)
        return;

    m_process->disconnect(this);
    m_process->deleteLater();
    m_process = nullptr;
}

void JsonPageBuilder::ensureProgressDialog()
{
    if (m_progressDialog)
        return;

    auto* dialog = new QProgressDialog(tr("正在计算，请稍候..."), QString(), 0, 0, this);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(tr("正在计算"));
    dialog->setCancelButton(nullptr);
    dialog->setAutoClose(false);
    dialog->setAutoReset(false);
    dialog->setMinimumDuration(0);
    m_progressDialog = dialog;
}
