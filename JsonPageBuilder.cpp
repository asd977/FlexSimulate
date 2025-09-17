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
    , m_jsonPath(jsonPath)
{
    setWindowTitle(("广汽APP-Demo"));
    setMinimumWidth(500);

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

    for (int i = 0; i < sections.size(); ++i)
    {
        const QJsonObject sec = sections.at(i).toObject();
        const QString title = sec.value("title").toString();

        auto* titleBtn = new QPushButton(title, this);
        titleBtn->setMinimumHeight(40);
        titleBtn->setStyleSheet(kBtnQss);

        mainLayout->addWidget(titleBtn);
        m_titleButtons.push_back(titleBtn);

        QVector<QLabel*> nameLabels;
        QVector<QLineEdit*> edits;

        const QJsonArray dataList = sec.value("data").toArray();
        for (int j = 0; j < dataList.size(); ++j) {
            const QJsonObject item = dataList.at(j).toObject();
            const QString cnName = item.value("cn_name").toString();
            const QJsonValue val = item.value("value");

            auto* h = new QHBoxLayout();
            auto* lab = new QLabel(cnName + ("："), this);
            lab->setMinimumWidth(80);
            lab->setMinimumHeight(40);

            auto* edit = new QLineEdit(this);
            // 将 JSON 值回显为字符串
            if (val.isDouble()) {
                edit->setText(QString::number(val.toDouble(), 'g', 15));
            } else if (val.isString()) {
                edit->setText(val.toString());
            } else if (val.isBool()) {
                edit->setText(val.toBool() ? "1" : "0");
            } else if (val.isNull() || val.isUndefined()) {
                edit->setText("");
            } else {
                // 其他类型（对象/数组）不在本场景中，转成字符串
                edit->setText(QString::fromUtf8(QJsonDocument(val.toObject()).toJson(QJsonDocument::Compact)));
            }

            h->addWidget(lab, 1);
            h->addWidget(edit, 2);
            mainLayout->addLayout(h);

            nameLabels.push_back(lab);
            edits.push_back(edit);
        }

        m_labelNameWidgets.push_back(nameLabels);
        m_labelDataWidgets.push_back(edits);
    }

    m_calculateButton = new QPushButton(("计算"), this);
    m_calculateButton->setMinimumHeight(40);
    connect(m_calculateButton, &QPushButton::clicked,
            this, &JsonPageBuilder::onCalculateButtonClicked);

    mainLayout->addWidget(m_calculateButton);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
    resize(400, 600);
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

    QString message;

    // 1) 先保存 JSON
    if (!saveJson(m_jsonPath)) {
        QMessageBox::warning(this, tr("警告"),
                             tr("保存 JSON 失败：%1").arg(m_jsonPath));
        m_calculateButton->setEnabled(true);
        return;
    }

    // 2) 执行外部命令（Windows 下：cmd /c calculate.bat）
    int exitCode = -1;
    QString stderrText, stdoutText;
    try {
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start("cmd", QStringList() << "/c" << "calculate.bat");
        // 若完全隐藏窗口，可把 "cmd /c" 换成直接 p.start("calculate.bat")
        // 或使用: p.start("cmd", {"/c","start","/B","/WAIT","calculate.bat"});
        const bool ok = p.waitForFinished(-1);
        stdoutText = QString::fromLocal8Bit(p.readAllStandardOutput());
        stderrText = QString::fromLocal8Bit(p.readAllStandardError());
        exitCode = ok ? p.exitCode() : -1;
    } catch (...) {
        exitCode = -1;
    }

    if (exitCode == 0) {
        message = QString("compute successfully At %1").arg(now);
    }

    // 3) 检测 .msg
    if (QFile::exists(m_msgPath)) {
        const QString all = readWholeFile(m_msgPath);
        const QString err = extractErrorMsgFromMsg(all);
        if (!err.isEmpty()) {
            message = QString("error info: %1 At %2").arg(err, now);
        }
    }
    // 4) 否则检测 .dat
    else if (QFile::exists(m_datPath)) {
        const QString all = readWholeFile(m_datPath);
        QString err = extractErrorMsgFromDat(all);
        if (!err.isEmpty()) {
            message = QString("error info: %1 At %2").arg(err, now);
        }
    }

    if (message.isEmpty()) {
        // 兜底信息：既无错误也无成功码
        message = QString("finished with code %1 At %2")
                      .arg(exitCode)
                      .arg(now);
        if (!stderrText.trimmed().isEmpty())
            message += QString("\n%1").arg(stderrText.trimmed());
    }

    QMessageBox::information(this, ("提示框"),
                             message, QMessageBox::Ok);
    m_calculateButton->setEnabled(true);
}
