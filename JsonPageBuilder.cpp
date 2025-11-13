#include "JsonPageBuilder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QComboBox>
#include <QFrame>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QDebug>
#include <QSet>

namespace
{
QFileInfo latestResultInfo(const QDir& dir)
{
    const QStringList objPatterns{ QString("*.obj"), QString("*.OBJ") };
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
    // 统一字体，避免中文/特殊字符在某些系统字体下显示不好
    this->setFont(QFont(QString("Microsoft YaHei UI"), 9));

    QFileInfo info(m_jsonPath);
    QDir parentDir = info.exists() ? info.dir() : QDir(info.absolutePath());

    // 始终使用 para.json 作为参数文件
    const QFileInfo paraInfo(parentDir.filePath(QString("para.json")));
    if (!info.exists() || info.fileName().compare(QString("para.json"), Qt::CaseInsensitive) != 0)
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
        m_datPath = info.dir().filePath(QString("Job-2.dat"));
        m_msgPath = info.dir().filePath(QString("Job-2.msg"));
    }
    else
    {
        m_datPath = parentDir.filePath(QString("Job-2.dat"));
        m_msgPath = parentDir.filePath(QString("Job-2.msg"));
    }

    const QFileInfo batInfo(parentDir.filePath(QString("calculate.bat")));
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

    m_titleButtons.clear();
    m_labelNameWidgets.clear();
    m_labelDataWidgets.clear();
    m_metalSections.clear();

    m_labelNameWidgets.reserve(sections.size());
    m_labelDataWidgets.reserve(sections.size());
    m_metalSections.reserve(sections.size());

    for (int i = 0; i < sections.size(); ++i)
    {
        const QJsonObject sec      = sections.at(i).toObject();
        const QString title        = sec.value(QString("title")).toString();
        const QString introduction = sec.value(QString("introduction")).toString();
        const QString mattype      = sec.value(QString("mattype")).toString();
        const QJsonArray dataList  = sec.value(QString("data")).toArray();

        // ★★ 关键：先在 m_metalSections 中占一个位置，再通过引用来用它 ★★
        m_metalSections.push_back(MetalSectionControls{});
        MetalSectionControls& metalCtrl = m_metalSections.last();

        // ===== A. 每个 section 一张卡片 =====
        auto* sectionFrame = new QFrame(this);
        sectionFrame->setObjectName(QString("SectionFrame"));
        sectionFrame->setFrameShape(QFrame::StyledPanel);
        sectionFrame->setFrameShadow(QFrame::Plain);
        sectionFrame->setStyleSheet(
            "QFrame#SectionFrame {"
            "  background-color: #f7f9fc;"
            "  border: 1px solid #d0d7e2;"
            "  border-radius: 6px;"
            "}"
        );

        auto* sectionLayout = new QVBoxLayout(sectionFrame);
        sectionLayout->setSpacing(4);
        sectionLayout->setContentsMargins(8, 6, 8, 8);

        // ===== B. 标题按钮 =====
        auto* titleBtn = new QPushButton(title, sectionFrame);
        titleBtn->setMinimumHeight(32);
        titleBtn->setStyleSheet(
            QString::fromLatin1(kBtnQss) +
            "QPushButton { font-size: 13pt; font-weight: bold; }"
        );
        if (!introduction.isEmpty())
            titleBtn->setToolTip(introduction);

        sectionLayout->addWidget(titleBtn);
        m_titleButtons.push_back(titleBtn);

        // ===== C. introduction 说明文字 =====
        if (!introduction.isEmpty())
        {
            auto* introLabel = new QLabel(introduction, sectionFrame);
            introLabel->setWordWrap(true);
            QFont f = introLabel->font();
            f.setPointSizeF(f.pointSizeF() - 1);
            introLabel->setFont(f);
            introLabel->setStyleSheet(QString("color: #666666;"));
            introLabel->setContentsMargins(2, 0, 2, 2);
            sectionLayout->addWidget(introLabel);
        }

        QVector<QLabel*>    nameLabels;
        QVector<QLineEdit*> edits;

        const bool isMetal =
            !mattype.isEmpty() &&
            mattype.compare(QString("metal"), Qt::CaseInsensitive) == 0;

        if (!isMetal)
        {
            // ===== 非 metal：按原始 data 简单生成 label + edit =====
            auto* grid = new QGridLayout();
            grid->setHorizontalSpacing(12);
            grid->setVerticalSpacing(6);
            grid->setContentsMargins(0, 2, 0, 0);
            grid->setColumnStretch(0, 0);
            grid->setColumnStretch(1, 1);

            auto createEdit = [&](const QJsonValue& val, QWidget* parent) -> QLineEdit*
            {
                auto* edit = new QLineEdit(parent);
                if (val.isDouble()) {
                    edit->setText(QString::number(val.toDouble(), 'g', 15));
                } else if (val.isString()) {
                    edit->setText(val.toString());
                } else if (val.isBool()) {
                    edit->setText(val.toBool() ? QString("1") : QString("0"));
                } else if (val.isArray()) {
                    edit->setText(QString::fromUtf8(
                        QJsonDocument(val.toArray()).toJson(QJsonDocument::Compact)));
                } else if (val.isObject()) {
                    edit->setText(QString::fromUtf8(
                        QJsonDocument(val.toObject()).toJson(QJsonDocument::Compact)));
                } else {
                    edit->setText(QString());
                }
                return edit;
            };

            int row = 0;
            for (const QJsonValue& v : dataList)
            {
                const QJsonObject item = v.toObject();
                const QString cnName   = item.value(QString("cn_name")).toString();
                const QJsonValue val   = item.value(QString("value"));

                auto* lab  = new QLabel(cnName + QString("："), sectionFrame);
                lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                if (!introduction.isEmpty())
                    lab->setToolTip(introduction);

                auto* edit = createEdit(val, sectionFrame);

                grid->addWidget(lab,  row, 0);
                grid->addWidget(edit, row, 1);

                nameLabels.push_back(lab);
                edits.push_back(edit);
                ++row;
            }

            sectionLayout->addLayout(grid);

            metalCtrl.isMetal = false;   // 非 metal
        }
        else
        {
            // ===== metal：固定四种模型，可添加 / 删除 =====
            metalCtrl.isMetal       = true;
            metalCtrl.introduction  = introduction;
            metalCtrl.gridNextRow   = 0;

            // “添加材料性质” 行
            auto* addLayout = new QHBoxLayout();
            addLayout->setContentsMargins(0, 0, 0, 2);

            auto* addLabel = new QLabel(tr("添加材料性质："), sectionFrame);
            if (!introduction.isEmpty())
                addLabel->setToolTip(introduction);

            auto* combo = new QComboBox(sectionFrame);
            combo->addItem(tr("密度 density"),          QString("density"));
            combo->addItem(tr("弹性 elastic"),          QString("elastic"));
            combo->addItem(tr("塑性 plastic"),          QString("plastic"));
            combo->addItem(tr("超弹性 hyperelastic"),   QString("hyperelastic"));
            if (!introduction.isEmpty())
                combo->setToolTip(introduction);

            auto* addBtn = new QPushButton(tr("添加"), sectionFrame);
            if (!introduction.isEmpty())
                addBtn->setToolTip(introduction);

            addLayout->addWidget(addLabel);
            addLayout->addWidget(combo);
            addLayout->addWidget(addBtn);
            addLayout->addStretch();
            sectionLayout->addLayout(addLayout);

            metalCtrl.addModelCombo  = combo;
            metalCtrl.addModelButton = addBtn;

            // 模型网格布局
            auto* grid = new QGridLayout();
            grid->setHorizontalSpacing(12);
            grid->setVerticalSpacing(4);
            grid->setContentsMargins(0, 2, 0, 0);
            grid->setColumnStretch(0, 0);
            grid->setColumnStretch(1, 1);

            metalCtrl.grid = grid;

            // 预创建四种 key
            metalCtrl.models[QString("density")];
            metalCtrl.models[QString("elastic")];
            metalCtrl.models[QString("plastic")];
            metalCtrl.models[QString("hyperelastic")];

            // 添加按钮逻辑：根据下拉框当前选择添加模型
            const int sectionIndex = i;
            connect(addBtn, &QPushButton::clicked, this, [this, sectionIndex, combo]() {
                const QString key = combo->currentData().toString();
                if (!key.isEmpty())
                    addMetalModel(sectionIndex, key);
            });

            // 先根据现有 data 中的 model 列出有哪些模型
            QSet<QString> modelKeys;
            for (const QJsonValue& v : dataList) {
                const QJsonObject item = v.toObject();
                const QString model = item.value(QString("model")).toString();
                if (!model.isEmpty())
                    modelKeys.insert(model);
            }

            // ★★ 这里现在 addMetalModel 真正能拿到 m_metalSections[sectionIndex] ★★
            for (const QString& mk : modelKeys) {
                addMetalModel(i, mk);
            }

            // 再把数据填回对应的输入框
            for (const QJsonValue& v : dataList)
            {
                const QJsonObject item = v.toObject();
                const QString model = item.value(QString("model")).toString();
                const QString name  = item.value(QString("name")).toString();
                const QJsonValue val = item.value(QString("value"));

                if (!metalCtrl.models.contains(model))
                    continue;
                MetalModelInfo& info = metalCtrl.models[model];
                if (!info.present)
                    continue;

                QLineEdit* edit = info.editsByName.value(name, nullptr);
                if (!edit)
                    continue;

                if (val.isDouble())
                    edit->setText(QString::number(val.toDouble(), 'g', 15));
                else
                    edit->setText(val.toVariant().toString());
            }

            sectionLayout->addLayout(grid);
        }

        m_labelNameWidgets.push_back(nameLabels);
        m_labelDataWidgets.push_back(edits);

        mainLayout->addWidget(sectionFrame);
    }

    // 计算按钮
    m_calculateButton = new QPushButton(tr("计算"), this);
    m_calculateButton->setMinimumHeight(40);
    connect(m_calculateButton, &QPushButton::clicked,
            this, &JsonPageBuilder::onCalculateButtonClicked);

    mainLayout->addWidget(m_calculateButton);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

// ===== metal 模型添加 / 删除 =====

void JsonPageBuilder::addMetalModel(int sectionIndex, const QString& modelKey)
{
    if (sectionIndex < 0 || sectionIndex >= m_metalSections.size())
        return;

    MetalSectionControls& mc = m_metalSections[sectionIndex];
    if (!mc.isMetal || !mc.grid)
        return;

    if (!mc.models.contains(modelKey))
        mc.models[modelKey] = MetalModelInfo{};

    MetalModelInfo& info = mc.models[modelKey];
    if (info.present)
    {
        QString modelName;
        if (modelKey == QString("density"))       modelName = tr("密度 density");
        else if (modelKey == QString("elastic"))  modelName = tr("弹性 elastic");
        else if (modelKey == QString("plastic"))  modelName = tr("塑性 plastic");
        else if (modelKey == QString("hyperelastic")) modelName = tr("超弹性 hyperelastic");
        else modelName = modelKey;

        QMessageBox::warning(this, tr("提示"),
                             tr("该材料的 %1 模型已存在，不能重复添加。").arg(modelName));
        return;
    }

    const QString intro = mc.introduction;

    // 标题行：模型名 + 删除按钮
    QString headerText;
    if (modelKey == QString("density"))
        headerText = tr("模型：密度 density");
    else if (modelKey == QString("elastic"))
        headerText = tr("模型：弹性 elastic");
    else if (modelKey == QString("plastic"))
        headerText = tr("模型：塑性 plastic");
    else if (modelKey == QString("hyperelastic"))
        headerText = tr("模型：超弹性 hyperelastic");
    else
        headerText = tr("模型：%1").arg(modelKey);

    int row = mc.gridNextRow;

    auto* headerLabel = new QLabel(headerText, this);
    QFont hf = headerLabel->font();
    hf.setBold(true);
    headerLabel->setFont(hf);
    headerLabel->setStyleSheet(QString("color: #004a7f;"));
    if (!intro.isEmpty())
        headerLabel->setToolTip(intro);

    auto* removeBtn = new QPushButton(tr("删除"), this);
    removeBtn->setFixedWidth(60);

    mc.grid->addWidget(headerLabel, row, 0);
    mc.grid->addWidget(removeBtn, row, 1, Qt::AlignRight);
    mc.gridNextRow++;

    info.labels.push_back(headerLabel);
    info.removeButton = removeBtn;

    // 删除按钮逻辑
    connect(removeBtn, &QPushButton::clicked,
            this, [this, sectionIndex, modelKey]() {
        removeMetalModel(sectionIndex, modelKey);
    });

    // 不同模型的参数行
    auto addParamRow = [&](const QString& name,
                           const QString& cnName) -> QLineEdit*
    {
        auto* lab = new QLabel(cnName + QString("："), this);
        lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        if (!intro.isEmpty())
            lab->setToolTip(intro);

        auto* edit = new QLineEdit(this);

        mc.grid->addWidget(lab,  mc.gridNextRow, 0);
        mc.grid->addWidget(edit, mc.gridNextRow, 1);
        mc.gridNextRow++;

        info.labels.push_back(lab);
        info.editsByName.insert(name, edit);
        return edit;
    };

    if (modelKey == QString("density"))
    {
        addParamRow(QString("D"), QString("密度ρ"));
    }
    else if (modelKey == QString("elastic"))
    {
        addParamRow(QString("E"), QString("弹性模量E"));
        addParamRow(QString("u"), QString("泊松比v"));
    }
    else if (modelKey == QString("plastic"))
    {
        addParamRow(QString("YS"),  QString("屈服强度YS"));
        addParamRow(QString("e"),   QString("断后延伸率A"));
        addParamRow(QString("UTS"), QString("抗拉强度UTS"));
    }
    else if (modelKey == QString("hyperelastic"))
    {
        addParamRow(QString("C10"), QString("C10"));
        addParamRow(QString("C01"), QString("C01"));
        addParamRow(QString("D1"),  QString("D1"));
    }
    else
    {
        // 未知模型：不做参数行
    }

    info.present = true;
}

void JsonPageBuilder::removeMetalModel(int sectionIndex, const QString& modelKey)
{
    if (sectionIndex < 0 || sectionIndex >= m_metalSections.size())
        return;

    MetalSectionControls& mc = m_metalSections[sectionIndex];
    if (!mc.isMetal || !mc.grid)
        return;

    if (!mc.models.contains(modelKey))
        return;

    MetalModelInfo& info = mc.models[modelKey];
    if (!info.present)
        return;

    // 删除标题 & 参数标签
    for (QLabel* lab : info.labels) {
        if (!lab) continue;
        mc.grid->removeWidget(lab);
        lab->hide();
        lab->deleteLater();
    }
    info.labels.clear();

    // 删除输入框
    for (auto it = info.editsByName.begin(); it != info.editsByName.end(); ++it) {
        QLineEdit* edit = it.value();
        if (!edit) continue;
        mc.grid->removeWidget(edit);
        edit->hide();
        edit->deleteLater();
    }
    info.editsByName.clear();

    // 删除按钮
    if (info.removeButton) {
        mc.grid->removeWidget(info.removeButton);
        info.removeButton->hide();
        info.removeButton->deleteLater();
        info.removeButton = nullptr;
    }

    info.present = false;
}

// ===== JSON 读写 =====

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
        const QJsonArray arr = doc.object().value(QString("data")).toArray();
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

    const int secCount = qMin(sections.size(), m_titleButtons.size());

    for (int i = 0; i < secCount; ++i) {
        QJsonObject secObj = sections.at(i).toObject();
        const QString title   = secObj.value(QString("title")).toString();
        const QString mattype = secObj.value(QString("mattype")).toString();

        const bool isMetal =
            !mattype.isEmpty() &&
            mattype.compare(QString("metal"), Qt::CaseInsensitive) == 0;

        if (!isMetal) {
            // 非 metal：只更新 value
            const auto& nameLabs = m_labelNameWidgets[i];
            const auto& edits    = m_labelDataWidgets[i];

            for (int j = 0; j < nameLabs.size() && j < edits.size(); ++j) {
                QString cn = nameLabs[j]->text();
                if (cn.endsWith(QString("：")) || cn.endsWith(":"))
                    cn.chop(1);
                cn = cn.trimmed();

                const QString valText = edits[j]->text();
                applyEditToJson(sections, title, cn, valText);
            }
        } else {
            // metal：按 UI 中存在的模型重建 data 数组
            QJsonArray newData;
            if (i < m_metalSections.size()) {
                const MetalSectionControls& mc = m_metalSections[i];

                auto appendModel = [&](const QString& modelKey,
                                       const QString& name,
                                       const QString& cnName,
                                       const QLineEdit* edit)
                {
                    if (!mc.models.contains(modelKey))
                        return;
                    const MetalModelInfo& info = mc.models[modelKey];
                    if (!info.present)
                        return;
                    if (!edit)
                        return;
                    QJsonObject obj;
                    obj[QString("model")]   = modelKey;
                    obj[QString("name")]    = name;
                    obj[QString("cn_name")] = cnName;
                    obj[QString("value")]   = strictConvert(edit->text());
                    newData.append(obj);
                };

                const MetalModelInfo& densityInfo = mc.models.value(QString("density"));
                if (densityInfo.present) {
                    appendModel(QString("density"), QString("D"),
                                QString("密度ρ"),
                                densityInfo.editsByName.value(QString("D"), nullptr));
                }

                const MetalModelInfo& elasticInfo = mc.models.value(QString("elastic"));
                if (elasticInfo.present) {
                    appendModel(QString("elastic"), QString("E"),
                                QString("弹性模量E"),
                                elasticInfo.editsByName.value(QString("E"), nullptr));
                    appendModel(QString("elastic"), QString("u"),
                                QString("泊松比v"),
                                elasticInfo.editsByName.value(QString("u"), nullptr));
                }

                const MetalModelInfo& plasticInfo = mc.models.value(QString("plastic"));
                if (plasticInfo.present) {
                    appendModel(QString("plastic"), QString("YS"),
                                QString("屈服强度YS"),
                                plasticInfo.editsByName.value(QString("YS"), nullptr));
                    appendModel(QString("plastic"), QString("e"),
                                QString("断后延伸率A"),
                                plasticInfo.editsByName.value(QString("e"), nullptr));
                    appendModel(QString("plastic"), QString("UTS"),
                                QString("抗拉强度UTS"),
                                plasticInfo.editsByName.value(QString("UTS"), nullptr));
                }

                const MetalModelInfo& hyperInfo = mc.models.value(QString("hyperelastic"));
                if (hyperInfo.present) {
                    appendModel(QString("hyperelastic"), QString("C10"),
                                QString("C10"),
                                hyperInfo.editsByName.value(QString("C10"), nullptr));
                    appendModel(QString("hyperelastic"), QString("C01"),
                                QString("C01"),
                                hyperInfo.editsByName.value(QString("C01"), nullptr));
                    appendModel(QString("hyperelastic"), QString("D1"),
                                QString("D1"),
                                hyperInfo.editsByName.value(QString("D1"), nullptr));
                }
            }

            secObj[QString("data")] = newData;
            sections[i] = secObj;
        }
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonDocument doc(sections);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    qInfo("成功修改 json 内容");
    return true;
}

void JsonPageBuilder::applyEditToJson(QJsonArray& sections,
                                      const QString& title,
                                      const QString& cnName,
                                      const QString& valueText)
{
    for (int i = 0; i < sections.size(); ++i) {
        QJsonObject sec = sections[i].toObject();
        if (sec.value(QString("title")).toString() == title) {
            QJsonArray dataArr = sec.value(QString("data")).toArray();
            for (int j = 0; j < dataArr.size(); ++j) {
                QJsonObject item = dataArr[j].toObject();
                if (item.value(QString("cn_name")).toString() == cnName) {
                    item[QString("value")] = strictConvert(valueText);
                    dataArr[j] = item;
                    sec[QString("data")] = dataArr;
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

// ===== 文件读取 / 文本处理 =====

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

// ===== 计算相关 =====

void JsonPageBuilder::onCalculateButtonClicked()
{
    if (m_process)
        return;

    if (m_calculateButton)
        m_calculateButton->setEnabled(false);

    m_calculationTimestamp = QDateTime::currentDateTime()
                                 .toString(QString("yyyy-MM-dd HH:mm:ss"));

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
    const QString expectedBatPath = workingDir.absoluteFilePath(QString("calculate.bat"));
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

    m_process->start(QString("cmd"),
                     QStringList() << QString("/c")
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
        timestamp = QDateTime::currentDateTime().toString(QString("yyyy-MM-dd HH:mm:ss"));
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
            message += QString("\n%1").arg(stderrText.trimmed());
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
