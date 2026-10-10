#pragma once
#include "document.h"
#include "viewport.h"
#include <array>

namespace fontu {
struct SnapOptions { bool grid = false; bool geometry = false; };
enum class SnapAxes { Both, Horizontal, Vertical };
struct SnapResult { QPointF point; double distance; };
constexpr double snapTolerance = 8;
double gridStep(double zoom);
std::array<QPointF, 9> boundsSnapPoints(const QRectF &bounds);
std::optional<SnapResult> snapPoint(const QPointF &screenPoint, const Document &document,
                                  const Viewport &viewport, SnapOptions options,
                                  std::optional<ShapeId> excluded = {},
                                  SnapAxes axes = SnapAxes::Both, const std::vector<ShapeId> &excludedIds = {});
std::optional<std::pair<SnapResult, QPointF>> snapBounds(
        const QRectF &screenBounds, const Document &document, const Viewport &viewport,
        SnapOptions options, std::optional<ShapeId> excluded = {}, const std::vector<ShapeId> &excludedIds = {});
}
