#pragma once
#include "project.h"
#include <QDateTime>
#include <QString>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace fontu {

template<int Minimum, int Maximum>
class FontUnits {
public:
    static constexpr int maximum = Maximum;
    static std::optional<FontUnits> rounded(double value) {
        const auto whole = std::round(value);
        if (!(whole >= Minimum && whole <= Maximum)) return {};
        return FontUnits(int(whole));
    }
    operator int() const { return value_; }
private:
    explicit FontUnits(int value) : value_(value) {}
    int value_;
};
using FontCoordinate = FontUnits<-16383, 16383>;
using FontAdvance = FontUnits<0, 16383>;

struct FontPoint {
    FontCoordinate x;
    FontCoordinate y;
    friend bool operator==(FontPoint a, FontPoint b) { return int(a.x) == int(b.x) && int(a.y) == int(b.y); }
    friend bool operator!=(FontPoint a, FontPoint b) { return !(a == b); }
};
struct LineSegment { FontPoint end; };
struct CubicSegment { FontPoint c1; FontPoint c2; FontPoint end; };
using ContourSegment = std::variant<LineSegment, CubicSegment>;
FontPoint segmentEnd(const ContourSegment &segment);

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
    FontAdvance advance;
    std::vector<Contour> contours;
};

using GlyphIndex = std::uint16_t;

struct GlyphOrder {
    FontGlyph notdef;
    std::vector<FontGlyph> glyphs;
    std::size_t size() const { return glyphs.size() + 1; }
    const FontGlyph &at(GlyphIndex index) const { return index == 0 ? notdef : glyphs.at(index - 1); }
};

struct LineMetrics {
    FontCoordinate ascender;
    FontCoordinate descender;
    FontCoordinate xHeight;
    FontCoordinate capHeight;
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
