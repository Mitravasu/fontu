#include "project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace fontu {
namespace {
constexpr qint64 maximumFileSize = 64 * 1024 * 1024;
constexpr double maximumCoordinate = 1e9;
struct InvalidProject : std::runtime_error { using std::runtime_error::runtime_error; };
void check(bool valid, const char *message) { if (!valid) throw InvalidProject(message); }
QJsonObject object(const QJsonValue &value) {
    check(value.isObject(), "Expected a project object.");
    return value.toObject();
}
QJsonArray array(const QJsonValue &value) {
    check(value.isArray(), "Expected a project array.");
    return value.toArray();
}
double number(const QJsonValue &value) {
    check(value.isDouble(), "Expected a finite number.");
    const double result = value.toDouble();
    check(std::isfinite(result) && std::abs(result) <= maximumCoordinate, "A number is outside the supported range.");
    return result;
}
std::uint64_t identity(const QJsonValue &value) {
    check(value.isString(), "IDs must be decimal strings.");
    const auto text = value.toString();
    check(!text.isEmpty() && text.size() <= 20 && (text == "0" || text.front() != '0'), "Invalid ID.");
    for (const auto character : text) check(character >= '0' && character <= '9', "Invalid ID.");
    bool ok = false;
    const auto result = text.toULongLong(&ok);
    check(ok && result < std::numeric_limits<std::uint64_t>::max(), "ID is outside the supported range.");
    return result;
}
QPointF readPoint(const QJsonArray &value, int offset = 0) {
    return {number(value.at(offset)), number(value.at(offset + 1))};
}
QJsonObject definitionObject(const GlyphDocumentDefinition &d) {
    return {{"bounds", QJsonArray{d.bounds.x(), d.bounds.y(), d.bounds.width(), d.bounds.height()}},
            {"unitsPerEm", d.unitsPerEm}, {"baseline", d.baseline}, {"xHeight", d.xHeight},
            {"capHeight", d.capHeight}, {"ascender", d.ascender}, {"descender", d.descender},
            {"sideBearing", d.sideBearing},
            {"guideEnabled", QJsonObject{{"baseline", d.guideEnabled.baseline}, {"xHeight", d.guideEnabled.xHeight},
                {"capHeight", d.guideEnabled.capHeight}, {"ascender", d.guideEnabled.ascender},
                {"descender", d.guideEnabled.descender}, {"sideBearing", d.guideEnabled.sideBearing}}}};
}
GlyphDocumentDefinition readDefinition(const QJsonValue &value) {
    const auto o = object(value);
    const auto b = array(o.value("bounds"));
    check(b.size() == 4, "Bounds require four numbers.");
    GlyphDocumentDefinition d;
    d.bounds = {number(b[0]), number(b[1]), number(b[2]), number(b[3])};
    check(d.bounds.width() >= .001 && d.bounds.height() >= .001
          && std::isfinite(d.bounds.right()) && std::isfinite(d.bounds.bottom())
          && std::abs(d.bounds.right()) <= maximumCoordinate && std::abs(d.bounds.bottom()) <= maximumCoordinate,
          "Invalid document bounds.");
    d.unitsPerEm = number(o.value("unitsPerEm"));
    check(d.unitsPerEm > 0, "Units per em must be positive.");
    d.baseline = number(o.value("baseline"));
    d.xHeight = number(o.value("xHeight"));
    d.capHeight = number(o.value("capHeight"));
    d.ascender = number(o.value("ascender"));
    d.descender = number(o.value("descender"));
    // Projects saved before side-bearing guides get one tenth of the width, like a new Classic project.
    d.sideBearing = o.contains("sideBearing") ? number(o.value("sideBearing")) : d.bounds.width() / 10;
    if (o.contains("guideEnabled")) {
        const auto enabled = object(o.value("guideEnabled"));
        const auto flag = [&enabled](const char *name) {
            if (!enabled.contains(name)) return true;
            check(enabled.value(name).isBool(), "Guide enablement must be boolean.");
            return enabled.value(name).toBool();
        };
        d.guideEnabled = {flag("baseline"), flag("xHeight"), flag("capHeight"), flag("ascender"), flag("descender"),
                          flag("sideBearing")};
    }
    check(d.descender <= d.baseline && d.baseline <= d.xHeight && d.xHeight <= d.capHeight
          && d.capHeight <= d.ascender && d.descender >= d.bounds.top() && d.ascender <= d.bounds.bottom(),
          "Typography guides must be ordered and inside the document.");
    check(d.sideBearing >= 0 && d.sideBearing <= d.bounds.width() / 2,
          "Side bearing must be from 0 to half the document width.");
    return d;
}
QJsonObject shapeObject(const Shape &shape) {
    QJsonArray commands;
    for (int i = 0; i < shape.path.elementCount(); ++i) {
        const auto e = shape.path.elementAt(i);
        if (e.isMoveTo()) commands.append(QJsonArray{"move", e.x, e.y});
        else if (e.isLineTo()) commands.append(QJsonArray{"line", e.x, e.y});
        else if (e.isCurveTo()) {
            const auto c = shape.path.elementAt(++i);
            const auto end = shape.path.elementAt(++i);
            commands.append(QJsonArray{"cubic", e.x, e.y, c.x, c.y, end.x, end.y});
        }
    }
    return {{"id", QString::number(shape.id)}, {"paint", shape.paint == ShapePaint::Filled ? "filled" : "stroke"},
            {"fillRule", shape.path.fillRule() == Qt::OddEvenFill ? "oddEven" : "winding"}, {"commands", commands}};
}
Shape readShape(const QJsonValue &value, const GlyphDocumentDefinition &definition) {
    const auto o = object(value);
    Shape shape{identity(o.value("id")), {}};
    check(shape.id > 0, "Shape IDs must be positive.");
    const auto paint = o.value("paint").toString();
    check(paint == "filled" || paint == "stroke", "Unknown shape paint.");
    shape.paint = paint == "filled" ? ShapePaint::Filled : ShapePaint::Stroke;
    const auto rule = o.value("fillRule").toString();
    check(rule == "oddEven" || rule == "winding", "Unknown path fill rule.");
    shape.path.setFillRule(rule == "oddEven" ? Qt::OddEvenFill : Qt::WindingFill);
    const auto commands = array(o.value("commands"));
    check(!commands.isEmpty() && commands.size() <= 100000, "Invalid path command count.");
    bool started = false;
    for (const auto &value : commands) {
        const auto command = array(value);
        check(!command.isEmpty() && command[0].isString(), "Invalid path command.");
        const auto kind = command[0].toString();
        if (kind == "move") {
            check(command.size() == 3, "Invalid move command.");
            shape.path.moveTo(readPoint(command, 1));
            started = true;
        } else if (kind == "line") {
            check(started && command.size() == 3, "Invalid line command.");
            shape.path.lineTo(readPoint(command, 1));
        } else if (kind == "cubic") {
            check(started && command.size() == 7, "Invalid cubic command.");
            shape.path.cubicTo(readPoint(command, 1), readPoint(command, 3), readPoint(command, 5));
        } else throw InvalidProject("Unknown path command.");
    }
    Document admission(definition, {});
    check(admission.addPath(shape.path, shape.paint).has_value(), "Path is empty, degenerate, or outside the document.");
    return shape;
}
}
QByteArray projectBytes(const ProjectSnapshot &project, bool includeActiveGlyph) {
    QJsonArray glyphs;
    for (const auto &glyph : project.glyphs) {
        QJsonArray shapes;
        for (const auto &shape : glyph.shapes) shapes.append(shapeObject(shape));
        glyphs.append(QJsonObject{{"id", QString::number(glyph.seed.id.value)}, {"label", glyph.seed.label}, {"shapes", shapes}});
    }
    QJsonObject result{{"format", "fontu"}, {"version", 1}, {"definition", definitionObject(project.definition)}, {"glyphs", glyphs}};
    if (includeActiveGlyph) result.insert("activeGlyph", QString::number(project.activeGlyph.value));
    return QJsonDocument(result).toJson(QJsonDocument::Indented);
}
ProjectReadResult parseProject(const QByteArray &bytes) {
    try {
        check(bytes.size() <= maximumFileSize, "Project file is too large.");
        QJsonParseError error;
        const auto json = QJsonDocument::fromJson(bytes, &error);
        check(error.error == QJsonParseError::NoError && json.isObject(), "Project is not valid JSON.");
        const auto root = json.object();
        check(root.value("format").toString() == "fontu", "This is not a fontu project.");
        check(root.value("version").isDouble() && root.value("version").toDouble() == 1, "Unsupported project version.");
        ProjectSnapshot project{readDefinition(root.value("definition")), {}, {0}};
        const auto glyphs = array(root.value("glyphs"));
        check(!glyphs.isEmpty() && glyphs.size() <= 100000, "Invalid glyph count.");
        std::unordered_set<std::uint64_t> glyphIds;
        for (const auto &value : glyphs) {
            const auto o = object(value);
            ProjectGlyph glyph{{{identity(o.value("id"))}, o.value("label").toString()}, {}};
            check(glyphIds.insert(glyph.seed.id.value).second, "Duplicate glyph ID.");
            check(o.value("label").isString() && !glyph.seed.label.isEmpty() && glyph.seed.label.size() <= 4096, "Invalid glyph label.");
            const auto shapes = array(o.value("shapes"));
            check(shapes.size() <= 100000, "Too many shapes.");
            std::unordered_set<ShapeId> shapeIds;
            for (const auto &value : shapes) {
                auto shape = readShape(value, project.definition);
                check(shapeIds.insert(shape.id).second, "Duplicate shape ID.");
                glyph.shapes.push_back(std::move(shape));
            }
            project.glyphs.push_back(std::move(glyph));
        }
        project.activeGlyph = root.contains("activeGlyph") ? GlyphId{identity(root.value("activeGlyph"))} : project.glyphs.front().seed.id;
        check(glyphIds.count(project.activeGlyph.value) == 1, "Active glyph is not in this project.");
        return project;
    } catch (const InvalidProject &error) { return QString::fromUtf8(error.what()); }
}
ProjectReadResult readProject(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return file.errorString();
    if (file.size() > maximumFileSize) return QStringLiteral("Project file is too large.");
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return file.errorString();
    return parseProject(bytes);
}
QString writeProject(const QString &path, const ProjectSnapshot &project) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return file.errorString();
    const auto bytes = projectBytes(project);
    if (file.write(bytes) != bytes.size()) return file.errorString();
    if (!file.commit()) return file.errorString();
    return {};
}
QString defaultProjectFolder() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("fontu");
}
}
