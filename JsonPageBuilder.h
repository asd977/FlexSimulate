#pragma once

#include <QWidget>
#include <QVector>
#include <QDateTime>
#include <QJsonArray>
#include <QMap>
#include <QString>
#include <QProcess>

class QPushButton;
class QLabel;
class QLineEdit;
class QProcess;
class QProgressDialog;
class QCheckBox;
class QComboBox;
class QGridLayout;

class JsonPageBuilder : public QWidget
{
    Q_OBJECT
public:
    explicit JsonPageBuilder(const QString& jsonPath, QWidget* parent = nullptr);

    struct MaterialPreset
    {
        QString key;                     // 材料唯一 key
        QString displayName;             // 下拉框显示文本
        QMap<QString, QString> valuesByFieldKey; // 参数名 (如 D/YS) -> 数值
        QMap<QString, QString> valuesByLabel;    // 中文名 (如 密度ρ) -> 数值
    };

    void setAvailableMaterials(const QVector<MaterialPreset>& materials);

signals:
    void logMessage(const QString& msg);
    void calculationFinished(const QString& objPath);
    void materialPresetSelected(const QString& materialKey);

private slots:
    void onCalculateButtonClicked();
    void handleProcessOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);

private:
    struct MetalModelInfo;
    struct MetalSectionControls;

    // ===== JSON & UI =====
    bool loadJson(const QString& path, QJsonArray& outSections);
    bool saveJson(const QString& path);
    void buildUiFromJson(const QJsonArray& sections);
    void applyEditToJson(QJsonArray& sections,
                         const QString& title,
                         const QString& cnName,
                         const QString& valueText);
    QJsonValue strictConvert(const QString& text);

    // metal section：添加 / 删除模型
    void addMetalModel(int sectionIndex, const QString& modelKey);
    void removeMetalModel(int sectionIndex, const QString& modelKey);
    void populateMaterialCombo(QComboBox* combo) const;
    void updateMaterialApplyState(MetalSectionControls& controls);
    void notifyMaterialSelectionChanged(MetalSectionControls& controls);
    void applyMaterialPreset(int sectionIndex);
    void applyPresetToSection(int sectionIndex, const MaterialPreset& preset);
    const MaterialPreset* findMaterialPreset(const QString& key) const;

    // ===== 文件读取 / 文本处理 =====
    QString readWholeFile(const QString& path);
    QString cleanText(QString s);
    QString extractErrorMsgFromMsg(const QString& content);
    QString extractErrorMsgFromDat(const QString& content);

    // ===== 计算流程 =====
    void finalizeCalculation(int exitCode, bool finishedSuccessfully,
                             const QString& failureReason);
    void resetCalculationState();
    void ensureProgressDialog();

private:
    // metal section 中每个模型的控件集合
    struct MetalModelInfo {
        bool present = false;                          // 当前模型是否已经被添加
        QPushButton* removeButton = nullptr;           // “删除”按钮
        QVector<QLabel*> labels;                       // 包含模型标题 + 参数标签
        QMap<QString, QLineEdit*> editsByFieldKey;     // name -> 对应输入框指针
        QMap<QString, QLineEdit*> editsByLabel;        // cn_name -> 对应输入框指针
    };

    // 每个 section 的 metal 控件集合
    struct MetalSectionControls {
        bool isMetal = false;
        QString introduction;

        QComboBox*  addModelCombo  = nullptr;
        QPushButton* addModelButton = nullptr;
        QComboBox*  materialPresetCombo = nullptr;
        QPushButton* applyMaterialButton = nullptr;
        QGridLayout* grid = nullptr;
        int gridNextRow = 0;

        // key: "density" / "elastic" / "plastic" / "hyperelastic"
        QMap<QString, MetalModelInfo> models;
    };

private:
    // 路径 & 目录
    QString m_jsonPath;
    QString m_modelDirectory;
    QString m_datPath;
    QString m_msgPath;
    QString m_batPath;

    // UI 组件缓存（非 metal）
    QVector<QPushButton*>          m_titleButtons;
    QVector<QVector<QLabel*>>      m_labelNameWidgets;  // 非 metal：section -> labels
    QVector<QVector<QLineEdit*>>   m_labelDataWidgets;  // 非 metal：section -> edits

    // metal section 控件，与 sections 一一对应
    QVector<MetalSectionControls>  m_metalSections;
    QVector<MaterialPreset>        m_materialPresets;

    QPushButton*                   m_calculateButton = nullptr;

    // 进程相关
    QProcess*        m_process = nullptr;
    QString          m_pendingWorkingDirectory;
    QString          m_pendingStdOut;
    QString          m_pendingStdErr;
    QString          m_previousResultPath;
    QDateTime        m_previousResultModified;

    // 状态 & 对话框
    QString          m_calculationTimestamp;
    QProgressDialog* m_progressDialog = nullptr;
};
