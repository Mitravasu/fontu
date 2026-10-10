#pragma once
#include "document.h"

namespace fontu {
enum class BuilderOperation { Combine, Subtract };
struct ShapeRegion {
    QPainterPath path;
    std::vector<ShapeId> sources;
};
class ShapeBuilder {
public:
    explicit ShapeBuilder(const Document &document);
    const std::vector<ShapeRegion> &regions() const { return regions_; }
    std::optional<std::size_t> hit(const QPointF &point) const;
    std::vector<std::size_t> crossed(const QPointF &from, const QPointF &to) const;
    bool apply(Document &document, const std::vector<std::size_t> &touched, BuilderOperation operation) const;
private:
    std::vector<ShapeRegion> regions_;
};
}
