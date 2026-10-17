#include "project.h"
#include "project_creation.h"
#include "snapping.h"
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
    void creationCoverageAndUnicode() {
        ProjectCreationDefinition request;
        auto result = createProject(request);
        QVERIFY(std::holds_alternative<ProjectSnapshot>(result));
        auto project = std::get<ProjectSnapshot>(result);
        QCOMPARE(project.glyphs.size(), std::size_t(10));
        QCOMPARE(project.glyphs.front().seed.label, QString("0"));
        QCOMPARE(project.glyphs.back().seed.label, QString("9"));
        request.coverage = BuiltinCharacters::Letters;
        project = std::get<ProjectSnapshot>(createProject(request));
        QString letters;
        for (const auto &glyph : project.glyphs) letters += glyph.seed.label;
        QCOMPARE(letters, QString("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
        request.coverage = BuiltinCharacters::Programming;
        project = std::get<ProjectSnapshot>(createProject(request));
        QCOMPARE(project.glyphs.size(), std::size_t(95));
        for (std::size_t i = 0; i < project.glyphs.size(); ++i) {
            QCOMPARE(project.glyphs[i].seed.label, QString(QChar(char16_t(0x20 + i))));
            QCOMPARE(project.glyphs[i].seed.id.value, std::uint64_t(i + 1));
        }
        request.coverage = CustomCharacters{QString::fromUtf8("A😀Aé😀é \n") + QChar(0)};
        project = std::get<ProjectSnapshot>(createProject(request));
        QCOMPARE(project.glyphs.size(), std::size_t(8));
        QCOMPARE(project.glyphs[1].seed.label, QString::fromUtf8("😀"));
        QCOMPARE(project.glyphs[2].seed.label, QString::fromUtf8("é"));
        QCOMPARE(project.glyphs[4].seed.label, QString::fromUtf8("́"));
        QCOMPARE(project.glyphs[7].seed.label, QString(QChar(0)));
        const auto roundtrip = parseProject(projectBytes(project));
        QVERIFY(std::holds_alternative<ProjectSnapshot>(roundtrip));
        QCOMPARE(projectBytes(std::get<ProjectSnapshot>(roundtrip)), projectBytes(project));
    }
    void creationValidationAndPresets() {
        for (const auto &preset : creationPresets()) {
            auto result = createProject({preset.definition, BuiltinCharacters::Numbers});
            QVERIFY(std::holds_alternative<ProjectSnapshot>(result));
        }
        const auto roomy = std::get<ProjectSnapshot>(createProject({creationPresets()[2].definition, BuiltinCharacters::Letters}));
        QCOMPARE(roomy.definition.bounds, QRectF(-600, -350, 1200, 1400));
        QCOMPARE(roomy.definition.unitsPerEm, 1000.);
        QCOMPARE(roomy.definition.ascender, 750.);
        auto request = ProjectCreationDefinition{};
        const auto rejected = [](const ProjectCreationDefinition &r) { return std::holds_alternative<CreationError>(createProject(r)); };
        request.coverage = CustomCharacters{};
        QVERIFY(rejected(request));
        request.coverage = CustomCharacters{QString(QChar(0xd800))};
        QVERIFY(rejected(request));
        request.coverage = CustomCharacters{QString(QChar(0xdc00))};
        QVERIFY(rejected(request));
        QString excessive;
        for (char32_t c = 0x1000; c < 0x2001; ++c) excessive += QString::fromUcs4(&c, 1);
        request.coverage = CustomCharacters{excessive};
        QVERIFY(rejected(request));
        request.coverage = CustomCharacters{QString(200001, QChar('a'))};
        QVERIFY(rejected(request));
        request = {};
        request.definition.guideEnabled.ascender = false;
        request.definition.ascender = 751;
        QVERIFY(rejected(request));
        request = {};
        request.definition.unitsPerEm = 1000.5;
        QVERIFY(rejected(request));
        request = {};
        request.definition.bounds = {-7.5, -250, 15, 1000};
        QVERIFY(rejected(request));
        request = {};
        request.definition.baseline = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(rejected(request));
        for (const double extent : {16., 8192.}) {
            request = {};
            auto &d = request.definition;
            d.bounds = {-extent / 2, -extent / 4, extent, extent};
            d.unitsPerEm = extent;
            d.descender = -extent / 4; d.baseline = 0; d.xHeight = extent / 2;
            d.capHeight = extent * .7; d.ascender = extent * .75; d.sideBearing = extent / 10;
            const auto accepted = createProject(request);
            QVERIFY(std::holds_alternative<ProjectSnapshot>(accepted));
            QCOMPARE(std::get<ProjectSnapshot>(accepted).definition.bounds.width(), extent);
        }
    }
    void presetsValidateAndGuideErrorsAreSpecific() {
        for (const auto &preset : creationPresets()) {
            const auto error = validateWorkspace(preset.definition);
            QVERIFY2(!error, preset.name);
        }
        auto d = creationPresets()[1].definition;
        d.xHeight = 1200; d.capHeight = 1600; d.ascender = 1800;
        auto error = validateWorkspace(d);
        QVERIFY(error);
        QCOMPARE(error->field, CreationField::Guides);
        QCOMPARE(error->guides, QStringList({"Ascender", "Cap height"}));
        QCOMPARE(error->message, QString("Ascender (1800) and Cap height (1600) are above the workspace top (1536). "
                                         "Raise Height to at least 2400 units, or move those guides inside -512 to 1536."));
        QVERIFY(std::holds_alternative<CreationError>(createProject({d, BuiltinCharacters::Numbers})));
        d = {};
        d.descender = -300; d.guideEnabled.descender = false;
        error = validateWorkspace(d);
        QVERIFY(error);
        QCOMPARE(error->guides, QStringList({"Descender"}));
        QVERIFY(error->message.startsWith("Descender (-300) is below the workspace bottom (-250). Raise Height to at least 1200 units, or move Descender inside -250 to 750."));
        QVERIFY(error->message.contains("Unchecked guides must fit too"));
        d = {};
        d.ascender = 7000;
        QVERIFY(!validateWorkspace(d)->message.contains("Raise Height")); // Would need more than 8192.
        d = {};
        d.xHeight = 720;
        error = validateWorkspace(d);
        QCOMPARE(error->guides, QStringList({"x-height", "Cap height"}));
        QVERIFY(error->message.startsWith("x-height (720) is above Cap height (700)."));
    }
    void optionalGuidesRoundtripAndSnapping() {
        auto project = fixture();
        project.definition.guideEnabled = {false, true, false, true, false};
        const auto loaded = std::get<ProjectSnapshot>(parseProject(projectBytes(project)));
        QVERIFY(!loaded.definition.guideEnabled.baseline);
        QVERIFY(loaded.definition.guideEnabled.xHeight);
        QVERIFY(!loaded.definition.guideEnabled.capHeight);
        QCOMPARE(projectBytes(loaded), projectBytes(project));
        auto root = QJsonDocument::fromJson(projectBytes(project)).object();
        auto definition = root["definition"].toObject();
        definition.remove("guideEnabled"); root["definition"] = definition;
        const auto legacy = std::get<ProjectSnapshot>(parseProject(QJsonDocument(root).toJson()));
        for (const auto &guide : legacy.definition.guides()) QVERIFY(guide.enabled);
        definition["guideEnabled"] = QJsonObject{{"baseline", "false"}}; root["definition"] = definition;
        QVERIFY(std::holds_alternative<QString>(parseProject(QJsonDocument(root).toJson())));
        GlyphDocumentDefinition d;
        d.guideEnabled.xHeight = false;
        Viewport view;
        Document hidden(d, {});
        QVERIFY(!snapPoint(view.toScreen({123, 502}), hidden, view, {false, true}));
        d.guideEnabled.xHeight = true;
        const auto snapped = snapPoint(view.toScreen({123, 502}), Document(d, {}), view, {false, true});
        QVERIFY(snapped);
        QCOMPARE(view.toDocument(snapped->point), QPointF(123, 500));
    }
    void sideBearingGuidesValidateRoundtripAndSnap() {
        GlyphDocumentDefinition d;
        QCOMPARE(d.sideBearingGuides(), (std::array<double, 2>{-400, 400}));
        QCOMPARE(creationPresets()[2].definition.sideBearingGuides(), (std::array<double, 2>{-400, 400}));
        d.sideBearing = 500;
        QVERIFY(!validateWorkspace(d)); // The lines may meet at the centre.
        d.sideBearing = 501;
        auto error = validateWorkspace(d);
        QVERIFY(error);
        QCOMPARE(error->field, CreationField::Guides);
        QCOMPARE(error->guides, QStringList({"Side bearing"}));
        QVERIFY(error->message.startsWith("Side bearing (501) must be from 0 to 500"));
        d.sideBearing = -1;
        QVERIFY(validateWorkspace(d));

        auto project = fixture();
        project.definition.sideBearing = 150;
        project.definition.guideEnabled.sideBearing = false;
        const auto loaded = std::get<ProjectSnapshot>(parseProject(projectBytes(project)));
        QCOMPARE(loaded.definition.sideBearing, 150.);
        QVERIFY(!loaded.definition.guideEnabled.sideBearing);
        QCOMPARE(projectBytes(loaded), projectBytes(project));
        auto root = QJsonDocument::fromJson(projectBytes(project)).object();
        auto definition = root["definition"].toObject();
        definition.remove("sideBearing"); definition.remove("guideEnabled"); root["definition"] = definition;
        const auto legacy = std::get<ProjectSnapshot>(parseProject(QJsonDocument(root).toJson()));
        QCOMPARE(legacy.definition.sideBearing, project.definition.bounds.width() / 10);
        QVERIFY(legacy.definition.guideEnabled.sideBearing);
        definition["sideBearing"] = project.definition.bounds.width(); root["definition"] = definition;
        QVERIFY(std::holds_alternative<QString>(parseProject(QJsonDocument(root).toJson())));

        d = {};
        Viewport view;
        const auto snapped = snapPoint(view.toScreen({397, 260}), Document(d, {}), view, {false, true});
        QVERIFY(snapped);
        QCOMPARE(view.toDocument(snapped->point), QPointF(400, 260));
        const auto crossing = snapPoint(view.toScreen({-397, 697}), Document(d, {}), view, {false, true});
        QVERIFY(crossing);
        QCOMPARE(view.toDocument(crossing->point), QPointF(-400, 700));
        d.guideEnabled.sideBearing = false;
        QVERIFY(!snapPoint(view.toScreen({397, 260}), Document(d, {}), view, {false, true}));
    }
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
