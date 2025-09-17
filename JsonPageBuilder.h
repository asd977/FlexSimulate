#pragma once

#include <QWidget>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonObject>

class JsonPageBuilder : public QWidget
{
    Q_OBJECT
public:
    explicit JsonPageBuilder(const QString& jsonPath,
                           QWidget* parent = nullptr);

private slots:
    void onCalculateButtonClicked();

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

private:
    // 对应 Python 中的三个列表
    QVector<QPushButton*> m_titleButtons;                // 每个分组标题按钮
    QVector<QVector<QLabel*>> m_labelNameWidgets;        // 每组内的标签（中文名）
    QVector<QVector<QLineEdit*>> m_labelDataWidgets;     // 每组内的输入框（值）

    QPushButton* m_calculateButton = nullptr;

    QString m_jsonPath;                                   // para.json
    QString m_datPath = QStringLiteral("Job-2.dat");
    QString m_msgPath = QStringLiteral("Job-2.msg");
};
