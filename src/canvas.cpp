#include "canvas.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <cmath>

namespace fontu {
namespace {
constexpr double handleSize = 8;
constexpr double hitTolerance = 5;
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
Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    setAutoFillBackground(false);
}
void Canvas::setTool(Tool tool) {
    cancelGesture();
    tool_ = tool;
    setCursor(tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    emit toolChanged(tool);
    update();
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
    viewport_.reset(QPointF(width() / 2.0, height() / 2.0));
    update();
}
void Canvas::resizeEvent(QResizeEvent *event) {
    if (!viewInitialized_) {
        viewport_.reset(QPointF(width() / 2.0, height() / 2.0));
        viewInitialized_ = true;
    } else {
        viewport_.pan(QPointF((event->size().width() - event->oldSize().width()) / 2.0,
                             (event->size().height() - event->oldSize().height()) / 2.0));
        cancelGesture();
    }
    QWidget::resizeEvent(event);
}
QRectF Canvas::selectionScreenBounds() const {
    if (auto id = document_.selection()) {
        if (const auto *shape = document_.find(*id))
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
    if (!document_.selection()) return -1;
    auto points = handles(selectionScreenBounds());
    for (int i = 0; i < 8; ++i) {
        if (QRectF(points[i] - QPointF(6, 6), QSizeF(12, 12)).contains(position)) return i;
    }
    return -1;
}
void Canvas::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#f8f9fb"));
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
    const auto origin = viewport_.toScreen({0, 0});
    painter.setPen(QColor("#b4bdca"));
    painter.drawLine(QPointF(origin.x(), 0), QPointF(origin.x(), height()));
    painter.drawLine(QPointF(0, origin.y()), QPointF(width(), origin.y()));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.save();
    painter.setTransform(viewport_.transform());
    for (const auto &shape : document_.shapes()) {
        const bool selected = document_.selection() == shape.id;
        painter.setPen(cosmeticPen(selected ? QColor("#2563eb") : QColor("#334155"), 1.5));
        painter.setBrush(selected ? QColor(96, 165, 250, 80) : QColor(100, 116, 139, 75));
        painter.drawPath(shape.path);
    }
    if (!preview_.isEmpty()) {
        auto pen = cosmeticPen(QColor("#2563eb"), 1.5);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(QColor(96, 165, 250, 65));
        painter.drawPath(preview_);
    }
    painter.restore();
    if (document_.selection()) {
        painter.setPen(QPen(QColor("#2563eb"), 1));
        painter.setBrush(Qt::NoBrush);
        const auto bounds = selectionScreenBounds();
        painter.drawRect(bounds);
        painter.setBrush(Qt::white);
        for (const auto &point : handles(bounds))
            painter.drawRect(QRectF(point - QPointF(handleSize / 2, handleSize / 2),
                                    QSizeF(handleSize, handleSize)));
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
        if (tool_ != Tool::Select) {
            gesture_ = Gesture::Draw;
            if (auto snap = snapPoint(position, document_, viewport_, snapOptions_)) {
                startDocument_ = viewport_.toDocument(snap->point);
                snapMarker_ = snap->point;
            }
            preview_ = {};
        } else {
            resizeHandle_ = handleAt(position);
            if (resizeHandle_ >= 0) {
                editingId_ = document_.selection();
                gesture_ = Gesture::Resize;
            } else {
                document_.select(document_.hitTest(startDocument_, hitTolerance / viewport_.zoom()));
                editingId_ = document_.selection();
                if (editingId_) gesture_ = Gesture::Move;
            }
            if (editingId_) originalPath_ = document_.find(*editingId_)->path;
        }
    }
    update();
}
void Canvas::updateGesture(const QPointF &position) {
    auto documentPoint = viewport_.toDocument(position);
    snapMarker_.reset();
    if (gesture_ == Gesture::Pan) {
        viewport_.pan(position - lastScreen_);
    } else if (gesture_ == Gesture::Draw) {
        if (auto snap = snapPoint(position, document_, viewport_, snapOptions_)) {
            documentPoint = viewport_.toDocument(snap->point);
            snapMarker_ = snap->point;
        }
        preview_ = makeShape(tool_ == Tool::Rectangle ? ShapeKind::Rectangle : ShapeKind::Ellipse,
                             QRectF(startDocument_, documentPoint));
    } else if (gesture_ == Gesture::Move && editingId_) {
        auto delta = documentPoint - startDocument_;
        const auto rawBounds = viewport_.transform().mapRect(translated(originalPath_, delta).boundingRect());
        if (position != startScreen_) {
            if (auto snap = moveSnap(rawBounds, *editingId_)) {
                const auto correction = snap->second;
                delta += QPointF(correction.x() / viewport_.zoom(), -correction.y() / viewport_.zoom());
                snapMarker_ = snap->first.point;
            }
        }
        document_.replacePath(*editingId_, translated(originalPath_, delta));
    } else if (gesture_ == Gesture::Resize && editingId_) {
        auto bounds = viewport_.transform().mapRect(originalPath_.boundingRect());
        auto handlePosition = handles(bounds)[resizeHandle_] + position - startScreen_;
        const auto axes = resizeAxes(resizeHandle_);
        if (axes == SnapAxes::Vertical) handlePosition.setX(bounds.center().x());
        if (axes == SnapAxes::Horizontal) handlePosition.setY(bounds.center().y());
        if (position != startScreen_) {
            if (auto snap = snapPoint(handlePosition, document_, viewport_, snapOptions_, editingId_, axes)) {
                handlePosition = snap->point;
                snapMarker_ = snap->point;
            }
        }
        if (resizeHandle_ == 0 || resizeHandle_ == 6 || resizeHandle_ == 7) bounds.setLeft(handlePosition.x());
        if (resizeHandle_ == 2 || resizeHandle_ == 3 || resizeHandle_ == 4) bounds.setRight(handlePosition.x());
        if (resizeHandle_ == 0 || resizeHandle_ == 1 || resizeHandle_ == 2) bounds.setTop(handlePosition.y());
        if (resizeHandle_ == 4 || resizeHandle_ == 5 || resizeHandle_ == 6) bounds.setBottom(handlePosition.y());
        auto target = QRectF(viewport_.toDocument(bounds.topLeft()),
                              viewport_.toDocument(bounds.bottomRight())).normalized();
        document_.replacePath(*editingId_, resized(originalPath_, target));
    }
    lastScreen_ = position;
    update();
}
std::optional<std::pair<SnapResult, QPointF>> Canvas::moveSnap(const QRectF &bounds, ShapeId excluded) const {
    std::optional<std::pair<SnapResult, QPointF>> best;
    for (const auto &point : boundsSnapPoints(bounds)) {
        if (auto snap = snapPoint(point, document_, viewport_, snapOptions_, excluded)) {
            if (!best || snap->distance < best->first.distance)
                best = std::make_pair(*snap, snap->point - point);
        }
    }
    return best;
}
void Canvas::updateHoverSnap() {
    snapMarker_.reset();
    if (hoverPosition_ && rect().contains(hoverPosition_->toPoint()) && gesture_ == Gesture::Idle && !spaceHeld_) {
        const auto position = *hoverPosition_;
        if (tool_ != Tool::Select) {
            if (auto snap = snapPoint(position, document_, viewport_, snapOptions_))
                snapMarker_ = snap->point;
        } else if (auto id = document_.selection()) {
            const auto bounds = selectionScreenBounds();
            const int handle = handleAt(position);
            if (handle >= 0) {
                if (auto snap = snapPoint(handles(bounds)[handle], document_, viewport_, snapOptions_, id, resizeAxes(handle)))
                    snapMarker_ = snap->point;
            } else if (document_.hitTest(viewport_.toDocument(position), hitTolerance / viewport_.zoom()) == id) {
                if (auto snap = moveSnap(bounds, *id)) snapMarker_ = snap->first.point;
            }
        }
    }
    update();
}
void Canvas::updateCursor(const QPointF &position) {
    if (spaceHeld_) { setCursor(Qt::OpenHandCursor); return; }
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
    if (gesture_ == Gesture::Draw) {
        const auto screenBounds = QRectF(startScreen_, event->position()).normalized();
        if (screenBounds.width() >= 3 && screenBounds.height() >= 3)
            document_.select(document_.addPath(preview_));
    }
    gesture_ = Gesture::Idle;
    editingId_.reset();
    originalPath_ = preview_ = {};
    hoverPosition_ = event->position();
    updateHoverSnap();
    updateCursor(event->position());
    update();
}
void Canvas::cancelGesture() {
    if (editingId_ && (gesture_ == Gesture::Move || gesture_ == Gesture::Resize))
        document_.replacePath(*editingId_, originalPath_);
    gesture_ = Gesture::Idle;
    editingId_.reset();
    originalPath_ = preview_ = {};
    snapMarker_.reset();
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
    if (event->key() == Qt::Key_Space) {
        spaceHeld_ = true;
        snapMarker_.reset();
        update();
        if (gesture_ == Gesture::Idle) setCursor(Qt::OpenHandCursor);
    } else if (event->key() == Qt::Key_Escape) {
        if (gesture_ == Gesture::Idle) { document_.select({}); snapMarker_.reset(); }
        else cancelGesture();
        update();
    } else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        cancelGesture();
        document_.deleteSelection();
        update();
    } else if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_V) setTool(Tool::Select);
        else if (event->key() == Qt::Key_R) setTool(Tool::Rectangle);
        else if (event->key() == Qt::Key_E) setTool(Tool::Ellipse);
        else if (event->key() == Qt::Key_0) resetView();
        else QWidget::keyPressEvent(event);
    } else QWidget::keyPressEvent(event);
}
void Canvas::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spaceHeld_ = false;
        updateHoverSnap();
        if (gesture_ == Gesture::Idle) setCursor(tool_ == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    } else QWidget::keyReleaseEvent(event);
}
void Canvas::leaveEvent(QEvent *event) {
    hoverPosition_.reset();
    snapMarker_.reset();
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
