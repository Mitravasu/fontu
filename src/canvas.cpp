#include "canvas.h"
#include "project_creation.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace fontu {
namespace {
constexpr double handleSize = 8;
constexpr double hitTolerance = 5;
constexpr double penCloseDistance = 8;
SnapAxes resizeAxes(int handle) {
    return handle == 1 || handle == 5 ? SnapAxes::Vertical
         : handle == 3 || handle == 7 ? SnapAxes::Horizontal : SnapAxes::Both;
}
QPen cosmeticPen(const QColor &color, double width = 1) {
    QPen pen(color, width);
    pen.setCosmetic(true);
    return pen;
}
}
std::vector<GlyphSeed> digitGlyphs() {
    std::vector<GlyphSeed> result;
    for (const auto c : builtinCharacterText(BuiltinCharacters::Numbers))
        result.push_back({{std::uint64_t(result.size() + 1)}, QString(c)});
    return result;
}
Canvas::Canvas(QWidget *parent) : Canvas(digitGlyphs(), parent) {}
Canvas::Canvas(std::vector<GlyphSeed> glyphs, QWidget *parent) : QWidget(parent) {
    if (glyphs.empty()) throw std::invalid_argument("A glyph collection must contain a glyph");
    std::unordered_set<std::uint64_t> ids;
    for (auto &glyph : glyphs) {
        if (!ids.insert(glyph.id.value).second)
            throw std::invalid_argument("Glyph IDs must be unique within a collection");
        if (glyph.label.isEmpty()) throw std::invalid_argument("A glyph must have a display label");
        glyphs_.push_back({std::move(glyph), {}, {}, 0});
    }
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    setAutoFillBackground(false);
    const auto publishHint = [this] { emit controlsHintChanged(controlsHint()); };
    connect(this, &Canvas::toolChanged, this, publishHint);
    connect(this, &Canvas::historyChanged, this, publishHint);
    refreshReference();
}
std::vector<GlyphSeed> Canvas::glyphs() const {
    std::vector<GlyphSeed> result;
    result.reserve(glyphs_.size());
    for (const auto &glyph : glyphs_) result.push_back(glyph.seed);
    return result;
}
bool Canvas::setActiveGlyph(GlyphId id) {
    const auto target = std::find_if(glyphs_.begin(), glyphs_.end(), [id](const GlyphRecord &glyph) {
        return glyph.seed.id == id;
    });
    if (target == glyphs_.end()) return false;
    const auto index = std::size_t(target - glyphs_.begin());
    if (index == activeGlyphIndex_) return true;
    cancelGesture();
    spaceHeld_ = false;
    hoverPosition_.reset();
    gestureButton_ = Qt::NoButton;
    resizeHandle_ = -1;
    activeGlyphIndex_ = index;
    refreshBuilder();
    refreshReference();
    setCursor(tool_ == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    emit activeGlyphChanged();
    emit historyChanged();
    update();
    return true;
}
ProjectSnapshot Canvas::projectSnapshot() const {
    ProjectSnapshot result{document().definition(), {}, activeGlyphId()};
    for (std::size_t i = 0; i < glyphs_.size(); ++i) {
        const auto &glyph = glyphs_[i];
        const auto &completed = i == activeGlyphIndex_ && gestureBefore_ ? *gestureBefore_ : glyph.document;
        result.glyphs.push_back({glyph.seed, completed.shapes()});
    }
    return result;
}
void Canvas::cancelPendingInput() {
    cancelGesture();
    spaceHeld_ = false;
    gestureButton_ = Qt::NoButton;
    hoverPosition_.reset();
    setCursor(tool_ == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}
void Canvas::restoreProject(ProjectSnapshot project) {
    std::vector<GlyphRecord> replacement;
    std::size_t activeIndex = 0;
    for (auto &glyph : project.glyphs) {
        if (glyph.seed.id == project.activeGlyph) activeIndex = replacement.size();
        replacement.push_back({std::move(glyph.seed), Document(project.definition, std::move(glyph.shapes)), {}, 0});
    }
    cancelPendingInput();
    glyphs_ = std::move(replacement);
    activeGlyphIndex_ = activeIndex;
    refreshBuilder();
    refreshReference();
    resetView();
    emit glyphsChanged();
    emit activeGlyphChanged();
    emit historyChanged();
    update();
}
QString Canvas::controlsHint() const {
    if (tool_ == Tool::ShapeBuilder) {
        if (!builder_ || builder_->geometry.regions().empty())
            return QStringLiteral("Select closed fills with Shift+click or Select All, then B • Open strokes ignored");
        return QStringLiteral("Click a region • Drag merges • Alt removes • Esc cancels • Space+drag pans");
    }
    if (tool_ == Tool::Pen) {
        return (penDraft_.size() >= 3
            ? QStringLiteral("Click first anchor to fill • Enter finishes")
            : QStringLiteral("Click anchors • Enter finishes"))
            + QStringLiteral(" • Drag to curve • Esc cancels • Wheel zooms • Space+drag pans");
    }
    if (tool_ == Tool::Select)
        return QStringLiteral("Shift+click selects more • Drag moves • Handles resize • Delete selection • Wheel zooms • Space+drag pans");
    return QStringLiteral("Drag draws or moves • Handles resize • Delete selection • Esc cancels • Wheel zooms • Space+drag pans");
}
void Canvas::recordEdit(Document before) {
    const auto &oldShapes = before.shapes();
    const auto &newShapes = activeGlyph().document.shapes();
    if (oldShapes.size() == newShapes.size()
        && std::equal(oldShapes.begin(), oldShapes.end(), newShapes.begin(), [](const Shape &a, const Shape &b) {
            return a.id == b.id && a.paint == b.paint && a.path == b.path;
        })) return;
    activeGlyph().history.erase(activeGlyph().history.begin() + activeGlyph().historyPosition, activeGlyph().history.end());
    activeGlyph().history.push_back({std::move(before), activeGlyph().document});
    activeGlyph().historyPosition = activeGlyph().history.size();
    emit historyChanged();
}
void Canvas::undo() {
    if (stepPenHistory(false)) return;
    cancelGesture();
    if (!canUndo()) return;
    activeGlyph().document = activeGlyph().history[--activeGlyph().historyPosition].before;
    refreshBuilder();
    updateHoverSnap();
    if (hoverPosition_) updateCursor(*hoverPosition_);
    emit historyChanged();
    update();
}
void Canvas::redo() {
    if (stepPenHistory(true)) return;
    cancelGesture();
    if (!canRedo()) return;
    activeGlyph().document = activeGlyph().history[activeGlyph().historyPosition++].after;
    refreshBuilder();
    updateHoverSnap();
    if (hoverPosition_) updateCursor(*hoverPosition_);
    emit historyChanged();
    update();
}
bool Canvas::stepPenHistory(bool forward) {
    if (!hasPenHistory()) return false;
    auto &source = forward ? penRedo_ : penDraft_;
    auto &destination = forward ? penDraft_ : penRedo_;
    if (source.empty()) return true;
    destination.push_back(source.back());
    source.pop_back();
    gesture_ = Gesture::Idle;
    snapMarker_.reset();
    updateHoverSnap();
    if (hoverPosition_) updateCursor(*hoverPosition_);
    emit historyChanged();
    update();
    return true;
}
void Canvas::setTool(Tool tool) {
    cancelGesture();
    tool_ = tool;
    refreshBuilder();
    setCursor(tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    emit toolChanged(tool);
    update();
}
void Canvas::setRenderMode(RenderMode mode) {
    if (renderMode_ == mode) return;
    renderMode_ = mode;
    emit renderModeChanged(mode);
    update();
}
void Canvas::setReferenceVisible(bool visible) {
    if (referenceVisible_ == visible) return;
    referenceVisible_ = visible;
    emit referenceVisibilityChanged(visible);
    update();
}
void Canvas::setReferenceFont(const QFont &font) {
    referenceFont_ = font;
    refreshReference();
}
void Canvas::refreshReference() {
    reference_ = referenceGlyphPath(activeGlyph().seed.label, document().definition(), referenceFont_);
    update();
}
void Canvas::refreshBuilder() {
    builder_.reset();
    if (tool_ == Tool::ShapeBuilder) builder_.emplace(document());
    emit controlsHintChanged(controlsHint());
}
void Canvas::extendBuilder(const QPointF &point) {
    if (!builder_ || !builder_->gesture) return;
    auto &gesture = *builder_->gesture;
    for (const auto index : builder_->geometry.crossed(gesture.last, point))
        if (std::find(gesture.touched.begin(), gesture.touched.end(), index) == gesture.touched.end())
            gesture.touched.push_back(index);
    gesture.last = point;
    builder_->hovered = builder_->geometry.hit(point);
}
void Canvas::setGridSnapEnabled(bool enabled) {
    if (snapOptions_.grid == enabled) return;
    cancelGesture();
    snapOptions_.grid = enabled;
    updateHoverSnap();
    emit snapOptionsChanged();
}
void Canvas::setGeometrySnapEnabled(bool enabled) {
    if (snapOptions_.geometry == enabled) return;
    cancelGesture();
    snapOptions_.geometry = enabled;
    updateHoverSnap();
    emit snapOptionsChanged();
}
void Canvas::resetView() {
    cancelGesture();
    const auto bounds = activeGlyph().document.definition().bounds;
    const auto center = QPointF(width() / 2.0, height() / 2.0);
    viewport_.reset(center);
    viewport_.zoomAt(center, std::min((width() - 160.0) / bounds.width(), (height() - 80.0) / bounds.height()));
    viewport_.pan(QPointF(-bounds.center().x() * viewport_.zoom(), bounds.center().y() * viewport_.zoom()));
    update();
}
void Canvas::resizeEvent(QResizeEvent *event) {
    if (!viewInitialized_) {
        resetView();
        viewInitialized_ = true;
    } else {
        viewport_.pan(QPointF((event->size().width() - event->oldSize().width()) / 2.0,
                             (event->size().height() - event->oldSize().height()) / 2.0));
        cancelGesture();
    }
    QWidget::resizeEvent(event);
}
QRectF Canvas::selectionScreenBounds() const {
    if (auto id = activeGlyph().document.selection()) {
        if (const auto *shape = activeGlyph().document.find(*id))
            return viewport_.transform().mapRect(shape->path.boundingRect());
    }
    return {};
}
std::array<QPointF, 8> Canvas::handles(const QRectF &r) const {
    return {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(),
            QPointF(r.right(), r.center().y()), r.bottomRight(),
            QPointF(r.center().x(), r.bottom()), r.bottomLeft(),
            QPointF(r.left(), r.center().y())};
}
int Canvas::handleAt(const QPointF &position) const {
    if (activeGlyph().document.selectedIds().size() != 1 || tool_ == Tool::ShapeBuilder) return -1;
    const auto bounds = selectionScreenBounds();
    auto points = handles(bounds);
    for (int i = 0; i < 8; ++i) {
        if (bounds.width() == 0 && i != 1 && i != 5) continue;
        if (bounds.height() == 0 && i != 3 && i != 7) continue;
        if (QRectF(points[i] - QPointF(6, 6), QSizeF(12, 12)).contains(position)) return i;
    }
    return -1;
}
void Canvas::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#dce1e8"));
    const auto &definition = activeGlyph().document.definition();
    const auto page = viewport_.transform().mapRect(definition.bounds);
    painter.fillRect(page, QColor("#fafbfc"));
    painter.save();
    painter.setClipRect(page);
    const auto visible = QRectF(viewport_.toDocument({0, 0}),
                                viewport_.toDocument({double(width()), double(height())})).normalized();
    const double step = gridStep(viewport_.zoom());
    painter.setPen(QColor("#e2e5ea"));
    for (double x = std::floor(visible.left() / step) * step; x <= visible.right(); x += step) {
        const double sx = viewport_.toScreen({x, 0}).x();
        painter.drawLine(QPointF(sx, 0), QPointF(sx, height()));
    }
    for (double y = std::floor(visible.top() / step) * step; y <= visible.bottom(); y += step) {
        const double sy = viewport_.toScreen({0, y}).y();
        painter.drawLine(QPointF(0, sy), QPointF(width(), sy));
    }
    if (referenceVisible_ && !reference_.isEmpty()) {
        // Tracing guide only: never part of the document, hit testing, snapping, or export.
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setTransform(viewport_.transform());
        painter.fillPath(reference_, QColor(71, 85, 105, 38));
    }
    painter.restore();
    painter.setPen(QPen(QColor("#8492a6"), 1));
    painter.drawRect(page);
    for (const auto &guide : definition.guides()) {
        if (!guide.enabled) continue;
        const auto y = viewport_.toScreen({0, guide.position}).y();
        const bool baseline = guide.position == definition.baseline;
        QPen pen(baseline ? QColor("#5c6a91") : QColor("#a7b2c8"), baseline ? 1.5 : 1);
        pen.setStyle(baseline ? Qt::SolidLine : Qt::DashLine);
        painter.setPen(pen);
        painter.drawLine(QPointF(page.left(), y), QPointF(page.right(), y));
        painter.setPen(QColor("#48566f"));
        const auto labelX = std::clamp(page.left() + 8, 8.0, std::max(8.0, width() - 170.0));
        if (y >= 12 && y <= height() - 18)
            painter.drawText(QPointF(labelX, y - 5), QString::fromUtf8(guide.name) + " " + QString::number(guide.position));
    }
    const auto origin = viewport_.toScreen({0, 0});
    painter.setPen(QPen(QColor("#a7b2c8"), 1, Qt::DashLine));
    painter.drawLine(QPointF(origin.x(), page.top()), QPointF(origin.x(), page.bottom()));
    painter.setPen(QColor("#48566f"));
    if (page.top() >= 30 && page.top() < height())
        painter.drawText(QPointF(page.left(), page.top() - 25),
                         QString("Glyph workspace · %1 × %2 font units · %3 UPM")
                             .arg(definition.bounds.width()).arg(definition.bounds.height()).arg(definition.unitsPerEm));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.save();
    painter.setTransform(viewport_.transform());
    for (const auto &shape : activeGlyph().document.shapes()) {
        const bool selected = activeGlyph().document.isSelected(shape.id);
        if (renderMode_ == RenderMode::Opaque) {
            painter.setPen(shape.paint == ShapePaint::Stroke ? cosmeticPen(Qt::black, 1.5) : QPen(Qt::NoPen));
            painter.setBrush(shape.paint == ShapePaint::Stroke ? QBrush(Qt::NoBrush) : QBrush(Qt::black));
        } else {
            painter.setPen(cosmeticPen(selected ? QColor("#2563eb") : QColor("#334155"), 1.5));
            painter.setBrush(shape.paint == ShapePaint::Stroke ? QBrush(Qt::NoBrush)
                             : QBrush(selected ? QColor(96, 165, 250, 80) : QColor(100, 116, 139, 75)));
        }
        painter.drawPath(shape.path);
    }
    if (builder_) {
        painter.setPen(cosmeticPen(QColor("#2563eb"), 1));
        painter.setBrush(Qt::NoBrush);
        if (renderMode_ == RenderMode::XRay)
            for (const auto &region : builder_->geometry.regions()) painter.drawPath(region.path);
        const bool subtract = builder_->gesture && builder_->gesture->operation == BuilderOperation::Subtract;
        painter.setBrush(subtract ? QColor(239, 68, 68, 125) : QColor(37, 99, 235, 110));
        painter.setPen(cosmeticPen(subtract ? QColor("#dc2626") : QColor("#1d4ed8"), 2));
        if (builder_->gesture) {
            for (const auto index : builder_->gesture->touched)
                painter.drawPath(builder_->geometry.regions()[index].path);
        } else if (builder_->hovered) painter.drawPath(builder_->geometry.regions()[*builder_->hovered].path);
    }
    if (!preview_.isEmpty()) {
        auto pen = cosmeticPen(QColor("#2563eb"), 1.5);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(QColor(96, 165, 250, 65));
        painter.drawPath(preview_);
    }
    if (!penDraft_.empty()) {
        painter.setPen(cosmeticPen(QColor("#2563eb"), 1.5));
        painter.setBrush(Qt::NoBrush);
        auto anchors = penDraft_;
        const bool closing = gesture_ == Gesture::Idle && hoverPosition_ && !spaceHeld_
            && canClosePen(*hoverPosition_);
        if (gesture_ == Gesture::Idle && hoverPosition_ && !spaceHeld_) {
            if (closing) painter.setBrush(QColor(96, 165, 250, 65));
            else {
                const auto point = snapMarker_.value_or(*hoverPosition_);
                anchors.push_back({definition.constrainPoint(viewport_.toDocument(point)), {}});
            }
        }
        painter.drawPath(makePenPath(anchors, closing));
    }
    painter.restore();
    painter.setPen(QPen(QColor("#2563eb"), 1));
    painter.setBrush(Qt::white);
    for (const auto &anchor : penDraft_) {
        const auto point = viewport_.toScreen(anchor.position);
        if (!anchor.tangent.isNull()) {
            const auto incoming = viewport_.toScreen(anchor.position - anchor.tangent);
            const auto outgoing = viewport_.toScreen(anchor.position + anchor.tangent);
            painter.drawLine(incoming, outgoing);
            painter.drawEllipse(incoming, 3, 3);
            painter.drawEllipse(outgoing, 3, 3);
        }
        painter.drawRect(QRectF(point - QPointF(4, 4), QSizeF(8, 8)));
    }
    if (penDraft_.size() >= 3) {
        const bool closing = gesture_ == Gesture::Idle && hoverPosition_ && !spaceHeld_
            && canClosePen(*hoverPosition_);
        painter.setPen(QPen(QColor("#2563eb"), closing ? 2 : 1));
        painter.setBrush(closing ? QBrush(QColor(96, 165, 250, 80)) : QBrush(Qt::NoBrush));
        painter.drawEllipse(viewport_.toScreen(penDraft_.front().position), penCloseDistance, penCloseDistance);
    }
    if (tool_ != Tool::ShapeBuilder && activeGlyph().document.selectedIds().size() == 1) {
        painter.setPen(QPen(QColor("#2563eb"), 1));
        painter.setBrush(Qt::NoBrush);
        const auto bounds = selectionScreenBounds();
        painter.drawRect(bounds);
        painter.setBrush(Qt::white);
        const auto points = handles(bounds);
        for (int i = 0; i < 8; ++i) {
            if (bounds.width() == 0 && i != 1 && i != 5) continue;
            if (bounds.height() == 0 && i != 3 && i != 7) continue;
            painter.drawRect(QRectF(points[i] - QPointF(handleSize / 2, handleSize / 2),
                                    QSizeF(handleSize, handleSize)));
        }
    }
    if (snapMarker_) {
        painter.setPen(QPen(QColor("#d97706"), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(*snapMarker_, 5, 5);
    }
    painter.setPen(QColor("#64748b"));
    painter.drawText(QRect(12, height() - 30, width() - 24, 22), Qt::AlignRight,
                     QString::number(viewport_.zoom() * 100, 'f', 0) + "%");
}
void Canvas::mousePressEvent(QMouseEvent *event) {
    if (gesture_ != Gesture::Idle) return;
    setFocus(Qt::MouseFocusReason);
    const auto position = event->position();
    hoverPosition_ = position;
    snapMarker_.reset();
    startScreen_ = lastScreen_ = position;
    startDocument_ = viewport_.toDocument(position);
    gestureButton_ = event->button();
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && spaceHeld_)) {
        gesture_ = Gesture::Pan;
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::LeftButton) {
        if (tool_ != Tool::Select && !activeGlyph().document.definition().bounds.contains(startDocument_)) {
            update();
            return;
        }
        if (tool_ == Tool::ShapeBuilder) {
            if (!builder_) refreshBuilder();
            const auto operation = event->modifiers().testFlag(Qt::AltModifier)
                ? BuilderOperation::Subtract : BuilderOperation::Combine;
            builder_->gesture = BuilderGesture{operation, {}, startDocument_};
            if (auto hit = builder_->geometry.hit(startDocument_)) builder_->gesture->touched.push_back(*hit);
            gestureBefore_ = activeGlyph().document;
            gesture_ = Gesture::Build;
        } else if (tool_ == Tool::Pen) {
            if (canClosePen(position)) {
                auto before = activeGlyph().document;
                const auto id = activeGlyph().document.addPath(makePenPath(penDraft_, true), ShapePaint::Filled);
                if (!id) return;
                activeGlyph().document.select(id);
                penDraft_.clear();
                penRedo_.clear();
                recordEdit(std::move(before));
                updateCursor(position);
                update();
                return;
            }
            if (auto snap = snapPoint(position, activeGlyph().document, viewport_, snapOptions_)) {
                startDocument_ = activeGlyph().document.definition().constrainPoint(viewport_.toDocument(snap->point));
                snapMarker_ = snap->point;
            }
            if (penDraft_.empty()) activeGlyph().document.select({});
            penRedo_.clear();
            penDraft_.push_back({startDocument_, {}});
            gesture_ = Gesture::PenAnchor;
            emit historyChanged();
        } else if (tool_ != Tool::Select) {
            gestureBefore_ = activeGlyph().document;
            gesture_ = Gesture::Draw;
            if (auto snap = snapPoint(position, activeGlyph().document, viewport_, snapOptions_)) {
                startDocument_ = activeGlyph().document.definition().constrainPoint(viewport_.toDocument(snap->point));
                snapMarker_ = snap->point;
            }
            preview_ = {};
        } else {
            if (event->modifiers().testFlag(Qt::ShiftModifier)) {
                if (auto hit = activeGlyph().document.hitTest(startDocument_, hitTolerance / viewport_.zoom()))
                    activeGlyph().document.toggleSelection(*hit);
                update();
                return;
            }
            resizeHandle_ = handleAt(position);
            if (resizeHandle_ >= 0) {
                editingId_ = activeGlyph().document.selection();
                gesture_ = Gesture::Resize;
            } else {
                const auto hit = activeGlyph().document.hitTest(startDocument_, hitTolerance / viewport_.zoom());
                if (!hit || !activeGlyph().document.isSelected(*hit)) activeGlyph().document.select(hit);
                editingId_ = activeGlyph().document.selection();
                if (editingId_) gesture_ = Gesture::Move;
            }
            if (editingId_) {
                originalPath_ = activeGlyph().document.find(*editingId_)->path;
                gestureBefore_ = activeGlyph().document;
            }
        }
    }
    update();
}
void Canvas::updateGesture(const QPointF &position) {
    auto documentPoint = viewport_.toDocument(position);
    snapMarker_.reset();
    if (gesture_ == Gesture::Build) {
        extendBuilder(documentPoint);
    } else if (gesture_ == Gesture::Pan) {
        viewport_.pan(position - lastScreen_);
    } else if (gesture_ == Gesture::PenAnchor) {
        penDraft_.back().tangent = {};
        if (penDraft_.size() > 1 && QLineF(startScreen_, position).length() >= 3) {
            if (auto snap = snapPoint(position, activeGlyph().document, viewport_, snapOptions_)) {
                documentPoint = viewport_.toDocument(snap->point);
                snapMarker_ = snap->point;
            }
            const auto tangent = activeGlyph().document.definition().constrainTangent(penDraft_.back().position,
                                                                         documentPoint - penDraft_.back().position);
            penDraft_.back().tangent = tangent;
            if (viewport_.toScreen(penDraft_.back().position + tangent) != snapMarker_) snapMarker_.reset();
        }
    } else if (gesture_ == Gesture::Draw) {
        if (auto snap = snapPoint(position, activeGlyph().document, viewport_, snapOptions_)) {
            documentPoint = viewport_.toDocument(snap->point);
            snapMarker_ = snap->point;
        }
        const auto constrained = activeGlyph().document.definition().constrainPoint(documentPoint);
        if (constrained != documentPoint) snapMarker_.reset();
        documentPoint = constrained;
        preview_ = makeShape(tool_ == Tool::Rectangle ? ShapeKind::Rectangle : ShapeKind::Ellipse,
                             QRectF(startDocument_, documentPoint));
    } else if (gesture_ == Gesture::Move && editingId_) {
        auto delta = documentPoint - startDocument_;
        auto sourceBounds = originalPath_.boundingRect();
        if (gestureBefore_ && activeGlyph().document.selectedIds().size() > 1)
            for (const auto id : activeGlyph().document.selectedIds())
                sourceBounds = sourceBounds.united(gestureBefore_->find(id)->path.boundingRect());
        const auto rawBounds = viewport_.transform().mapRect(sourceBounds.translated(delta));
        if (position != startScreen_) {
            if (auto snap = moveSnap(rawBounds, *editingId_)) {
                const auto correction = snap->second;
                delta += QPointF(correction.x() / viewport_.zoom(), -correction.y() / viewport_.zoom());
                snapMarker_ = snap->first.point;
            }
        }
        const auto constrained = activeGlyph().document.definition().constrainTranslation(sourceBounds, delta);
        if (constrained != delta) snapMarker_.reset();
        if (gestureBefore_ && activeGlyph().document.selectedIds().size() > 1) {
            for (const auto id : activeGlyph().document.selectedIds())
                activeGlyph().document.replacePath(id, translated(gestureBefore_->find(id)->path, constrained));
        } else activeGlyph().document.replacePath(*editingId_, translated(originalPath_, constrained));
    } else if (gesture_ == Gesture::Resize && editingId_) {
        auto bounds = viewport_.transform().mapRect(originalPath_.boundingRect());
        auto handlePosition = handles(bounds)[resizeHandle_] + position - startScreen_;
        const auto axes = resizeAxes(resizeHandle_);
        if (axes == SnapAxes::Vertical) handlePosition.setX(bounds.center().x());
        if (axes == SnapAxes::Horizontal) handlePosition.setY(bounds.center().y());
        if (position != startScreen_) {
            if (auto snap = snapPoint(handlePosition, activeGlyph().document, viewport_, snapOptions_, editingId_, axes)) {
                handlePosition = snap->point;
                snapMarker_ = snap->point;
            }
        }
        if (resizeHandle_ == 0 || resizeHandle_ == 6 || resizeHandle_ == 7) bounds.setLeft(handlePosition.x());
        if (resizeHandle_ == 2 || resizeHandle_ == 3 || resizeHandle_ == 4) bounds.setRight(handlePosition.x());
        if (resizeHandle_ == 0 || resizeHandle_ == 1 || resizeHandle_ == 2) bounds.setTop(handlePosition.y());
        if (resizeHandle_ == 4 || resizeHandle_ == 5 || resizeHandle_ == 6) bounds.setBottom(handlePosition.y());
        const auto sourceBounds = originalPath_.boundingRect();
        if (sourceBounds.width() == 0) {
            bounds.setLeft(viewport_.toScreen(sourceBounds.topLeft()).x());
            bounds.setWidth(0);
        }
        if (sourceBounds.height() == 0) {
            bounds.setTop(viewport_.toScreen(sourceBounds.topLeft()).y());
            bounds.setHeight(0);
        }
        auto target = QRectF(viewport_.toDocument(bounds.topLeft()),
                              viewport_.toDocument(bounds.bottomRight())).normalized();
        const auto constrained = QRectF(activeGlyph().document.definition().constrainPoint(target.topLeft()),
                                         activeGlyph().document.definition().constrainPoint(target.bottomRight()));
        if (constrained != target) snapMarker_.reset();
        if (!activeGlyph().document.replacePath(*editingId_, resized(originalPath_, constrained))) snapMarker_.reset();
    }
    lastScreen_ = position;
    update();
}
std::optional<std::pair<SnapResult, QPointF>> Canvas::moveSnap(const QRectF &bounds, ShapeId excluded) const {
    return snapBounds(bounds, activeGlyph().document, viewport_, snapOptions_, excluded,
                      activeGlyph().document.selectedIds());
}
bool Canvas::canClosePen(const QPointF &position) const {
    return penDraft_.size() >= 3
        && activeGlyph().document.definition().bounds.contains(viewport_.toDocument(position))
        && QLineF(position, viewport_.toScreen(penDraft_.front().position)).length() <= penCloseDistance;
}
void Canvas::updateHoverSnap() {
    snapMarker_.reset();
    if (tool_ == Tool::ShapeBuilder) {
        if (builder_) builder_->hovered = hoverPosition_ && !spaceHeld_ && rect().contains(hoverPosition_->toPoint())
            ? builder_->geometry.hit(viewport_.toDocument(*hoverPosition_)) : std::nullopt;
        update();
        return;
    }
    if (hoverPosition_ && rect().contains(hoverPosition_->toPoint()) && gesture_ == Gesture::Idle && !spaceHeld_) {
        const auto position = *hoverPosition_;
        if (tool_ != Tool::Select && !activeGlyph().document.definition().bounds.contains(viewport_.toDocument(position))) {
            update();
            return;
        }
        if (tool_ == Tool::Pen && canClosePen(position)) {
            update();
            return;
        } else if (tool_ != Tool::Select) {
            if (auto snap = snapPoint(position, activeGlyph().document, viewport_, snapOptions_))
                snapMarker_ = snap->point;
        } else if (auto id = activeGlyph().document.selection()) {
            const auto bounds = selectionScreenBounds();
            const int handle = handleAt(position);
            if (handle >= 0) {
                if (auto snap = snapPoint(handles(bounds)[handle], activeGlyph().document, viewport_, snapOptions_, id, resizeAxes(handle)))
                    snapMarker_ = snap->point;
            } else if (activeGlyph().document.hitTest(viewport_.toDocument(position), hitTolerance / viewport_.zoom()) == id) {
                if (auto snap = moveSnap(bounds, *id)) snapMarker_ = snap->first.point;
            }
        }
    }
    update();
}
void Canvas::updateCursor(const QPointF &position) {
    if (spaceHeld_) { setCursor(Qt::OpenHandCursor); return; }
    if (tool_ == Tool::Pen && canClosePen(position)) { setCursor(Qt::PointingHandCursor); return; }
    if (tool_ != Tool::Select) { setCursor(Qt::CrossCursor); return; }
    const int handle = handleAt(position);
    static const Qt::CursorShape cursors[] = {
        Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor,
        Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeHorCursor};
    setCursor(handle < 0 ? Qt::ArrowCursor : cursors[handle]);
}
void Canvas::mouseMoveEvent(QMouseEvent *event) {
    hoverPosition_ = event->position();
    if (gesture_ != Gesture::Idle) updateGesture(event->position());
    else {
        updateCursor(event->position());
        updateHoverSnap();
    }
}
void Canvas::mouseReleaseEvent(QMouseEvent *event) {
    if (gesture_ == Gesture::Idle || event->button() != gestureButton_) return;
    updateGesture(event->position());
    if (gesture_ == Gesture::Build && builder_ && builder_->gesture) {
        const auto &build = *builder_->gesture;
        builder_->geometry.apply(activeGlyph().document, build.touched, build.operation);
    } else if (gesture_ == Gesture::Draw) {
        const auto screenBounds = viewport_.transform().mapRect(preview_.boundingRect());
        if (screenBounds.width() >= 3 && screenBounds.height() >= 3)
            activeGlyph().document.select(activeGlyph().document.addPath(preview_));
    }
    auto before = std::move(gestureBefore_);
    gestureBefore_.reset();
    if (before) recordEdit(std::move(*before));
    gesture_ = Gesture::Idle;
    refreshBuilder();
    editingId_.reset();
    originalPath_ = preview_ = {};
    hoverPosition_ = event->position();
    updateHoverSnap();
    updateCursor(event->position());
    update();
}
void Canvas::cancelGesture() {
    const bool hadPenHistory = hasPenHistory();
    if (builder_) { builder_->gesture.reset(); builder_->hovered.reset(); }
    if (gestureBefore_ && (gesture_ == Gesture::Move || gesture_ == Gesture::Resize))
        activeGlyph().document = *gestureBefore_;
    gestureBefore_.reset();
    gesture_ = Gesture::Idle;
    editingId_.reset();
    originalPath_ = preview_ = {};
    snapMarker_.reset();
    penDraft_.clear();
    penRedo_.clear();
    if (hadPenHistory) emit historyChanged();
    if (hoverPosition_) updateCursor(*hoverPosition_);
    update();
}
void Canvas::wheelEvent(QWheelEvent *event) {
    if (gesture_ == Gesture::Idle) {
        double steps = event->angleDelta().y() / 120.0;
        if (steps == 0) steps = event->pixelDelta().y() / 100.0;
        viewport_.zoomAt(event->position(), std::pow(1.2, steps));
        hoverPosition_ = event->position();
        updateCursor(event->position());
        updateHoverSnap();
    }
    event->accept();
}
void Canvas::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::Undo)) {
        undo();
    } else if (event->matches(QKeySequence::Redo)) {
        redo();
    } else if (event->matches(QKeySequence::SelectAll)) {
        cancelGesture();
        activeGlyph().document.selectAll();
        refreshBuilder();
        update();
    } else if (event->key() == Qt::Key_Space) {
        spaceHeld_ = true;
        snapMarker_.reset();
        update();
        if (gesture_ == Gesture::Idle) setCursor(Qt::OpenHandCursor);
    } else if (event->key() == Qt::Key_Escape) {
        if (gesture_ == Gesture::Idle && !hasPenHistory()) { activeGlyph().document.select({}); snapMarker_.reset(); }
        else cancelGesture();
        refreshBuilder();
        update();
    } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && tool_ == Tool::Pen) {
        if (gesture_ == Gesture::Idle && penDraft_.size() >= 2) {
            auto before = activeGlyph().document;
            const auto id = activeGlyph().document.addPath(makePenPath(penDraft_), ShapePaint::Stroke);
            if (!id) return;
            activeGlyph().document.select(id);
            penDraft_.clear();
            penRedo_.clear();
            recordEdit(std::move(before));
            if (hoverPosition_) updateCursor(*hoverPosition_);
            update();
        }
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        cancelGesture();
        auto before = activeGlyph().document;
        activeGlyph().document.deleteSelection();
        refreshBuilder();
        recordEdit(std::move(before));
        update();
    } else if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_V) setTool(Tool::Select);
        else if (event->key() == Qt::Key_R) setTool(Tool::Rectangle);
        else if (event->key() == Qt::Key_E) setTool(Tool::Ellipse);
        else if (event->key() == Qt::Key_P) setTool(Tool::Pen);
        else if (event->key() == Qt::Key_B) setTool(Tool::ShapeBuilder);
        else if (event->key() == Qt::Key_0) resetView();
        else QWidget::keyPressEvent(event);
    } else QWidget::keyPressEvent(event);
}
void Canvas::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spaceHeld_ = false;
        updateHoverSnap();
        if (gesture_ == Gesture::Idle) {
            if (hoverPosition_) updateCursor(*hoverPosition_);
            else setCursor(tool_ == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        }
    } else QWidget::keyReleaseEvent(event);
}
void Canvas::leaveEvent(QEvent *event) {
    hoverPosition_.reset();
    snapMarker_.reset();
    if (builder_) builder_->hovered.reset();
    update();
    QWidget::leaveEvent(event);
}
void Canvas::focusOutEvent(QFocusEvent *event) {
    spaceHeld_ = false;
    hoverPosition_.reset();
    cancelGesture();
    QWidget::focusOutEvent(event);
}
}
