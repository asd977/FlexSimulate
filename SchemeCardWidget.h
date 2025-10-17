#pragma once
#include <QFrame>
#include <QPixmap>

class QLabel;
class QToolButton;

class SchemeCardWidget : public QFrame {
    Q_OBJECT
public:
    /**
     * @brief Constructs a scheme card widget representing the given scheme.
     * @param id Unique identifier of the scheme.
     * @param parent Optional parent widget.
     */
    explicit SchemeCardWidget(const QString& id, QWidget* parent=nullptr);

    /**
     * @brief Sets the title text displayed on the card.
     * @param title Title text to show.
     */
    void setTitle(const QString& title);

    /**
     * @brief Returns the current card title.
     * @return Title text.
     */
    QString title() const;

    /**
     * @brief Updates the thumbnail image shown on the card.
     * @param pm Thumbnail pixmap.
     */
    void setThumbnail(const QPixmap& pm);

    /**
     * @brief Sets helper text shown beneath the card title.
     * @param text Hint text to display.
     */
    void setHintText(const QString& text);

    /**
     * @brief Controls visibility of the delete button.
     * @param visible Whether the button is visible.
     */
    void setDeleteButtonVisible(bool visible);

    /**
     * @brief Enables or disables the delete button.
     * @param enabled Whether the button is enabled.
     */
    void setDeleteButtonEnabled(bool enabled);

    /**
     * @brief Sets the tooltip text for the delete button.
     * @param text Tooltip text.
     */
    void setDeleteButtonToolTip(const QString& text);

    /**
     * @brief Controls visibility of the open button.
     * @param visible Whether the button is visible.
     */
    void setOpenButtonVisible(bool visible);

    /**
     * @brief Enables or disables the open button.
     * @param enabled Whether the button is enabled.
     */
    void setOpenButtonEnabled(bool enabled);

    /**
     * @brief Sets the tooltip text for the open button.
     * @param text Tooltip text.
     */
    void setOpenButtonToolTip(const QString& text);

    /**
     * @brief Returns the identifier associated with the card.
     * @return Scheme identifier.
     */
    QString id() const { return m_id; }

signals:
    /**
     * @brief Emitted when the card should open its scheme.
     * @param id Scheme identifier.
     */
    void openRequested(const QString& id);

    /**
     * @brief Emitted when the user requests deletion of the scheme.
     * @param id Scheme identifier.
     */
    void deleteRequested(const QString& id);

    /**
     * @brief Emitted when the user requests more scheme details.
     * @param id Scheme identifier.
     */
    void detailsRequested(const QString& id);

protected:
    /**
     * @brief Handles mouse press events to trigger detail requests.
     * @param ev Mouse event information.
     */
    void mousePressEvent(QMouseEvent* ev) override;

    /**
     * @brief Handles double clicks to open the scheme quickly.
     * @param ev Mouse event information.
     */
    void mouseDoubleClickEvent(QMouseEvent* ev) override;

    /**
     * @brief Responds to resize events to keep the layout consistent.
     * @param ev Resize event information.
     */
    void resizeEvent(QResizeEvent* ev) override;

private:
    /**
     * @brief Updates the thumbnail display to maintain aspect ratio.
     */
    void updateThumbnailDisplay();

    /**
     * @brief Checks if a point lies within a tool button.
     * @param button Button to test.
     * @param pos Point in widget coordinates.
     * @return True if the point is inside the button.
     */
    bool isPointInsideButton(const QToolButton* button, const QPoint& pos) const;

    QString m_id;
    QLabel* m_imageLabel;
    QLabel* m_titleLabel;
    QToolButton* m_deleteBtn;
    QToolButton* m_openBtn;
    QLabel* m_hintLabel;
    QPixmap m_thumbnail;
};
