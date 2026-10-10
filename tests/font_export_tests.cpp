#include "font_description.h"
#include <QTimeZone>
#include <QtTest>
using namespace fontu;

namespace {
const QDateTime fixedTime(QDate(2026, 1, 2), QTime(3, 4, 5), QTimeZone::UTC);
FontSettings settings(QString family = "Test Face") { return {std::move(family), fixedTime}; }
ProjectSnapshot project(std::vector<ProjectGlyph> glyphs) {
    const auto active = glyphs.front().seed.id;
    return {GlyphDocumentDefinition{}, std::move(glyphs), active};
}
Shape filled(ShapeId id, QPainterPath path) { return {id, std::move(path), ShapePaint::Filled}; }
QPainterPath ring() {
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    path.addRect(-100, 0, 200, 200);
    path.addRect(-50, 50, 100, 100);
    return path;
}
DescribedFont described(const ProjectSnapshot &p) { return std::get<DescribedFont>(describeFont(p, settings())); }
std::vector<FontPoint> ends(const Contour &contour) {
    std::vector<FontPoint> result;
    for (const auto &segment : contour.segments()) result.push_back(segmentEnd(segment));
    return result;
}
}
Q_DECLARE_METATYPE(FontPoint)
namespace QTest {
template<> char *toString(const FontPoint &p) { return qstrdup(QString("(%1, %2)").arg(p.x).arg(p.y).toUtf8()); }
}

class FontExportTests : public QObject {
    Q_OBJECT
private slots:
    void rectangleShiftsToDrawingAreaOriginAsACycle() {
        const auto font = described(project({{{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 20, 30}))}}})).font;
        QCOMPARE(font.glyphs.size(), std::size_t(2));
        QCOMPARE(font.glyphs.at(1).name, QString("uni0041"));
        QCOMPARE(font.glyphs.at(1).advance, std::uint16_t(1000));
        const auto &c = font.glyphs.at(1).contours.at(0);
        QCOMPARE(c.start(), (FontPoint{500, 0}));
        QCOMPARE(ends(c), (std::vector<FontPoint>{{520, 0}, {520, 30}, {500, 30}, {500, 0}}));
        QCOMPARE(font.characterMap.at(U'A'), GlyphIndex(1));
    }
    void openFilledPathIsClosedWithALine() {
        QPainterPath open;
        open.moveTo(0, 0);
        open.lineTo(100, 0);
        open.lineTo(100, 100);
        const auto font = described(project({{{{1}, "v"}, {filled(1, open)}}})).font;
        const auto &c = font.glyphs.at(1).contours.at(0);
        QCOMPARE(ends(c), (std::vector<FontPoint>{{600, 0}, {600, 100}, {500, 0}}));
    }
    void notdefOccupiesGlyphZeroAsAHollowBox() {
        const auto font = described(project({{{{1}, "A"}, {}}})).font;
        const auto &notdef = font.glyphs.at(0);
        QCOMPARE(notdef.name, QString(".notdef"));
        QCOMPARE(notdef.advance, std::uint16_t(1000));
        QCOMPARE(ends(notdef.contours.at(0)), (std::vector<FontPoint>{{900, 0}, {900, 700}, {100, 700}, {100, 0}}));
        QCOMPARE(ends(notdef.contours.at(1)), (std::vector<FontPoint>{{150, 650}, {850, 650}, {850, 50}, {150, 50}}));
    }
    void nonzeroBaselineBecomesFontZeroAndHalvesRoundAwayFromZero() {
        auto p = project({{{{1}, "x"}, {filled(1, makeShape(ShapeKind::Rectangle, {0.5, 10, 20, 30.5}))}}});
        p.definition.baseline = 10;
        const auto font = described(p).font;
        QCOMPARE(font.glyphs.at(1).contours.at(0).start(), (FontPoint{501, 0}));
        QCOMPARE(font.lineMetrics.ascender, std::int16_t(740));
        QCOMPARE(font.lineMetrics.descender, std::int16_t(-260));
    }
    void evenOddHoleIsReversedToClockwise() {
        const auto result = described(project({{{{1}, "O"}, {filled(1, ring())}}}));
        const auto &contours = result.font.glyphs.at(1).contours;
        QCOMPARE(ends(contours.at(0)), (std::vector<FontPoint>{{600, 0}, {600, 200}, {400, 200}, {400, 0}}));
        QCOMPARE(contours.at(1).start(), (FontPoint{450, 50}));
        QCOMPARE(ends(contours.at(1)), (std::vector<FontPoint>{{450, 150}, {550, 150}, {550, 50}, {450, 50}}));
        QCOMPARE(result.warnings.size(), std::size_t(0));
    }
    void ellipseKeepsItsFourCubics() {
        const auto font = described(project({{{{1}, "o"}, {filled(1, makeShape(ShapeKind::Ellipse, {-100, 0, 200, 200}))}}})).font;
        const auto &c = font.glyphs.at(1).contours.at(0);
        QCOMPARE(c.segments().size(), std::size_t(4));
        QCOMPARE(std::get<CubicSegment>(c.segments()[3]).end, (FontPoint{600, 100}));
        QCOMPARE(c.start(), (FontPoint{600, 100}));
    }
    void selfCrossingShapeIsFlattenedWithAWarning() {
        const auto bowtie = makePenPath({{{0, 0}, {}}, {{100, 100}, {}}, {{100, 0}, {}}, {{0, 100}, {}}}, true);
        const auto result = described(project({{{{1}, "x"}, {filled(1, bowtie)}}}));
        QCOMPARE(result.font.glyphs.at(1).contours.size(), std::size_t(2));
        QCOMPARE(result.warnings.size(), std::size_t(1));
        QCOMPARE(result.warnings[0].kind, ExportWarningKind::ShapeFlattened);
        QCOMPARE(warningText(result.warnings[0]), QString("“x”: a self-intersecting shape was converted to straight segments."));
    }
    void labelPolicyMapsSingleScalarsOnly() {
        const auto result = described(project({
            {{{7}, "alternate zero"}, {}}, {{{8}, "0"}, {}}, {{{9}, "0"}, {}}, {{{10}, QString::fromUcs4(U"\U0001F600", 1)}, {}}}));
        QCOMPARE(result.font.glyphs.at(1).name, QString("glyph7"));
        QCOMPARE(result.font.glyphs.at(2).name, QString("uni0030"));
        QCOMPARE(result.font.glyphs.at(3).name, QString("glyph9"));
        QCOMPARE(result.font.glyphs.at(4).name, QString("u1F600"));
        QCOMPARE(result.font.characterMap, (std::map<char32_t, GlyphIndex>{{U'0', 2}, {U'\U0001F600', 4}}));
        QCOMPARE(result.warnings.size(), std::size_t(2));
        QCOMPARE(result.warnings[0].kind, ExportWarningKind::LabelNotMapped);
        QCOMPARE(result.warnings[1].kind, ExportWarningKind::DuplicateLabel);
        QCOMPARE(result.warnings[1].glyph, GlyphId{9});
    }
    void strokesAreSkippedWithAWarning() {
        const auto stroke = makePenPath({{{-100, 0}, {}}, {{100, 100}, {}}});
        const auto result = described(project({{{{1}, "l"}, {{1, stroke, ShapePaint::Stroke}, {2, stroke, ShapePaint::Stroke}}}}));
        QCOMPARE(result.font.glyphs.at(1).contours.size(), std::size_t(0));
        QCOMPARE(result.warnings.size(), std::size_t(1));
        QCOMPARE(warningText(result.warnings[0]),
                 QString("“l”: skipped 2 stroke shape(s). Strokes have no width in the font, so only filled shapes export."));
    }
    void rejectsFractionalUnitsPerEm() {
        auto p = project({{{{1}, "A"}, {}}});
        p.definition.unitsPerEm = 1000.5;
        QCOMPARE(std::get<QString>(describeFont(p, settings())), QString("Units per em must be a whole number from 16 to 16384."));
    }
    void rejectsPointsOutsideTheOpenTypeRange() {
        auto p = project({{{{1}, "W"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 20000, 30}))}}});
        QCOMPARE(std::get<QString>(describeFont(p, settings())), QString("Glyph “W” has points outside the range OpenType can store."));
    }
    void namesComeFromTheFamilySetting() {
        const auto font = std::get<DescribedFont>(describeFont(project({{{{1}, "A"}, {}}}), settings("  My (Font) "))).font;
        QCOMPARE(font.names.family, QString("My (Font)"));
        QCOMPARE(font.names.style, QString("Regular"));
        QCOMPARE(font.names.postScriptName, QString("MyFont-Regular"));
    }
};
QTEST_MAIN(FontExportTests)
#include "font_export_tests.moc"
