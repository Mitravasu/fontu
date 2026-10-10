#include "snapping.h"
#include <QLineF>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace fontu {
double gridStep(double zoom) {
    double step = 25;
    while (step * zoom < 16) step *= 2;
    while (step * zoom > 64) step /= 2;
    return step;
}
std::array<QPointF, 9> boundsSnapPoints(const QRectF &r) {
    return {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(),
            QPointF(r.right(), r.center().y()), r.bottomRight(),
            QPointF(r.center().x(), r.bottom()), r.bottomLeft(),
            QPointF(r.left(), r.center().y()), r.center()};
}
std::optional<SnapResult> snapPoint(const QPointF &point, const Document &document,
                                  const Viewport &viewport, SnapOptions options,
                                  std::optional<ShapeId> excluded, SnapAxes axes) {
    std::optional<SnapResult> best;
    auto consider = [&](QPointF target) {
        if (axes == SnapAxes::Horizontal && std::abs(target.y() - point.y()) > 1e-7) return;
        if (axes == SnapAxes::Vertical && std::abs(target.x() - point.x()) > 1e-7) return;
        const double distance = QLineF(point, target).length();
        if (distance <= snapTolerance && (!best || distance < best->distance))
            best = SnapResult{target, distance};
    };
    if (options.grid) {
        const auto doc = viewport.toDocument(point);
        const double step = gridStep(viewport.zoom());
        QPointF target(std::round(doc.x() / step) * step, std::round(doc.y() / step) * step);
        if (axes == SnapAxes::Horizontal) target.setY(doc.y());
        if (axes == SnapAxes::Vertical) target.setX(doc.x());
        if (document.definition().bounds.contains(target)) consider(viewport.toScreen(target));
    }
    if (options.geometry) {
        const auto docPoint = viewport.toDocument(point);
        for (const auto &guide : document.definition().guides())
            if (guide.enabled)
                consider(viewport.toScreen({document.definition().constrainPoint(docPoint).x(), guide.position}));
        consider(viewport.toScreen({0, document.definition().constrainPoint(docPoint).y()}));
        for (const auto &shape : document.shapes()) {
            if (excluded == shape.id) continue;
            const auto path = viewport.transform().map(shape.path);
            if (!path.boundingRect().adjusted(-snapTolerance, -snapTolerance,
                                             snapTolerance, snapTolerance).contains(point)) continue;
            for (const auto &boundsPoint : boundsSnapPoints(path.boundingRect())) consider(boundsPoint);
            // Qt flattens curves after mapping to logical pixels. Stored paths stay cubic.
            for (const auto &polygon : path.toSubpathPolygons()) {
                for (int i = 1; i < polygon.size(); ++i) {
                    const auto a = polygon[i - 1];
                    const auto delta = polygon[i] - a;
                    const double lengthSquared = QPointF::dotProduct(delta, delta);
                    if (lengthSquared == 0) { consider(a); continue; }
                    double t;
                    if (axes == SnapAxes::Horizontal) {
                        if (std::abs(delta.y()) < 1e-12) {
                            if (std::abs(a.y() - point.y()) > 1e-7) continue;
                            t = QPointF::dotProduct(point - a, delta) / lengthSquared;
                        } else t = (point.y() - a.y()) / delta.y();
                    } else if (axes == SnapAxes::Vertical) {
                        if (std::abs(delta.x()) < 1e-12) {
                            if (std::abs(a.x() - point.x()) > 1e-7) continue;
                            t = QPointF::dotProduct(point - a, delta) / lengthSquared;
                        } else t = (point.x() - a.x()) / delta.x();
                    } else t = QPointF::dotProduct(point - a, delta) / lengthSquared;
                    if (axes != SnapAxes::Both && (t < 0 || t > 1)) continue;
                    consider(a + delta * std::clamp(t, 0.0, 1.0));
                }
            }
        }
    }
    return best;
}
}
