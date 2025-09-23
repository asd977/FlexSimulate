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

namespace
{
QFileInfo latestResultInfo(const QDir& dir)
{
    const QStringList stepPatterns{ QStringLiteral("*.step"), QStringLiteral("*.STEP"),
                                    QStringLiteral("*.stp"), QStringLiteral("*.STP") };
    QFileInfoList files = dir.entryInfoList(stepPatterns, QDir::Files,
                                            QDir::Time | QDir::IgnoreCase);
    if (!files.isEmpty())
        return files.first();

    const QStringList stlPatterns{ QStringLiteral("*.stl"), QStringLiteral("*.STL") };
    files = dir.entryInfoList(stlPatterns, QDir::Files,
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
    m_calculateButton->setEnabled(false);
    const QString now = QDateTime::currentDateTime()
                        .toString("yyyy-MM-dd HH:mm:ss");

    QFileInfo jsonInfo(m_jsonPath);
    if (!jsonInfo.exists())
    {
        const QString warn = tr("未找到参数文件：%1")
                                 .arg(QDir::toNativeSeparators(m_jsonPath));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
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
        m_calculateButton->setEnabled(true);
        return;
    }

    QFileInfo batInfo;
    if (!m_batPath.isEmpty())
        batInfo.setFile(m_batPath);
    const QString expectedBatPath = workingDir.absoluteFilePath(QStringLiteral("calculate.bat"));
    if (!batInfo.exists())
    {
        batInfo.setFile(expectedBatPath);
    }

    if (!batInfo.exists())
    {
        const QString warn = tr("未找到计算脚本：%1")
                                 .arg(QDir::toNativeSeparators(expectedBatPath));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
        m_calculateButton->setEnabled(true);
        return;
    }

    m_batPath = batInfo.absoluteFilePath();

    QFileInfo previousResult = latestResultInfo(workingDir);

    emit logMessage(tr("开始计算，保存参数到 %1")
                        .arg(QDir::toNativeSeparators(m_jsonPath)));

    // 1) 先保存 JSON
    if (!saveJson(m_jsonPath)) {
        const QString warn = tr("保存 JSON 失败：%1")
                                 .arg(QDir::toNativeSeparators(m_jsonPath));
        emit logMessage(warn);
        QMessageBox::warning(this, tr("警告"), warn);
        m_calculateButton->setEnabled(true);
        return;
    }

    emit logMessage(tr("已保存参数，开始执行计算脚本"));

    // 2) 执行外部命令（Windows 下：cmd /c calculate.bat）
    int exitCode = -1;
    QString stderrText, stdoutText;
    QProcess process;
    if (workingDir.exists())
        process.setWorkingDirectory(workingDir.absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral("cmd"),
                  QStringList() << QStringLiteral("/c")
                                << QDir::toNativeSeparators(m_batPath));
    const bool finished = process.waitForFinished(-1);
    stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
    stderrText = QString::fromLocal8Bit(process.readAllStandardError());
    exitCode = finished ? process.exitCode() : -1;

    if (!finished) {
        emit logMessage(tr("计算脚本执行异常：%1")
                            .arg(process.errorString()));
    }

    if (!stdoutText.trimmed().isEmpty())
        emit logMessage(tr("输出：%1").arg(stdoutText.trimmed()));
    if (!stderrText.trimmed().isEmpty())
        emit logMessage(tr("错误：%1").arg(stderrText.trimmed()));

    QString message;
    if (exitCode == 0) {
        message = tr("计算成功，时间：%1").arg(now);
    }

    // 3) 检测 .msg
    if (QFile::exists(m_msgPath)) {
        const QString all = readWholeFile(m_msgPath);
        const QString err = extractErrorMsgFromMsg(all);
        if (!err.isEmpty()) {
            message = tr("错误信息：%1 时间：%2").arg(err, now);
        }
    }
    // 4) 否则检测 .dat
    else if (QFile::exists(m_datPath)) {
        const QString all = readWholeFile(m_datPath);
        QString err = extractErrorMsgFromDat(all);
        if (!err.isEmpty()) {
            message = tr("错误信息：%1 时间：%2").arg(err, now);
        }
    }

    if (message.isEmpty()) {
        // 兜底信息：既无错误也无成功码
        message = tr("计算结束，退出码 %1 时间：%2")
                      .arg(exitCode)
                      .arg(now);
        if (!stderrText.trimmed().isEmpty())
            message += QStringLiteral("\n%1").arg(stderrText.trimmed());
    }

    emit logMessage(message);

    QFileInfo latestResult = latestResultInfo(workingDir);
    QString newResultPath;
    if (latestResult.exists()) {
        const bool isNewFile = !previousResult.exists() ||
                               latestResult.absoluteFilePath() != previousResult.absoluteFilePath();
        const bool isUpdated = previousResult.exists() &&
                               latestResult.absoluteFilePath() == previousResult.absoluteFilePath() &&
                               latestResult.lastModified() > previousResult.lastModified();
        if (isNewFile || isUpdated)
        {
            newResultPath = latestResult.absoluteFilePath();
            const QString suffix = latestResult.suffix().toLower();
            const QString fileType = (suffix == QStringLiteral("stl")) ? QStringLiteral("STL")
                                                                       : QStringLiteral("STEP");
            emit logMessage(tr("检测到新的 %1 输出：%2")
                                .arg(fileType,
                                     QDir::toNativeSeparators(newResultPath)));
        }
    }

    emit calculationFinished(newResultPath);

    QMessageBox::information(this, tr("提示框"),
                             message, QMessageBox::Ok);
    m_calculateButton->setEnabled(true);
}
