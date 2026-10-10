#pragma once
#include "project.h"
#include "viewport.h"
#include "snapping.h"
#include "shape_builder.h"
#include <QWidget>
#include <array>
#include <utility>

namespace fontu {
std::vector<GlyphSeed> digitGlyphs();
enum class Tool { Select, Rectangle, Ellipse, Pen, ShapeBuilder };
enum class RenderMode { XRay, Opaque };
class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr);
    Canvas(std::vector<GlyphSeed> glyphs, QWidget *parent = nullptr);
    const Document &document() const { return activeGlyph().document; }
    std::vector<GlyphSeed> glyphs() const;
    GlyphId activeGlyphId() const { return activeGlyph().seed.id; }
    bool setActiveGlyph(GlyphId id);
    ProjectSnapshot projectSnapshot() const;
    void restoreProject(ProjectSnapshot project);
    void cancelPendingInput();
    const Viewport &viewport() const { return viewport_; }
    Tool tool() const { return tool_; }
    RenderMode renderMode() const { return renderMode_; }
    void setRenderMode(RenderMode mode);
    QString controlsHint() const;
    void setTool(Tool tool);
    void resetView();
    bool canUndo() const { return hasPenHistory() ? !penDraft_.empty() : activeGlyph().historyPosition > 0; }
    bool canRedo() const { return hasPenHistory() ? !penRedo_.empty() : activeGlyph().historyPosition < activeGlyph().history.size(); }
    void undo();
    void redo();
    bool gridSnapEnabled() const { return snapOptions_.grid; }
    bool geometrySnapEnabled() const { return snapOptions_.geometry; }
    void setGridSnapEnabled(bool enabled);
    void setGeometrySnapEnabled(bool enabled);
signals:
    void activeGlyphChanged();
    void glyphsChanged();
    void toolChanged(Tool tool);
    void renderModeChanged(RenderMode mode);
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
    enum class Gesture { Idle, Draw, Move, Resize, Pan, PenAnchor, Build };
    void cancelGesture();
    void refreshBuilder();
    void extendBuilder(const QPointF &point);
    struct BuilderGesture {
        BuilderOperation operation;
        std::vector<std::size_t> touched;
        QPointF last;
    };
    struct BuilderSession {
        explicit BuilderSession(const Document &document) : geometry(document) {}
        ShapeBuilder geometry;
        std::optional<std::size_t> hovered;
        std::optional<BuilderGesture> gesture;
    };
    std::optional<BuilderSession> builder_;
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
    struct HistoryEntry { Document before; Document after; };
    struct GlyphRecord {
        GlyphSeed seed;
        Document document;
        std::vector<HistoryEntry> history;
        std::size_t historyPosition = 0;
    };
    GlyphRecord &activeGlyph() { return glyphs_[activeGlyphIndex_]; }
    const GlyphRecord &activeGlyph() const { return glyphs_[activeGlyphIndex_]; }
    std::vector<GlyphRecord> glyphs_;
    std::size_t activeGlyphIndex_ = 0;
    std::optional<Document> gestureBefore_;
    Viewport viewport_;
    Tool tool_ = Tool::Select;
    RenderMode renderMode_ = RenderMode::XRay;
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
