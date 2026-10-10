#include "viewport.h"
#include <algorithm>
#include <cmath>
namespace fontu {
QPointF Viewport::toScreen(const QPointF &point) const {
    return offset_ + QPointF(point.x() * zoom_, -point.y() * zoom_);
}
QPointF Viewport::toDocument(const QPointF &point) const {
    auto delta = point - offset_;
    return {delta.x() / zoom_, -delta.y() / zoom_};
}
QTransform Viewport::transform() const {
    return QTransform(zoom_, 0, 0, -zoom_, offset_.x(), offset_.y());
}
void Viewport::zoomAt(const QPointF &anchor, double factor) {
    if (!std::isfinite(factor) || factor <= 0) return;
    const auto documentAnchor = toDocument(anchor);
    zoom_ = std::clamp(zoom_ * factor, 0.05, 32.0);
    offset_ = anchor - QPointF(documentAnchor.x() * zoom_, -documentAnchor.y() * zoom_);
}
}
