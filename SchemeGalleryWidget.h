#pragma once
#include <QWidget>
#include <QPointer>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class SchemeGalleryWidget; }
QT_END_NAMESPACE

class SchemeCardWidget;

class SchemeGalleryWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs the scheme gallery widget.
     * @param parent Optional parent widget.
     */
    explicit SchemeGalleryWidget(QWidget *parent = nullptr);

    /**
     * @brief Destroys the gallery widget.
     */
    ~SchemeGalleryWidget();

    struct CardOptions {
        bool showDeleteButton = true;
        bool enableDeleteButton = true;
        bool showOpenButton = false;
        bool enableOpenButton = true;
        QString hintText;
        QString deleteToolTip;
        QString openToolTip;
    };

    /**
     * @brief Removes all scheme cards from the gallery.
     */
    void clearSchemes();

    /**
     * @brief Adds a scheme card to the gallery.
     * @param id Identifier of the scheme.
     * @param name Display name of the scheme.
     * @param thumb Optional thumbnail image.
     * @param options Additional card configuration options.
     */
    void addScheme(const QString& id,
                   const QString& name = QString(),
                   const QPixmap& thumb = QPixmap(),
                   const CardOptions& options = CardOptions());

    /**
     * @brief Removes a scheme card by identifier.
     * @param id Scheme identifier to remove.
     */
    void removeSchemeById(const QString& id);

signals:
    /**
     * @brief Emitted when a scheme should be opened from the gallery.
     * @param id Scheme identifier.
     */
    void schemeOpenRequested(const QString& id);

    /**
     * @brief Emitted when a scheme should be deleted from the gallery.
     * @param id Scheme identifier.
     */
    void schemeDeleteRequested(const QString& id);

    /**
     * @brief Emitted when the user requests more scheme details.
     * @param id Scheme identifier.
     */
    void schemeDetailsRequested(const QString& id);

    /**
     * @brief Emitted when the user wants to create a new scheme.
     */
    void createSchemeRequested();

protected:
    /**
     * @brief Recomputes the layout when the widget is resized.
     * @param e Resize event information.
     */
    void resizeEvent(QResizeEvent* e) override;

private:
    /**
     * @brief Arranges cards according to the current geometry.
     */
    void relayoutCards();

private:
    Ui::SchemeGalleryWidget *ui;
    QList<QPointer<SchemeCardWidget>> m_cards;
    int m_cardW = 268;                // 估算卡片宽度（含间距）
    int m_lastColumnCount = 0;
};
