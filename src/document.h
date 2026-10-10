#pragma once
#include <QPainterPath>
#include <cstdint>
#include <optional>
#include <vector>
#include <array>

namespace fontu {
using ShapeId = std::uint64_t;
enum class ShapeKind { Rectangle, Ellipse };
enum class ShapePaint { Filled, Stroke };
struct Shape { ShapeId id; QPainterPath path; ShapePaint paint = ShapePaint::Filled; };
struct PenAnchor { QPointF position; QPointF tangent; };
QPainterPath makePenPath(const std::vector<PenAnchor> &anchors, bool closed = false);
QPainterPath makeShape(ShapeKind kind, const QRectF &bounds);
QPainterPath translated(const QPainterPath &path, const QPointF &delta);
QPainterPath resized(const QPainterPath &path, const QRectF &bounds);

struct TypographyGuide { const char *name; double position; };
struct GlyphDocumentDefinition {
    QRectF bounds{-500, -250, 1000, 1000};
    double unitsPerEm = 1000;
    double baseline = 0;
    double xHeight = 500;
    double capHeight = 700;
    double ascender = 750;
    double descender = -250;
    std::array<TypographyGuide, 5> guides() const;
    QPointF constrainPoint(const QPointF &point) const;
    QPointF constrainTranslation(const QRectF &source, const QPointF &delta) const;
    QPointF constrainTangent(const QPointF &anchor, const QPointF &tangent) const;
};

class Document {
public:
    const GlyphDocumentDefinition &definition() const { return definition_; }
    const std::vector<Shape> &shapes() const { return shapes_; }
    const Shape *find(ShapeId id) const;
    std::optional<ShapeId> addPath(const QPainterPath &path, ShapePaint paint = ShapePaint::Filled);
    bool replacePath(ShapeId id, const QPainterPath &path);
    void select(std::optional<ShapeId> id);
    std::optional<ShapeId> selection() const { return selected_; }
    bool deleteSelection();
    std::optional<ShapeId> hitTest(const QPointF &point, double tolerance) const;
private:
    GlyphDocumentDefinition definition_;
    std::vector<Shape> shapes_;
    std::optional<ShapeId> selected_;
    ShapeId nextId_ = 1;
};
}
