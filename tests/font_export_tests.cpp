#include "font_export.h"
#include <QRawFont>
#include <QTemporaryDir>
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
quint32 bigEndian32(const QByteArray &bytes, int offset) {
    quint32 value = 0;
    for (int i = 0; i < 4; ++i) value = value << 8 | quint8(bytes[offset + i]);
    return value;
}
quint32 sum32(QByteArray bytes) {
    while (bytes.size() % 4) bytes.append('\0');
    quint32 sum = 0;
    for (int i = 0; i < bytes.size(); i += 4) sum += bigEndian32(bytes, i);
    return sum;
}
QByteArray table(const QByteArray &font, const char *tag) {
    const int count = quint8(font[4]) << 8 | quint8(font[5]);
    for (int i = 0; i < count; ++i) {
        const int record = 12 + 16 * i;
        if (font.mid(record, 4) == tag) return font.mid(int(bigEndian32(font, record + 8)), int(bigEndian32(font, record + 12)));
    }
    return {};
}
QByteArray rectangleFont() {
    return fontBytes(described(project({{{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 20, 30}))}}})).font, FontFormat::Otf);
}
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
    void sfntHeaderTableOrderAndChecksums() {
        const auto bytes = rectangleFont();
        QCOMPARE(bytes.left(12), QByteArray::fromHex("4f54544f" "0009" "0080" "0003" "0010"));
        QByteArray tags;
        for (int i = 0; i < 9; ++i) {
            const int record = 12 + 16 * i;
            tags += bytes.mid(record, 4);
            const auto offset = int(bigEndian32(bytes, record + 8));
            QCOMPARE(offset % 4, 0);
            auto data = bytes.mid(offset, int(bigEndian32(bytes, record + 12)));
            if (bytes.mid(record, 4) == "head") data.replace(8, 4, QByteArray(4, '\0'));
            QCOMPARE(sum32(data), bigEndian32(bytes, record + 4));
        }
        QCOMPARE(tags, QByteArray("CFF OS/2cmapheadhheahmtxmaxpnamepost"));
        QCOMPARE(sum32(bytes), 0xB1B0AFBAu);
    }
    void sameDescriptionEncodesToIdenticalBytes() {
        QCOMPARE(rectangleFont(), rectangleFont());
    }
    void rectangleCharstringOmitsTheClosingLine() {
        QVERIFY(table(rectangleFont(), "CFF ").contains(QByteArray::fromHex("f8888b15" "9f8b8ba9778b05" "0e")));
    }
    void cmapFormat4MapsTheLabel() {
        QCOMPARE(table(rectangleFont(), "cmap"), QByteArray::fromHex(
            "0000" "0002" "00000003" "00000014" "00030001" "00000014"
            "0004" "0020" "0000" "0004" "0004" "0001" "0000"
            "0041ffff" "0000" "0041ffff" "ffc00001" "00000000"));
    }
    void cmapAddsFormat12AboveTheBmp() {
        const auto font = described(project({{{{1}, QString::fromUcs4(U"\U0001F600", 1)}, {}}})).font;
        const auto cmap = table(fontBytes(font, FontFormat::Otf), "cmap");
        QCOMPARE(cmap.left(36), QByteArray::fromHex(
            "0000" "0004" "00000003" "00000024" "00000004" "0000003c" "00030001" "00000024" "0003000a" "0000003c"));
        QCOMPARE(cmap.mid(0x3c), QByteArray::fromHex("000c" "0000" "0000001c" "00000000" "00000001" "0001f600" "0001f600" "00000001"));
    }
    void headAndHmtxCarryMetrics() {
        const auto bytes = rectangleFont();
        const auto head = table(bytes, "head");
        QCOMPARE(head.mid(18, 2), QByteArray::fromHex("03e8"));
        QCOMPARE(head.mid(20, 16), QByteArray::fromHex("00000000e57ce625" "00000000e57ce625"));
        QCOMPARE(head.mid(36, 8), QByteArray::fromHex("0064" "0000" "0384" "02bc"));
        QCOMPARE(table(bytes, "hmtx"), QByteArray::fromHex("03e80064" "03e801f4"));
        QCOMPARE(table(bytes, "maxp"), QByteArray::fromHex("00005000" "0002"));
    }
    void exportedFileLoadsInThePlatformRasterizer() {
        QTemporaryDir directory;
        const auto path = directory.filePath("Test Face.otf");
        const auto stroke = makePenPath({{{-100, 0}, {}}, {{100, 100}, {}}});
        const auto result = exportFont(path, project({
            {{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 20, 30})), {2, stroke, ShapePaint::Stroke}}},
            {{{2}, " "}, {}},
            {{{3}, "O"}, {filled(1, ring()), filled(2, makeShape(ShapeKind::Rectangle, {-10, 90, 20, 20}))}},
            {{{4}, "o"}, {filled(1, makeShape(ShapeKind::Ellipse, {-100, 0, 200, 200}))}}}), settings(), FontFormat::Otf);
        const auto &report = std::get<FontExportReport>(result);
        QCOMPARE(report.glyphCount, std::size_t(5));
        QCOMPARE(report.warnings.size(), std::size_t(1));
        QCOMPARE(report.warnings[0].kind, ExportWarningKind::StrokeSkipped);

        QRawFont raw(path, 1000, QFont::PreferNoHinting);
        QVERIFY(raw.isValid());
        QCOMPARE(raw.familyName(), QString("Test Face"));
        QCOMPARE(raw.unitsPerEm(), 1000.);
        QCOMPARE(raw.glyphIndexesForString("A Oo"), (QList<quint32>{1, 2, 3, 4}));
        QCOMPARE(raw.advancesForGlyphIndexes(QList<quint32>{1, 2, 3}), (QList<QPointF>{{1000, 0}, {1000, 0}, {1000, 0}}));
        QCOMPARE(raw.pathForGlyph(1).boundingRect(), QRectF(500, -30, 20, 30));
        QCOMPARE(raw.pathForGlyph(2).isEmpty(), true);
        QCOMPARE(raw.pathForGlyph(4).boundingRect(), QRectF(400, -200, 200, 200));
        auto o = raw.pathForGlyph(3);
        o.setFillRule(Qt::WindingFill);
        QCOMPARE(o.boundingRect(), QRectF(400, -200, 200, 200));
        QCOMPARE(o.contains(QPointF(410, -100)), true);
        QCOMPARE(o.contains(QPointF(500, -100)), true);
        QCOMPARE(o.contains(QPointF(470, -70)), false);
    }
    void nonThousandUnitsPerEmScalesThroughTheFontMatrix() {
        QTemporaryDir directory;
        const auto path = directory.filePath("Wide.otf");
        auto p = project({{{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 1024, 512}))}}});
        p.definition.unitsPerEm = 2048;
        p.definition.bounds = QRectF(-1024, -512, 2048, 2048);
        QVERIFY(std::holds_alternative<FontExportReport>(exportFont(path, p, settings("Wide"), FontFormat::Otf)));
        QRawFont raw(path, 2048, QFont::PreferNoHinting);
        QVERIFY(raw.isValid());
        QCOMPARE(raw.unitsPerEm(), 2048.);
        QCOMPARE(raw.advancesForGlyphIndexes(QList<quint32>{1}).at(0).x(), 2048.);
        QCOMPARE(raw.pathForGlyph(1).boundingRect(), QRectF(1024, -512, 1024, 512));
    }
    void exportFailureReportsTheFileError() {
        const auto result = exportFont("/nonexistent-fontu-folder/x.otf", project({{{{1}, "A"}, {}}}), settings(), FontFormat::Otf);
        QVERIFY(std::holds_alternative<QString>(result));
        QVERIFY(!QFile::exists("/nonexistent-fontu-folder/x.otf"));
    }
};
QTEST_MAIN(FontExportTests)
#include "font_export_tests.moc"
