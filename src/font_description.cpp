#include "font_description.h"
#include <QLineF>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

namespace fontu {
namespace {
constexpr double maximumCoordinate = 16383;
constexpr std::size_t maximumGlyphs = 65535;

struct OutOfRange {};

struct Placement {
    double dx;
    double dy;
};
Placement placement(const GlyphDocumentDefinition &d) { return {-d.bounds.left(), -d.baseline}; }

std::int16_t fontUnit(double value) {
    const auto rounded = std::round(value);
    if (!std::isfinite(rounded) || std::abs(rounded) > maximumCoordinate) throw OutOfRange{};
    return std::int16_t(rounded);
}
FontPoint toFontPoint(QPointF point, Placement p) { return {fontUnit(point.x() + p.dx), fontUnit(point.y() + p.dy)}; }

QPolygonF flattened(const Contour &contour) {
    QPainterPath path(QPointF(contour.start().x, contour.start().y));
    for (const auto &segment : contour.segments()) {
        if (const auto *line = std::get_if<LineSegment>(&segment)) path.lineTo(line->end.x, line->end.y);
        else {
            const auto &cubic = std::get<CubicSegment>(segment);
            path.cubicTo(cubic.c1.x, cubic.c1.y, cubic.c2.x, cubic.c2.y, cubic.end.x, cubic.end.y);
        }
    }
    return path.toFillPolygon();
}
double signedArea(const QPolygonF &polygon) {
    double area = 0;
    for (int i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area / 2;
}

std::vector<std::vector<ContourSegment>> subpathSegments(const QPainterPath &path, Placement p) {
    std::vector<std::vector<ContourSegment>> result;
    FontPoint start{}, current{};
    const auto finish = [&] {
        if (result.empty()) return;
        auto &segments = result.back();
        if (current != start) segments.push_back(LineSegment{start});
    };
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        const auto point = toFontPoint(e, p);
        if (e.isMoveTo()) {
            finish();
            result.emplace_back();
            start = current = point;
        } else if (e.isLineTo()) {
            if (point == current) continue;
            result.back().push_back(LineSegment{point});
            current = point;
        } else if (e.isCurveTo()) {
            const auto c2 = toFontPoint(path.elementAt(++i), p);
            const auto end = toFontPoint(path.elementAt(++i), p);
            if (point == current && c2 == current && end == current) continue;
            result.back().push_back(CubicSegment{point, c2, end});
            current = end;
        }
    }
    finish();
    return result;
}

bool properlyCrosses(QLineF a, QLineF b) {
    QPointF at;
    if (a.intersects(b, &at) != QLineF::BoundedIntersection) return false;
    const auto near = [&at](QPointF q) { return QLineF(at, q).length() < 1e-6; };
    return !near(a.p1()) && !near(a.p2()) && !near(b.p1()) && !near(b.p2());
}
bool edgesCross(const QPainterPath &path) {
    std::vector<QLineF> edges;
    for (const auto &polygon : path.toSubpathPolygons())
        for (int i = 1; i < polygon.size(); ++i)
            if (polygon[i - 1] != polygon[i]) edges.emplace_back(polygon[i - 1], polygon[i]);
    std::vector<QRectF> boxes;
    for (const auto &edge : edges) boxes.push_back(QRectF(edge.p1(), edge.p2()).normalized());
    for (std::size_t i = 0; i < edges.size(); ++i)
        for (std::size_t j = i + 1; j < edges.size(); ++j) {
            const auto &a = boxes[i], &b = boxes[j];
            if (a.right() < b.left() || b.right() < a.left() || a.bottom() < b.top() || b.bottom() < a.top()) continue;
            if (properlyCrosses(edges[i], edges[j])) return true;
        }
    return false;
}

double distanceToEdges(QPointF point, const QPolygonF &polygon) {
    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i], d = polygon[(i + 1) % polygon.size()] - a;
        const auto squared = QPointF::dotProduct(d, d);
        const auto t = squared == 0 ? 0 : std::clamp(QPointF::dotProduct(point - a, d) / squared, 0.0, 1.0);
        best = std::min(best, QLineF(point, a + d * t).length());
    }
    return best;
}
bool inside(const QPolygonF &inner, const QPolygonF &outer) {
    for (const auto &point : inner)
        if (distanceToEdges(point, outer) > 0.5) return outer.containsPoint(point, Qt::OddEvenFill);
    return false;
}

// Each shape is oriented on its own so its winding number is 1 where the canvas
// fills it and 0 elsewhere. Summed under nonzero, overlapping shapes then union
// without flattening their curves.
std::vector<Contour> shapeContours(const Shape &shape, Placement p, const ProjectGlyph &glyph,
                                   std::vector<ExportWarning> &warnings) {
    auto source = shape.path;
    auto subpaths = subpathSegments(source, p);
    if ((source.fillRule() == Qt::WindingFill && subpaths.size() > 1) || edgesCross(source)) {
        source = source.simplified();
        subpaths = subpathSegments(source, p);
        warnings.push_back({ExportWarningKind::ShapeFlattened, glyph.seed.id, glyph.seed.label, 1});
    }
    std::vector<Contour> contours;
    std::vector<QPolygonF> polygons;
    int dropped = 0;
    for (auto &segments : subpaths) {
        auto contour = Contour::cycle(std::move(segments));
        const auto polygon = contour ? flattened(*contour) : QPolygonF();
        if (!contour || signedArea(polygon) == 0) {
            ++dropped;
            continue;
        }
        contours.push_back(*contour);
        polygons.push_back(polygon);
    }
    if (dropped) warnings.push_back({ExportWarningKind::ContourDropped, glyph.seed.id, glyph.seed.label, dropped});
    for (std::size_t i = 0; i < contours.size(); ++i) {
        int depth = 0;
        for (std::size_t j = 0; j < contours.size(); ++j)
            if (i != j && inside(polygons[i], polygons[j])) ++depth;
        const bool counterclockwise = signedArea(polygons[i]) > 0;
        if (counterclockwise != (depth % 2 == 0)) contours[i] = contours[i].reversed();
    }
    return contours;
}

std::optional<char32_t> labelCodePoint(const QString &label) {
    char32_t scalar;
    if (label.size() == 1 && !label[0].isSurrogate()) scalar = label[0].unicode();
    else if (label.size() == 2 && label[0].isHighSurrogate() && label[1].isLowSurrogate())
        scalar = QChar::surrogateToUcs4(label[0], label[1]);
    else return {};
    if (scalar == 0xFFFF) return {};
    return scalar;
}

QString glyphName(std::optional<char32_t> codePoint, GlyphId id) {
    if (!codePoint) return "glyph" + QString::number(id.value);
    const auto hex = [&codePoint](int width) { return QString("%1").arg(uint(*codePoint), width, 16, QChar('0')).toUpper(); };
    return *codePoint <= 0xFFFF ? "uni" + hex(4) : "u" + hex(5);
}

QString postScriptName(const QString &family) {
    const QString suffix = "-Regular";
    QString name;
    for (const auto c : family)
        if (c.unicode() >= 33 && c.unicode() <= 126 && !QString("[](){}<>/%").contains(c)) name += c;
    if (name.isEmpty()) name = "Untitled";
    return name.left(63 - suffix.size()) + suffix;
}

std::vector<ContourSegment> box(int left, int bottom, int right, int top, bool counterclockwise) {
    const auto p = [](int x, int y) { return LineSegment{{std::int16_t(x), std::int16_t(y)}}; };
    if (counterclockwise) return {p(right, bottom), p(right, top), p(left, top), p(left, bottom)};
    return {p(left, top), p(right, top), p(right, bottom), p(left, bottom)};
}
FontGlyph notdefGlyph(std::uint16_t advance, std::int16_t capHeight, std::uint16_t unitsPerEm) {
    const int wall = std::max(1, unitsPerEm / 20);
    const int left = advance / 10, right = advance - advance / 10;
    const int top = std::max<int>(capHeight, 2 * wall + 1);
    FontGlyph glyph{".notdef", advance, {}};
    if (right - left <= 2 * wall) return glyph;
    glyph.contours.push_back(*Contour::cycle(box(left, 0, right, top, true)));
    glyph.contours.push_back(*Contour::cycle(box(left + wall, wall, right - wall, top - wall, false)));
    return glyph;
}
}

FontPoint segmentEnd(const ContourSegment &segment) {
    return std::visit([](const auto &s) { return s.end; }, segment);
}

std::optional<Contour> Contour::cycle(std::vector<ContourSegment> segments) {
    if (segments.empty()) return {};
    return Contour(std::move(segments));
}

Contour Contour::reversed() const {
    std::vector<ContourSegment> result;
    for (std::size_t k = segments_.size(); k-- > 0;) {
        const auto previous = segmentEnd(segments_[k == 0 ? segments_.size() - 1 : k - 1]);
        if (const auto *cubic = std::get_if<CubicSegment>(&segments_[k])) result.push_back(CubicSegment{cubic->c2, cubic->c1, previous});
        else result.push_back(LineSegment{previous});
    }
    return Contour(std::move(result));
}

QString warningText(const ExportWarning &warning) {
    const auto label = QString("“%1”").arg(warning.label);
    switch (warning.kind) {
    case ExportWarningKind::StrokeSkipped:
        return QString("%1: skipped %2 stroke shape(s). Strokes have no width in the font, so only filled shapes export.").arg(label).arg(warning.count);
    case ExportWarningKind::LabelNotMapped:
        return QString("%1: exported without a character mapping, because the label is not a single character.").arg(label);
    case ExportWarningKind::DuplicateLabel:
        return QString("%1: exported without a character mapping, because an earlier glyph already uses this character.").arg(label);
    case ExportWarningKind::ShapeFlattened:
        return QString("%1: a self-intersecting shape was converted to straight segments.").arg(label);
    case ExportWarningKind::ContourDropped:
        return QString("%1: dropped %2 contour(s) that had no area after rounding to whole font units.").arg(label).arg(warning.count);
    }
    return {};
}

FontDescriptionResult describeFont(const ProjectSnapshot &project, const FontSettings &settings) {
    const auto &d = project.definition;
    if (!(d.unitsPerEm >= 16 && d.unitsPerEm <= 16384 && std::floor(d.unitsPerEm) == d.unitsPerEm))
        return QString("Units per em must be a whole number from 16 to 16384.");
    if (project.glyphs.size() + 1 > maximumGlyphs)
        return QString("OpenType fonts hold at most 65535 glyphs, including .notdef.");
    const auto advance = std::round(d.bounds.width());
    if (!(advance >= 0 && advance <= 65535)) return QString("The drawing area is too wide for an OpenType advance width.");
    const auto family = settings.familyName.trimmed();
    if (family.isEmpty()) return QString("Enter a font family name.");

    DescribedFont result;
    auto &font = result.font;
    font.unitsPerEm = std::uint16_t(d.unitsPerEm);
    font.names = {family, "Regular", postScriptName(family)};
    font.timestamp = settings.timestamp;
    const auto p = placement(d);
    try {
        font.lineMetrics = {fontUnit(d.ascender + p.dy), fontUnit(d.descender + p.dy), 0,
                            fontUnit(d.xHeight + p.dy), fontUnit(d.capHeight + p.dy)};
    } catch (const OutOfRange &) {
        return QString("The typography guides are outside the range OpenType can store.");
    }
    font.glyphs.notdef = notdefGlyph(std::uint16_t(advance), font.lineMetrics.capHeight, font.unitsPerEm);
    for (const auto &glyph : project.glyphs) {
        const auto index = GlyphIndex(font.glyphs.size());
        auto codePoint = labelCodePoint(glyph.seed.label);
        if (!codePoint) result.warnings.push_back({ExportWarningKind::LabelNotMapped, glyph.seed.id, glyph.seed.label, 1});
        else if (!font.characterMap.emplace(*codePoint, index).second) {
            result.warnings.push_back({ExportWarningKind::DuplicateLabel, glyph.seed.id, glyph.seed.label, 1});
            codePoint.reset();
        }
        FontGlyph out{glyphName(codePoint, glyph.seed.id), std::uint16_t(advance), {}};
        int strokes = 0;
        try {
            for (const auto &shape : glyph.shapes) {
                if (shape.paint == ShapePaint::Stroke) {
                    ++strokes;
                    continue;
                }
                auto contours = shapeContours(shape, p, glyph, result.warnings);
                out.contours.insert(out.contours.end(), contours.begin(), contours.end());
            }
        } catch (const OutOfRange &) {
            return QString("Glyph “%1” has points outside the range OpenType can store.").arg(glyph.seed.label);
        }
        if (strokes) result.warnings.push_back({ExportWarningKind::StrokeSkipped, glyph.seed.id, glyph.seed.label, strokes});
        font.glyphs.glyphs.push_back(std::move(out));
    }
    return result;
}

}
