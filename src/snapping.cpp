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
namespace {
template<std::size_t N>
std::optional<std::pair<SnapResult, QPointF>> snapPoints(
        const std::array<QPointF, N> &points, const Document &document,
        const Viewport &viewport, SnapOptions options, std::optional<ShapeId> excluded,
        SnapAxes axes, const std::vector<ShapeId> &excludedIds) {
    using Candidate = std::optional<std::pair<SnapResult, QPointF>>;
    Candidate best;
    std::array<Candidate, N> intersections;
    auto consider = [&](Candidate &candidate, const QPointF &point, QPointF target) {
        if (axes == SnapAxes::Horizontal && std::abs(target.y() - point.y()) > 1e-7) return;
        if (axes == SnapAxes::Vertical && std::abs(target.x() - point.x()) > 1e-7) return;
        const double distance = QLineF(point, target).length();
        if (distance <= snapTolerance && (!candidate || distance < candidate->first.distance))
            candidate = std::make_pair(SnapResult{target, distance}, target - point);
    };
    const auto &definition = document.definition();
    const double step = gridStep(viewport.zoom());
    bool hasIntersection = false;
    if (options.geometry) {
        for (std::size_t i = 0; i < N; ++i) {
            const auto &point = points[i];
            const auto docPoint = viewport.toDocument(point);
            const double gridX = std::round(docPoint.x() / step) * step;
            const double gridY = std::round(docPoint.y() / step) * step;
            for (const auto &guide : definition.guides()) {
                if (!guide.enabled) continue;
                consider(intersections[i], point, viewport.toScreen({0, guide.position}));
                const QPointF crossing(gridX, guide.position);
                if (options.grid && definition.bounds.contains(crossing))
                    consider(intersections[i], point, viewport.toScreen(crossing));
            }
            if (definition.guideEnabled.sideBearing) {
                for (const double sideX : definition.sideBearingGuides()) {
                    for (const auto &guide : definition.guides())
                        if (guide.enabled) consider(intersections[i], point, viewport.toScreen({sideX, guide.position}));
                    const QPointF crossing(sideX, gridY);
                    if (options.grid && definition.bounds.contains(crossing))
                        consider(intersections[i], point, viewport.toScreen(crossing));
                }
            }
            const QPointF originGridCrossing(0, gridY);
            if (options.grid && definition.bounds.contains(originGridCrossing))
                consider(intersections[i], point, viewport.toScreen(originGridCrossing));
            hasIntersection |= intersections[i].has_value();
        }
    }
    for (std::size_t i = 0; i < N; ++i) {
        const auto &point = points[i];
        const auto considerTarget = [&](QPointF target) { consider(best, point, target); };
        if (options.grid) {
            const auto doc = viewport.toDocument(point);
            QPointF target(std::round(doc.x() / step) * step, std::round(doc.y() / step) * step);
            if (axes == SnapAxes::Horizontal) target.setY(doc.y());
            if (axes == SnapAxes::Vertical) target.setX(doc.x());
            if (definition.bounds.contains(target)) considerTarget(viewport.toScreen(target));
        }
        if (options.geometry) {
            const auto docPoint = viewport.toDocument(point);
            if (intersections[i]) {
                considerTarget(intersections[i]->first.point);
            } else if (!hasIntersection) {
                for (const auto &guide : definition.guides())
                    if (guide.enabled)
                        considerTarget(viewport.toScreen({definition.constrainPoint(docPoint).x(), guide.position}));
                considerTarget(viewport.toScreen({0, definition.constrainPoint(docPoint).y()}));
                if (definition.guideEnabled.sideBearing)
                    for (const double sideX : definition.sideBearingGuides())
                        considerTarget(viewport.toScreen({sideX, definition.constrainPoint(docPoint).y()}));
            }
            for (const auto &shape : document.shapes()) {
                if (excluded == shape.id || std::find(excludedIds.begin(), excludedIds.end(), shape.id) != excludedIds.end()) continue;
                const auto path = viewport.transform().map(shape.path);
                if (!path.boundingRect().adjusted(-snapTolerance, -snapTolerance,
                                                 snapTolerance, snapTolerance).contains(point)) continue;
                for (const auto &boundsPoint : boundsSnapPoints(path.boundingRect())) considerTarget(boundsPoint);
                // Qt flattens curves after mapping to logical pixels. Stored paths stay cubic.
                for (const auto &polygon : path.toSubpathPolygons()) {
                    for (int i = 1; i < polygon.size(); ++i) {
                        const auto a = polygon[i - 1];
                        const auto delta = polygon[i] - a;
                        const double lengthSquared = QPointF::dotProduct(delta, delta);
                        if (lengthSquared == 0) { considerTarget(a); continue; }
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
                        considerTarget(a + delta * std::clamp(t, 0.0, 1.0));
                    }
                }
            }
        }
    }
    return best;
}
}
std::optional<SnapResult> snapPoint(const QPointF &point, const Document &document,
                                  const Viewport &viewport, SnapOptions options,
                                  std::optional<ShapeId> excluded, SnapAxes axes, const std::vector<ShapeId> &excludedIds) {
    const auto snap = snapPoints(std::array<QPointF, 1>{point}, document, viewport, options, excluded, axes, excludedIds);
    return snap ? std::optional<SnapResult>(snap->first) : std::nullopt;
}
std::optional<std::pair<SnapResult, QPointF>> snapBounds(
        const QRectF &bounds, const Document &document, const Viewport &viewport,
        SnapOptions options, std::optional<ShapeId> excluded, const std::vector<ShapeId> &excludedIds) {
    return snapPoints(boundsSnapPoints(bounds), document, viewport, options, excluded, SnapAxes::Both, excludedIds);
}
}
