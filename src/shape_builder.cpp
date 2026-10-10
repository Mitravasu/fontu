#include "shape_builder.h"
#include <QLineF>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fontu {
namespace {
constexpr double geometryScale = 16;
QPainterPath canonicalVertices(QPainterPath path) {
    constexpr double precision = 1e9;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto element = path.elementAt(i);
        path.setElementPositionAt(i, std::round(element.x * precision) / precision,
                                  std::round(element.y * precision) / precision);
    }
    const auto retraces = [](const QPointF &a, const QPointF &b, const QPointF &c) {
        const auto u = b - a, v = c - b;
        if (QPointF::dotProduct(u, v) >= 0) return false;
        const auto longest = std::max({QLineF(a, b).length(), QLineF(b, c).length(), QLineF(a, c).length()});
        return std::abs(u.x() * v.y() - u.y() * v.x()) <= (2 / precision) * longest;
    };
    QPainterPath boundary;
    boundary.setFillRule(path.fillRule());
    for (auto polygon : path.toSubpathPolygons()) {
        if (!polygon.empty() && polygon.front() == polygon.back()) polygon.removeLast();
        std::vector<QPointF> vertices;
        for (const auto point : polygon) {
            if (!vertices.empty() && vertices.back() == point) continue;
            while (vertices.size() >= 2 && retraces(vertices[vertices.size() - 2], vertices.back(), point))
                vertices.pop_back();
            if (vertices.empty() || vertices.back() != point) vertices.push_back(point);
        }
        std::size_t begin = 0, end = vertices.size();
        while (end - begin >= 3) {
            if (vertices[begin] == vertices[end - 1]) { --end; continue; }
            if (retraces(vertices[end - 1], vertices[begin], vertices[begin + 1])) { ++begin; continue; }
            if (retraces(vertices[end - 2], vertices[end - 1], vertices[begin])) { --end; continue; }
            break;
        }
        if (end - begin < 3) continue;
        boundary.moveTo(vertices[begin]);
        for (auto i = begin + 1; i < end; ++i) boundary.lineTo(vertices[i]);
        boundary.closeSubpath();
    }
    return boundary;
}
bool closedFill(const Shape &shape) {
    if (shape.paint != ShapePaint::Filled) return false;
    QPointF start, end;
    bool contour = false;
    for (int i = 0; i < shape.path.elementCount(); ++i) {
        const auto element = shape.path.elementAt(i);
        const QPointF point(element.x, element.y);
        if (element.isMoveTo()) {
            if (contour && QLineF(start, end).length() > 1e-7) return false;
            start = point;
            contour = true;
        }
        end = point;
    }
    return contour && QLineF(start, end).length() <= 1e-7;
}
void splitWalk(const QPolygonF &polygon, std::vector<QPolygonF> &loops) {
    for (int i = 0; i + 1 < polygon.size(); ++i)
        for (int j = i + 1; j + 1 < polygon.size(); ++j) {
            if (polygon[i] != polygon[j]) continue;
            QPolygonF loop, remainder;
            for (int k = i; k <= j; ++k) loop.push_back(polygon[k]);
            for (int k = 0; k <= i; ++k) remainder.push_back(polygon[k]);
            for (int k = j + 1; k < polygon.size(); ++k) remainder.push_back(polygon[k]);
            splitWalk(loop, loops);
            splitWalk(remainder, loops);
            return;
        }
    loops.push_back(polygon);
}
QPointF interiorPoint(const QPainterPath &contour, const QPolygonF &polygon) {
    std::vector<double> heights;
    for (const auto point : polygon) heights.push_back(point.y());
    std::sort(heights.begin(), heights.end());
    for (std::size_t row = 1; row < heights.size(); ++row) {
        if (heights[row] == heights[row - 1]) continue;
        const auto y = (heights[row] + heights[row - 1]) / 2;
        std::vector<double> crossings;
        for (int edge = 1; edge < polygon.size(); ++edge) {
            const auto a = polygon[edge - 1], b = polygon[edge];
            if ((a.y() < y && b.y() > y) || (b.y() < y && a.y() > y))
                crossings.push_back(a.x() + (b.x() - a.x()) * (y - a.y()) / (b.y() - a.y()));
        }
        std::sort(crossings.begin(), crossings.end());
        for (std::size_t column = 1; column < crossings.size(); ++column) {
            const QPointF point((crossings[column] + crossings[column - 1]) / 2, y);
            if (contour.contains(point)) return point;
        }
    }
    return polygon.front();
}
std::vector<QPainterPath> components(const QPainterPath &path) {
    std::vector<QPolygonF> loops;
    for (const auto &polygon : path.simplified().toSubpathPolygons()) splitWalk(polygon, loops);
    std::vector<QPainterPath> contours;
    std::vector<double> areas;
    std::vector<QPointF> witnesses;
    for (const auto &polygon : loops) {
        double area = 0;
        for (int i = 1; i < polygon.size(); ++i)
            area += polygon[i - 1].x() * polygon[i].y() - polygon[i].x() * polygon[i - 1].y();
        if (std::abs(area) < 1e-9) continue;
        QPainterPath contour;
        contour.addPolygon(polygon);
        contour.closeSubpath();
        witnesses.push_back(interiorPoint(contour, polygon));
        contours.push_back(contour);
        areas.push_back(std::abs(area));
    }
    std::vector<int> parent(contours.size(), -1), depth(contours.size(), 0);
    for (std::size_t i = 0; i < contours.size(); ++i) {
        double enclosingArea = std::numeric_limits<double>::infinity();
        for (std::size_t j = 0; j < contours.size(); ++j) {
            if (areas[j] <= areas[i] || !contours[j].contains(witnesses[i])) continue;
            if (areas[j] < enclosingArea) { parent[i] = int(j); enclosingArea = areas[j]; }
        }
    }
    for (std::size_t i = 0; i < contours.size(); ++i)
        for (auto ancestor = parent[i]; ancestor >= 0; ancestor = parent[ancestor]) ++depth[i];
    std::vector<QPainterPath> result;
    for (std::size_t i = 0; i < contours.size(); ++i) {
        if (depth[i] % 2) continue;
        auto component = contours[i];
        component.setFillRule(Qt::OddEvenFill);
        for (std::size_t j = 0; j < contours.size(); ++j)
            if (parent[j] == int(i) && depth[j] % 2) component.addPath(contours[j]);
        result.push_back(component);
    }
    return result;
}
void appendRegions(std::vector<ShapeRegion> &output, const QPainterPath &path, const std::vector<ShapeId> &sources) {
    for (auto &piece : components(path)) output.push_back({std::move(piece), sources});
}
bool crossesInterior(const QPainterPath &path, const QPointF &from, const QPointF &to) {
    if (from == to) return path.contains(from);
    const auto delta = to - from;
    const auto lengthSquared = QPointF::dotProduct(delta, delta);
    std::vector<double> cuts{0, 1};
    const auto polygons = path.toSubpathPolygons();
    for (const auto &polygon : polygons)
        for (int i = 1; i < polygon.size(); ++i) {
            QPointF intersection;
            if (QLineF(from, to).intersects(QLineF(polygon[i - 1], polygon[i]), &intersection) == QLineF::BoundedIntersection)
                cuts.push_back(std::clamp(QPointF::dotProduct(intersection - from, delta) / lengthSquared, 0.0, 1.0));
        }
    std::sort(cuts.begin(), cuts.end());
    for (std::size_t i = 1; i < cuts.size(); ++i) {
        if (cuts[i] - cuts[i - 1] <= 1e-12) continue;
        const auto point = from + delta * ((cuts[i] + cuts[i - 1]) / 2);
        if (!path.contains(point)) continue;
        bool boundary = false;
        for (const auto &polygon : polygons)
            for (int edge = 1; edge < polygon.size(); ++edge) {
                const auto a = polygon[edge - 1], d = polygon[edge] - a;
                const auto squared = QPointF::dotProduct(d, d);
                if (squared == 0) continue;
                const auto t = std::clamp(QPointF::dotProduct(point - a, d) / squared, 0.0, 1.0);
                if (QLineF(point, a + d * t).length() < 1e-8) boundary = true;
            }
        if (!boundary) return true;
    }
    return false;
}
}
ShapeBuilder::ShapeBuilder(const Document &document) {
    const auto scale = QTransform::fromScale(geometryScale, geometryScale);
    for (const auto &shape : document.shapes()) {
        if (!document.isSelected(shape.id) || !closedFill(shape)) continue;
        const auto path = scale.map(shape.path).simplified();
        auto uncovered = path;
        std::vector<ShapeRegion> partition;
        for (const auto &region : regions_) {
            uncovered = uncovered.subtracted(region.path);
            appendRegions(partition, region.path.subtracted(path), region.sources);
            auto sources = region.sources;
            sources.push_back(shape.id);
            appendRegions(partition, region.path.intersected(path), sources);
        }
        appendRegions(partition, uncovered, {shape.id});
        regions_ = std::move(partition);
    }
    const auto inverse = QTransform::fromScale(1 / geometryScale, 1 / geometryScale);
    for (auto &region : regions_) region.path = canonicalVertices(inverse.map(region.path));
}
std::optional<std::size_t> ShapeBuilder::hit(const QPointF &point) const {
    for (std::size_t i = 0; i < regions_.size(); ++i)
        if (regions_[i].path.contains(point)) return i;
    return {};
}
std::vector<std::size_t> ShapeBuilder::crossed(const QPointF &from, const QPointF &to) const {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < regions_.size(); ++i)
        if (crossesInterior(regions_[i].path, from, to)) result.push_back(i);
    return result;
}
bool ShapeBuilder::apply(Document &document, const std::vector<std::size_t> &touched, BuilderOperation operation) const {
    if (touched.empty()) return false;
    std::vector<ShapeId> consumed;
    QPainterPath merged;
    for (const auto index : touched) {
        if (index >= regions_.size()) return false;
        const auto &region = regions_[index];
        for (const auto id : region.sources)
            if (std::find(consumed.begin(), consumed.end(), id) == consumed.end()) consumed.push_back(id);
        if (operation == BuilderOperation::Combine) merged = merged.united(region.path);
    }
    std::vector<QPainterPath> paths;
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        if (std::find(touched.begin(), touched.end(), i) != touched.end()) continue;
        const auto &region = regions_[i];
        const bool entirelyConsumed = std::all_of(region.sources.begin(), region.sources.end(), [&](ShapeId id) {
            return std::find(consumed.begin(), consumed.end(), id) != consumed.end();
        });
        if (entirelyConsumed) paths.push_back(region.path);
    }
    if (!merged.isEmpty()) paths.push_back(canonicalVertices(merged.simplified()));
    return document.replaceShapes(consumed, paths);
}
}
