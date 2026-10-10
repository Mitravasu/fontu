#pragma once
#include <QPainterPath>
#include <cstdint>
#include <optional>
#include <vector>

namespace fontu {
using ShapeId = std::uint64_t;
enum class ShapeKind { Rectangle, Ellipse };
struct Shape { ShapeId id; QPainterPath path; };
QPainterPath makeShape(ShapeKind kind, const QRectF &bounds);
QPainterPath translated(const QPainterPath &path, const QPointF &delta);
QPainterPath resized(const QPainterPath &path, const QRectF &bounds);

class Document {
public:
    const std::vector<Shape> &shapes() const { return shapes_; }
    const Shape *find(ShapeId id) const;
    std::optional<ShapeId> addPath(const QPainterPath &path);
    bool replacePath(ShapeId id, const QPainterPath &path);
    void select(std::optional<ShapeId> id);
    std::optional<ShapeId> selection() const { return selected_; }
    bool deleteSelection();
    std::optional<ShapeId> hitTest(const QPointF &point, double tolerance) const;
private:
    std::vector<Shape> shapes_;
    std::optional<ShapeId> selected_;
    ShapeId nextId_ = 1;
};
}
