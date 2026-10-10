#pragma once
#include "document.h"
#include "viewport.h"
#include "snapping.h"
#include <QWidget>
#include <array>
#include <utility>

namespace fontu {
enum class Tool { Select, Rectangle, Ellipse, Pen };
class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr);
    const Document &document() const { return document_; }
    const Viewport &viewport() const { return viewport_; }
    Tool tool() const { return tool_; }
    QString controlsHint() const;
    void setTool(Tool tool);
    void resetView();
    bool canUndo() const { return hasPenHistory() ? !penDraft_.empty() : historyPosition_ > 0; }
    bool canRedo() const { return hasPenHistory() ? !penRedo_.empty() : historyPosition_ < history_.size(); }
    void undo();
    void redo();
    bool gridSnapEnabled() const { return snapOptions_.grid; }
    bool geometrySnapEnabled() const { return snapOptions_.geometry; }
    void setGridSnapEnabled(bool enabled);
    void setGeometrySnapEnabled(bool enabled);
signals:
    void toolChanged(Tool tool);
    void snapOptionsChanged();
    void historyChanged();
    void controlsHintChanged(const QString &hint);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void leaveEvent(QEvent *) override;
private:
    enum class Gesture { Idle, Draw, Move, Resize, Pan, PenAnchor };
    void cancelGesture();
    bool hasPenHistory() const { return !penDraft_.empty() || !penRedo_.empty(); }
    bool stepPenHistory(bool forward);
    void recordEdit(Document before);
    void updateGesture(const QPointF &screenPosition);
    void updateCursor(const QPointF &screenPosition);
    void updateHoverSnap();
    bool canClosePen(const QPointF &screenPosition) const;
    std::optional<std::pair<SnapResult, QPointF>> moveSnap(const QRectF &screenBounds, ShapeId excluded) const;
    QRectF selectionScreenBounds() const;
    std::array<QPointF, 8> handles(const QRectF &screenBounds) const;
    int handleAt(const QPointF &screenPosition) const;
    SnapOptions snapOptions_;
    std::optional<QPointF> snapMarker_;
    std::optional<QPointF> hoverPosition_;
    Document document_;
    struct HistoryEntry { Document before; Document after; };
    std::vector<HistoryEntry> history_;
    std::size_t historyPosition_ = 0;
    std::optional<Document> gestureBefore_;
    Viewport viewport_;
    Tool tool_ = Tool::Select;
    Gesture gesture_ = Gesture::Idle;
    bool spaceHeld_ = false;
    bool viewInitialized_ = false;
    Qt::MouseButton gestureButton_ = Qt::NoButton;
    QPointF startScreen_, lastScreen_, startDocument_;
    QPainterPath originalPath_, preview_;
    std::vector<PenAnchor> penDraft_;
    std::vector<PenAnchor> penRedo_;
    std::optional<ShapeId> editingId_;
    int resizeHandle_ = -1;
};
}
