#pragma once
#include "document.h"
#include <QByteArray>
#include <QString>
#include <variant>

namespace fontu {
struct GlyphId {
    std::uint64_t value;
    friend bool operator==(GlyphId a, GlyphId b) { return a.value == b.value; }
};
struct GlyphSeed { GlyphId id; QString label; };
struct ProjectGlyph { GlyphSeed seed; std::vector<Shape> shapes; };
struct ProjectSnapshot {
    GlyphDocumentDefinition definition;
    std::vector<ProjectGlyph> glyphs;
    GlyphId activeGlyph;
};
using ProjectReadResult = std::variant<ProjectSnapshot, QString>;
QByteArray projectBytes(const ProjectSnapshot &project, bool includeActiveGlyph = true);
ProjectReadResult parseProject(const QByteArray &bytes);
ProjectReadResult readProject(const QString &path);
QString writeProject(const QString &path, const ProjectSnapshot &project);
QString defaultProjectFolder();
}
