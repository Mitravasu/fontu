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
QByteArray encoded(const FontDescription &font) { return std::get<QByteArray>(fontBytes(font, FontFormat::Otf)); }
ProjectSnapshot labelled(const std::vector<char32_t> &codes) {
    std::vector<ProjectGlyph> glyphs;
    for (const auto code : codes) glyphs.push_back({{{glyphs.size() + 1}, QString::fromUcs4(&code, 1)}, {}});
    return project(std::move(glyphs));
}
QByteArray rectangleFont() {
    return std::get<QByteArray>(fontBytes(described(project({{{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 20, 30}))}}})).font, FontFormat::Otf));
}
QPoint at(FontPoint point) { return {point.x, point.y}; }
QList<QPoint> ends(const Contour &contour) {
    QList<QPoint> result;
    for (const auto &segment : contour.segments()) result.push_back(at(segmentEnd(segment)));
    return result;
}
}

class FontExportTests : public QObject {
    Q_OBJECT
private slots:
    void rectangleShiftsToDrawingAreaOriginAsACycle() {
        const auto font = described(project({{{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {0, 0, 20, 30}))}}})).font;
        QCOMPARE(font.glyphs.size(), std::size_t(2));
        QCOMPARE(font.glyphs.at(1).name, QString("uni0041"));
        QCOMPARE(int(font.glyphs.at(1).advance), 1000);
        const auto &c = font.glyphs.at(1).contours.at(0);
        QCOMPARE(at(c.start()), QPoint(500, 0));
        QCOMPARE(ends(c), (QList<QPoint>{{520, 0}, {520, 30}, {500, 30}, {500, 0}}));
        QCOMPARE(font.characterMap.at(U'A'), GlyphIndex(1));
    }
    void openFilledPathIsClosedWithALine() {
        QPainterPath open;
        open.moveTo(0, 0);
        open.lineTo(100, 0);
        open.lineTo(100, 100);
        const auto font = described(project({{{{1}, "v"}, {filled(1, open)}}})).font;
        const auto &c = font.glyphs.at(1).contours.at(0);
        QCOMPARE(ends(c), (QList<QPoint>{{600, 0}, {600, 100}, {500, 0}}));
    }
    void notdefOccupiesGlyphZeroAsAHollowBox() {
        const auto font = described(project({{{{1}, "A"}, {}}})).font;
        const auto &notdef = font.glyphs.at(0);
        QCOMPARE(notdef.name, QString(".notdef"));
        QCOMPARE(int(notdef.advance), 1000);
        QCOMPARE(ends(notdef.contours.at(0)), (QList<QPoint>{{900, 0}, {900, 700}, {100, 700}, {100, 0}}));
        QCOMPARE(ends(notdef.contours.at(1)), (QList<QPoint>{{150, 650}, {850, 650}, {850, 50}, {150, 50}}));
    }
    void nonzeroBaselineBecomesFontZeroAndHalvesRoundAwayFromZero() {
        auto p = project({{{{1}, "x"}, {filled(1, makeShape(ShapeKind::Rectangle, {0.5, 10, 20, 30.5}))}}});
        p.definition.baseline = 10;
        const auto font = described(p).font;
        QCOMPARE(at(font.glyphs.at(1).contours.at(0).start()), QPoint(501, 0));
        QCOMPARE(int(font.lineMetrics.ascender), 740);
        QCOMPARE(int(font.lineMetrics.descender), -260);
    }
    void evenOddHoleIsReversedToClockwise() {
        const auto result = described(project({{{{1}, "O"}, {filled(1, ring())}}}));
        const auto &contours = result.font.glyphs.at(1).contours;
        QCOMPARE(ends(contours.at(0)), (QList<QPoint>{{600, 0}, {600, 200}, {400, 200}, {400, 0}}));
        QCOMPARE(at(contours.at(1).start()), QPoint(450, 50));
        QCOMPARE(ends(contours.at(1)), (QList<QPoint>{{450, 150}, {550, 150}, {550, 50}, {450, 50}}));
        QCOMPARE(result.warnings.size(), std::size_t(0));
    }
    void ellipseKeepsItsFourCubics() {
        const auto font = described(project({{{{1}, "o"}, {filled(1, makeShape(ShapeKind::Ellipse, {-100, 0, 200, 200}))}}})).font;
        const auto &c = font.glyphs.at(1).contours.at(0);
        QCOMPARE(c.segments().size(), std::size_t(4));
        QCOMPARE(at(std::get<CubicSegment>(c.segments()[3]).end), QPoint(600, 100));
        QCOMPARE(at(c.start()), QPoint(600, 100));
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
    void rejectsADrawingAreaWiderThanAnAdvanceCanStore() {
        auto p = project({{{{1}, "A"}, {}}});
        p.definition.bounds = QRectF(-20000, -250, 40000, 1000);
        QCOMPARE(std::get<QString>(describeFont(p, settings())),
                 QString("The drawing area is 40000 font units wide. OpenType export supports at most 16383."));
    }
    void widestDrawingAreaKeepsMetricsInRange() {
        auto p = project({{{{1}, "A"}, {filled(1, makeShape(ShapeKind::Rectangle, {-24574.5, 0, 100, 30}))}}});
        p.definition.bounds = QRectF(-8191.5, -250, 16383, 1000);
        const auto bytes = encoded(described(p).font);
        QCOMPARE(table(bytes, "head").mid(36, 8), QByteArray::fromHex("c001" "0000" "3999" "02bc"));
        QCOMPARE(table(bytes, "hhea").mid(10, 8), QByteArray::fromHex("3fff" "c001" "0666" "3999"));
        QCOMPARE(table(bytes, "OS/2").mid(2, 2), QByteArray::fromHex("3fff"));
        QCOMPARE(table(bytes, "hmtx"), QByteArray::fromHex("3fff0666" "3fffc001"));
        QRawFont raw(bytes, 1000, QFont::PreferNoHinting);
        QCOMPARE(raw.advancesForGlyphIndexes(QList<quint32>{0, 1}), (QList<QPointF>{{16383, 0}, {16383, 0}}));
        QCOMPARE(raw.pathForGlyph(1).boundingRect(), QRectF(-16383, -30, 100, 30));
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
        const auto cmap = table(encoded(font), "cmap");
        QCOMPARE(cmap.left(36), QByteArray::fromHex(
            "0000" "0004" "00000003" "00000024" "00000004" "0000003c" "00030001" "00000024" "0003000a" "0000003c"));
        QCOMPARE(cmap.mid(0x3c), QByteArray::fromHex("000c" "0000" "0000001c" "00000000" "00000001" "0001f600" "0001f600" "00000001"));
    }
    void shuffledLabelsResolveThroughTheCharacterMap() {
        std::vector<char32_t> codes;
        for (char32_t code = U'A'; code <= U'Z'; ++code) codes.push_back(code);
        for (char32_t i = 0; i < 9000; ++i) codes.push_back(0x4E00 + i * 7919 % 9000);
        for (char32_t i = 0; i < 100; ++i) codes.push_back(0xAC00 + 5 * i);
        QTemporaryDir directory;
        const auto path = directory.filePath("Shuffled.otf");
        QVERIFY(std::holds_alternative<FontExportReport>(exportFont(path, labelled(codes), settings(), FontFormat::Otf)));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cmap = table(file.readAll(), "cmap");
        const int subtable = int(bigEndian32(cmap, 8));
        QCOMPARE(cmap.mid(subtable, 4), QByteArray::fromHex("0004" "4998"));
        QCOMPARE(cmap.size() - subtable, 18840);
        QRawFont raw(path, 1000, QFont::PreferNoHinting);
        int wrong = 0;
        for (std::size_t i = 0; i < codes.size(); ++i)
            if (raw.glyphIndexesForString(QString::fromUcs4(&codes[i], 1)) != QList<quint32>{quint32(i + 1)}) ++wrong;
        QCOMPARE(wrong, 0);
        QCOMPARE(raw.glyphIndexesForString(QString::fromUcs4(U"\uAC01", 1)), QList<quint32>{0});
    }
    void characterMapBeyondTheFormat4LimitIsAnExportError() {
        std::vector<char32_t> codes;
        for (char32_t i = 0; i < 8200; ++i) codes.push_back(0x100 + 5 * i);
        QCOMPARE(std::get<QString>(fontBytes(described(labelled(codes)).font, FontFormat::Otf)),
                 QString("The glyph labels below U+10000 are too scattered to fit an OpenType character map. "
                         "Remove some labels or group them into consecutive characters."));
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
