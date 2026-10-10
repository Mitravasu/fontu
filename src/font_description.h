#pragma once
#include "project.h"
#include <QDateTime>
#include <QString>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace fontu {

// Font units, y-up, origin at the drawing-area left edge on the baseline.
// describeFont keeps |x| and |y| at most 16383 so every delta between two
// points fits the int16 operands of a Type 2 charstring and of glyf later.
struct FontPoint {
    std::int16_t x;
    std::int16_t y;
    friend bool operator==(FontPoint a, FontPoint b) { return a.x == b.x && a.y == b.y; }
    friend bool operator!=(FontPoint a, FontPoint b) { return !(a == b); }
};
struct LineSegment { FontPoint end; };
struct CubicSegment { FontPoint c1; FontPoint c2; FontPoint end; };
using ContourSegment = std::variant<LineSegment, CubicSegment>;
FontPoint segmentEnd(const ContourSegment &segment);

// A closed cycle of segments. The contour starts where its last segment ends,
// so a duplicated closing point and an open contour have no representation.
// Filled regions wind counterclockwise and holes clockwise, so the nonzero rule
// over all contours of a glyph paints the union of its shapes.
class Contour {
public:
    static std::optional<Contour> cycle(std::vector<ContourSegment> segments);
    FontPoint start() const { return segmentEnd(segments_.back()); }
    const std::vector<ContourSegment> &segments() const { return segments_; }
    Contour reversed() const;
private:
    explicit Contour(std::vector<ContourSegment> segments) : segments_(std::move(segments)) {}
    std::vector<ContourSegment> segments_;
};

struct FontGlyph {
    QString name;
    std::uint16_t advance;
    std::vector<Contour> contours;
};

using GlyphIndex = std::uint16_t;

// Glyph 0 is .notdef by construction, and project glyph i is glyph index i + 1.
struct GlyphOrder {
    FontGlyph notdef;
    std::vector<FontGlyph> glyphs;
    std::size_t size() const { return glyphs.size() + 1; }
    const FontGlyph &at(GlyphIndex index) const { return index == 0 ? notdef : glyphs.at(index - 1); }
};

struct LineMetrics {
    std::int16_t ascender;
    std::int16_t descender;
    std::int16_t lineGap;
    std::int16_t xHeight;
    std::int16_t capHeight;
};

struct FontNames {
    QString family;
    QString style;
    QString postScriptName;
};

struct FontDescription {
    std::uint16_t unitsPerEm;
    FontNames names;
    LineMetrics lineMetrics;
    GlyphOrder glyphs;
    std::map<char32_t, GlyphIndex> characterMap;
    QDateTime timestamp;
};

struct FontSettings {
    QString familyName;
    QDateTime timestamp;
};

enum class ExportWarningKind { StrokeSkipped, LabelNotMapped, DuplicateLabel, ShapeFlattened, ContourDropped };
struct ExportWarning {
    ExportWarningKind kind;
    GlyphId glyph;
    QString label;
    int count;
};
QString warningText(const ExportWarning &warning);

struct DescribedFont {
    FontDescription font;
    std::vector<ExportWarning> warnings;
};
using FontDescriptionResult = std::variant<DescribedFont, QString>;

FontDescriptionResult describeFont(const ProjectSnapshot &project, const FontSettings &settings);

}
