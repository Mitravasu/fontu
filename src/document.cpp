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
    return !path.isEmpty() && validBounds(path.boundingRect());
}
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
    if (!validBounds(source) || !validBounds(target)) return {};
    QTransform transform;
    transform.translate(target.x(), target.y());
    transform.scale(target.width() / source.width(), target.height() / source.height());
    transform.translate(-source.x(), -source.y());
    return transform.map(path);
}
const Shape *Document::find(ShapeId id) const {
    const auto it = std::find_if(shapes_.begin(), shapes_.end(),
                                 [id](const Shape &shape) { return shape.id == id; });
    return it == shapes_.end() ? nullptr : &*it;
}
std::optional<ShapeId> Document::addPath(const QPainterPath &path) {
    if (!validPath(path)) return {};
    const auto id = nextId_++;
    shapes_.push_back({id, path});
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
        if (it->path.contains(point) || stroker.createStroke(it->path).contains(point))
            return it->id;
    }
    return {};
}
}
