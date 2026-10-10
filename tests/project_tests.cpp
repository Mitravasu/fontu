#include "project.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>
using namespace fontu;
namespace {
ProjectSnapshot fixture() {
    GlyphDocumentDefinition definition;
    definition.bounds = {-600, -300, 1200, 1200};
    definition.unitsPerEm = 1200;
    definition.baseline = 10;
    definition.xHeight = 520;
    definition.capHeight = 720;
    definition.ascender = 800;
    definition.descender = -280;
    auto stroke = makePenPath({{{-100, 0}, {25, 40}}, {{150, 200}, {30, -10}}, {{200, 0}, {}}});
    stroke.setFillRule(Qt::WindingFill);
    return {definition, {{{{9007199254740993ULL}, "alternate zero"}, {{43, makeShape(ShapeKind::Ellipse, {-200, 0, 100, 200}), ShapePaint::Filled}, {44, stroke, ShapePaint::Stroke}}},
                         {{{42}, "one"}, {{9, makeShape(ShapeKind::Rectangle, {0, 0, 20, 30}), ShapePaint::Filled}}}}, {42}};
}
}
class ProjectTests : public QObject {
    Q_OBJECT
private slots:
    void roundTripPreservesProjectAndGeometry() {
        const auto original = fixture();
        const auto result = parseProject(projectBytes(original));
        QVERIFY(std::holds_alternative<ProjectSnapshot>(result));
        const auto loaded = std::get<ProjectSnapshot>(result);
        QCOMPARE(loaded.glyphs.size(), std::size_t(2));
        QCOMPARE(loaded.glyphs[0].seed.id.value, std::uint64_t(9007199254740993ULL));
        QCOMPARE(loaded.glyphs[0].seed.label, QString("alternate zero"));
        QCOMPARE(loaded.activeGlyph.value, std::uint64_t(42));
        QCOMPARE(loaded.definition.bounds, QRectF(-600, -300, 1200, 1200));
        QCOMPARE(loaded.definition.unitsPerEm, 1200.);
        QCOMPARE(loaded.definition.baseline, 10.);
        QCOMPARE(loaded.definition.xHeight, 520.);
        QCOMPARE(loaded.definition.capHeight, 720.);
        QCOMPARE(loaded.definition.ascender, 800.);
        QCOMPARE(loaded.definition.descender, -280.);
        QCOMPARE(loaded.glyphs[0].shapes[0].id, ShapeId(43));
        QCOMPARE(loaded.glyphs[0].shapes[0].path.boundingRect(), QRectF(-200, 0, 100, 200));
        QCOMPARE(loaded.glyphs[0].shapes[1].paint, ShapePaint::Stroke);
        QCOMPARE(loaded.glyphs[0].shapes[1].path.fillRule(), Qt::WindingFill);
        QCOMPARE(loaded.glyphs[0].shapes[1].path, original.glyphs[0].shapes[1].path);
        QCOMPARE(loaded.glyphs[1].shapes[0].path, original.glyphs[1].shapes[0].path);
        Document restored(loaded.definition, loaded.glyphs[0].shapes);
        QCOMPARE(restored.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 10, 10})), std::optional<ShapeId>(45));
        auto viewOnly = loaded;
        viewOnly.activeGlyph = viewOnly.glyphs[0].seed.id;
        QCOMPARE(projectBytes(viewOnly, false), projectBytes(loaded, false));
        QVERIFY(projectBytes(viewOnly) != projectBytes(loaded));
    }
    void highShapeIdsAllocateReadableUniqueSuccessors() {
        auto project = fixture();
        project.glyphs[0].shapes[0].id = std::numeric_limits<ShapeId>::max() - 1;
        project.glyphs[0].shapes[1].id = 1;
        project.glyphs[0].shapes.push_back({3, makeShape(ShapeKind::Rectangle, {40, 40, 10, 10}), ShapePaint::Filled});
        const auto loaded = parseProject(projectBytes(project));
        QVERIFY(std::holds_alternative<ProjectSnapshot>(loaded));
        auto restored = std::get<ProjectSnapshot>(loaded);
        Document document(restored.definition, restored.glyphs[0].shapes);
        QCOMPARE(document.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 10, 10})), std::optional<ShapeId>(2));
        QCOMPARE(document.addPath(makeShape(ShapeKind::Rectangle, {20, 20, 10, 10})), std::optional<ShapeId>(4));
        restored.glyphs[0].shapes = document.shapes();
        const auto reopened = parseProject(projectBytes(restored));
        QVERIFY(std::holds_alternative<ProjectSnapshot>(reopened));
        QCOMPARE(std::get<ProjectSnapshot>(reopened).glyphs[0].shapes.back().id, ShapeId(4));
    }
    void invalidProjectsAreRejected_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("corrupt") << QByteArray("{broken");
        auto root = QJsonDocument::fromJson(projectBytes(fixture())).object();
        const auto add = [&](const char *name, QJsonObject altered) { QTest::newRow(name) << QJsonDocument(altered).toJson(); };
        auto altered = root; altered["version"] = 2; add("version", altered);
        altered = root; altered["version"] = 1.5; add("fractional version", altered);
        altered = root; altered["activeGlyph"] = "999"; add("unknown active glyph", altered);
        altered = root; altered["glyphs"] = QJsonArray{}; add("empty collection", altered);
        auto glyphs = root["glyphs"].toArray();
        auto glyph = glyphs[0].toObject();
        auto shapes = glyph["shapes"].toArray();
        const auto badGlyph = [&](const char *name, QJsonObject g) { auto gs = glyphs; gs[0] = g; auto r = root; r["glyphs"] = gs; add(name, r); };
        auto g = glyph; g["id"] = "18446744073709551615"; badGlyph("overflow ID", g);
        g = glyph; g["id"] = 42; badGlyph("numeric ID", g);
        g = glyph; g["id"] = "42"; badGlyph("duplicate glyph", g);
        g = glyph; g["label"] = ""; badGlyph("empty label", g);
        auto duplicateShapes = shapes; duplicateShapes.append(shapes[0]); g = glyph; g["shapes"] = duplicateShapes; badGlyph("duplicate shape", g);
        const auto badShape = [&](const char *name, QJsonObject shape) { auto ss = shapes; ss[0] = shape; auto g = glyph; g["shapes"] = ss; badGlyph(name, g); };
        auto shape = shapes[0].toObject(); shape["paint"] = "unknown"; badShape("paint", shape);
        shape = shapes[0].toObject(); shape["fillRule"] = "unknown"; badShape("fill rule", shape);
        shape = shapes[0].toObject(); shape["commands"] = QJsonArray{QJsonArray{"cubic", 0, 1, 2, 3}}; badShape("malformed cubic", shape);
        shape = shapes[0].toObject(); shape["commands"] = QJsonArray{QJsonArray{"move", 0, 0}, QJsonArray{"line", 999999999, 0}}; badShape("outside bounds", shape);
        shape = shapes[0].toObject(); shape["commands"] = QJsonArray{QJsonArray{"move", 0, 0}, QJsonArray{"line", "NaN", 0}}; badShape("invalid number", shape);
        auto d = root["definition"].toObject(); d["unitsPerEm"] = 0; altered = root; altered["definition"] = d; add("units", altered);
        d = root["definition"].toObject(); d["bounds"] = QJsonArray{0, 0, -100, 100}; altered = root; altered["definition"] = d; add("bounds", altered);
        d = root["definition"].toObject(); d["descender"] = 900; altered = root; altered["definition"] = d; add("metrics", altered);
    }
    void invalidProjectsAreRejected() {
        QFETCH(QByteArray, bytes);
        const auto result = parseProject(bytes);
        QVERIFY(std::holds_alternative<QString>(result));
        QVERIFY(!std::get<QString>(result).isEmpty());
    }
    void unknownFieldsAndOptionalActiveGlyph() {
        auto root = QJsonDocument::fromJson(projectBytes(fixture())).object();
        root["future"] = QJsonObject{{"metadata", true}};
        root.remove("activeGlyph");
        const auto result = parseProject(QJsonDocument(root).toJson());
        QVERIFY(std::holds_alternative<ProjectSnapshot>(result));
        QCOMPARE(std::get<ProjectSnapshot>(result).activeGlyph.value, std::uint64_t(9007199254740993ULL));
    }
    void atomicSaveAndReadErrors() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("test.fontu");
        QVERIFY(writeProject(path, fixture()).isEmpty());
        auto result = readProject(path);
        QVERIFY(std::holds_alternative<ProjectSnapshot>(result));
        QCOMPARE(std::get<ProjectSnapshot>(result).glyphs[0].shapes[1].id, ShapeId(44));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto original = file.readAll(); file.close();
        QVERIFY(!writeProject(directory.path(), fixture()).isEmpty());
        QVERIFY(!writeProject(directory.filePath("missing/test.fontu"), fixture()).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), original);
        QVERIFY(std::holds_alternative<QString>(readProject(directory.filePath("missing.fontu"))));
    }
};
QTEST_MAIN(ProjectTests)
#include "project_tests.moc"
