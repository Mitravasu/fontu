#include "document.h"
#include <QPainterPathStroker>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <limits>

namespace fontu {
namespace {
bool validBounds(const QRectF &bounds) {
    return std::isfinite(bounds.x()) && std::isfinite(bounds.y())
        && std::isfinite(bounds.width()) && std::isfinite(bounds.height())
        && bounds.width() >= 0.001 && bounds.height() >= 0.001;
}
bool withinBounds(const QPainterPath &path, const QRectF &bounds) {
    const auto tolerated = bounds.adjusted(-1e-7, -1e-7, 1e-7, 1e-7);
    const auto ink = path.boundingRect();
    return tolerated.contains(ink.topLeft()) && tolerated.contains(ink.bottomRight());
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
std::array<TypographyGuide, 5> GlyphDocumentDefinition::guides() const {
    return {{{"Ascender", ascender, guideEnabled.ascender}, {"Cap height", capHeight, guideEnabled.capHeight},
             {"x-height", xHeight, guideEnabled.xHeight}, {"Baseline", baseline, guideEnabled.baseline},
             {"Descender", descender, guideEnabled.descender}}};
}
std::array<double, 2> GlyphDocumentDefinition::sideBearingGuides() const {
    return {bounds.left() + sideBearing, bounds.right() - sideBearing};
}
QPointF GlyphDocumentDefinition::constrainPoint(const QPointF &point) const {
    return {std::clamp(point.x(), bounds.left(), bounds.right()),
            std::clamp(point.y(), bounds.top(), bounds.bottom())};
}
QPointF GlyphDocumentDefinition::constrainTranslation(const QRectF &source, const QPointF &delta) const {
    return {std::clamp(delta.x(), bounds.left() - source.left(), bounds.right() - source.right()),
            std::clamp(delta.y(), bounds.top() - source.top(), bounds.bottom() - source.bottom())};
}
QPointF GlyphDocumentDefinition::constrainTangent(const QPointF &anchor, const QPointF &tangent) const {
    double scale = 1;
    if (tangent.x() != 0)
        scale = std::min(scale, std::min(anchor.x() - bounds.left(), bounds.right() - anchor.x()) / std::abs(tangent.x()));
    if (tangent.y() != 0)
        scale = std::min(scale, std::min(anchor.y() - bounds.top(), bounds.bottom() - anchor.y()) / std::abs(tangent.y()));
    return tangent * std::clamp(scale, 0.0, 1.0);
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
Document::Document(GlyphDocumentDefinition definition, std::vector<Shape> shapes)
    : definition_(definition), shapes_(std::move(shapes)) {
    for (const auto &shape : shapes_) nextId_ = std::max(nextId_, shape.id + 1);
}
const Shape *Document::find(ShapeId id) const {
    const auto it = std::find_if(shapes_.begin(), shapes_.end(),
                                 [id](const Shape &shape) { return shape.id == id; });
    return it == shapes_.end() ? nullptr : &*it;
}
std::optional<ShapeId> Document::addPath(const QPainterPath &path, ShapePaint paint) {
    if (!validPath(path) || !withinBounds(path, definition_.bounds)) return {};
    while (nextId_ == std::numeric_limits<ShapeId>::max() || find(nextId_)) {
        nextId_ = nextId_ == std::numeric_limits<ShapeId>::max() ? 1 : nextId_ + 1;
    }
    const auto id = nextId_++;
    shapes_.push_back({id, path, paint});
    return id;
}
bool Document::replacePath(ShapeId id, const QPainterPath &path) {
    if (!validPath(path) || !withinBounds(path, definition_.bounds)) return false;
    for (auto &shape : shapes_) {
        if (shape.id == id) { shape.path = path; return true; }
    }
    return false;
}
void Document::select(std::optional<ShapeId> id) {
    selected_.clear();
    if (id && find(*id)) selected_.push_back(*id);
}
bool Document::isSelected(ShapeId id) const {
    return std::find(selected_.begin(), selected_.end(), id) != selected_.end();
}
void Document::toggleSelection(ShapeId id) {
    if (!find(id)) return;
    const auto it = std::find(selected_.begin(), selected_.end(), id);
    if (it == selected_.end()) selected_.push_back(id);
    else selected_.erase(it);
}
void Document::selectAll() {
    selected_.clear();
    for (const auto &shape : shapes_) selected_.push_back(shape.id);
}
bool Document::deleteSelection() {
    if (selected_.empty()) return false;
    shapes_.erase(std::remove_if(shapes_.begin(), shapes_.end(),
                   [this](const Shape &shape) { return isSelected(shape.id); }), shapes_.end());
    selected_.clear();
    return true;
}
bool Document::replaceShapes(const std::vector<ShapeId> &sources, const std::vector<QPainterPath> &paths) {
    if (sources.empty()) return false;
    for (const auto id : sources) if (!find(id)) return false;
    for (const auto &path : paths)
        if (!validPath(path) || !withinBounds(path, definition_.bounds)) return false;
    auto candidate = *this;
    const auto consumed = [&](ShapeId id) {
        return std::find(sources.begin(), sources.end(), id) != sources.end();
    };
    auto insertion = std::find_if(candidate.shapes_.begin(), candidate.shapes_.end(),
                                [&](const Shape &shape) { return consumed(shape.id); });
    const auto index = std::size_t(insertion - candidate.shapes_.begin());
    candidate.shapes_.erase(std::remove_if(candidate.shapes_.begin(), candidate.shapes_.end(),
                              [&](const Shape &shape) { return consumed(shape.id); }), candidate.shapes_.end());
    candidate.selected_.erase(std::remove_if(candidate.selected_.begin(), candidate.selected_.end(), consumed),
                              candidate.selected_.end());
    std::vector<Shape> added;
    for (const auto &path : paths) {
        const auto id = candidate.addPath(path);
        if (!id) return false;
        added.push_back(candidate.shapes_.back());
        candidate.selected_.push_back(*id);
        candidate.shapes_.pop_back();
    }
    candidate.shapes_.insert(candidate.shapes_.begin() + index, added.begin(), added.end());
    *this = std::move(candidate);
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
