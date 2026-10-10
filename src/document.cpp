#include "document.h"
#include <QPainterPathStroker>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace fontu {
namespace {
bool validBounds(const QRectF &bounds) {
    return std::isfinite(bounds.x()) && std::isfinite(bounds.y())
        && std::isfinite(bounds.width()) && std::isfinite(bounds.height())
        && bounds.width() >= 0.001 && bounds.height() >= 0.001;
}
bool validPath(const QPainterPath &path) {
    for (int i = 0; i < path.elementCount(); ++i) {
        auto e = path.elementAt(i);
        if (!std::isfinite(e.x) || !std::isfinite(e.y)) return false;
    }
    const auto bounds = path.boundingRect();
    return !path.isEmpty() && std::isfinite(bounds.width()) && std::isfinite(bounds.height())
        && (bounds.width() >= 0.001 || bounds.height() >= 0.001);
}
}
QPainterPath makePenPath(const std::vector<PenAnchor> &anchors, bool closed) {
    QPainterPath path;
    if (anchors.empty()) return path;
    path.moveTo(anchors.front().position);
    const auto segmentEnd = anchors.size() + (closed ? 1 : 0);
    for (std::size_t i = 1; i < segmentEnd; ++i) {
        const auto &a = anchors[i - 1];
        const auto &b = anchors[i % anchors.size()];
        if (a.tangent.isNull() && b.tangent.isNull()) path.lineTo(b.position);
        else path.cubicTo(a.position + a.tangent, b.position - b.tangent, b.position);
    }
    if (closed) path.closeSubpath();
    return path;
}
QPainterPath makeShape(ShapeKind kind, const QRectF &bounds) {
    QPainterPath path;
    auto rect = bounds.normalized();
    if (!validBounds(rect)) return path;
    if (kind == ShapeKind::Rectangle) path.addRect(rect);
    else path.addEllipse(rect);
    return path;
}
QPainterPath translated(const QPainterPath &path, const QPointF &delta) {
    return QTransform::fromTranslate(delta.x(), delta.y()).map(path);
}
QPainterPath resized(const QPainterPath &path, const QRectF &bounds) {
    const auto source = path.boundingRect();
    const auto target = bounds.normalized();
    if (!validPath(path) || !std::isfinite(target.x()) || !std::isfinite(target.y())
        || !std::isfinite(target.width()) || !std::isfinite(target.height())) return {};
    if ((source.width() >= 0.001 && target.width() < 0.001)
        || (source.height() >= 0.001 && target.height() < 0.001)) return {};
    QTransform transform;
    transform.translate(target.x(), target.y());
    transform.scale(source.width() == 0 ? 1 : target.width() / source.width(),
                    source.height() == 0 ? 1 : target.height() / source.height());
    transform.translate(-source.x(), -source.y());
    return transform.map(path);
}
const Shape *Document::find(ShapeId id) const {
    const auto it = std::find_if(shapes_.begin(), shapes_.end(),
                                 [id](const Shape &shape) { return shape.id == id; });
    return it == shapes_.end() ? nullptr : &*it;
}
std::optional<ShapeId> Document::addPath(const QPainterPath &path, ShapePaint paint) {
    if (!validPath(path)) return {};
    const auto id = nextId_++;
    shapes_.push_back({id, path, paint});
    return id;
}
bool Document::replacePath(ShapeId id, const QPainterPath &path) {
    if (!validPath(path)) return false;
    for (auto &shape : shapes_) {
        if (shape.id == id) { shape.path = path; return true; }
    }
    return false;
}
void Document::select(std::optional<ShapeId> id) {
    selected_ = id && find(*id) ? id : std::nullopt;
}
bool Document::deleteSelection() {
    if (!selected_) return false;
    const auto id = *selected_;
    shapes_.erase(std::remove_if(shapes_.begin(), shapes_.end(),
                   [id](const Shape &shape) { return shape.id == id; }), shapes_.end());
    selected_.reset();
    return true;
}
std::optional<ShapeId> Document::hitTest(const QPointF &point, double tolerance) const {
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(0.001, tolerance * 2));
    for (auto it = shapes_.rbegin(); it != shapes_.rend(); ++it) {
        if ((it->paint == ShapePaint::Filled && it->path.contains(point))
            || stroker.createStroke(it->path).contains(point))
            return it->id;
    }
    return {};
}
}
