#pragma once
#include "project.h"
#include <QStringList>
#include <array>
#include <optional>

namespace fontu {
enum class BuiltinCharacters { Numbers, Letters, Programming };
struct CustomCharacters { QString text; };
using CharacterCoverage = std::variant<BuiltinCharacters, CustomCharacters>;
struct ProjectCreationDefinition {
    GlyphDocumentDefinition definition;
    CharacterCoverage coverage = BuiltinCharacters::Numbers;
};
struct CreationPreset { const char *name; const char *description; GlyphDocumentDefinition definition; };
enum class CreationField { Dimensions, UnitsPerEm, Guides, Characters };
struct CreationError {
    CreationField field;
    QString message;
    // TypographyGuide names (e.g. "Ascender") whose values cause a Guides error.
    QStringList guides = {};
};
using ProjectCreationResult = std::variant<ProjectSnapshot, CreationError>;
const std::array<CreationPreset, 3> &creationPresets();
QString builtinCharacterText(BuiltinCharacters characters);
QString characterCoverageDescription(BuiltinCharacters characters);
// Checks dimensions, UPM, and guide placement/order; characters are not checked.
std::optional<CreationError> validateWorkspace(const GlyphDocumentDefinition &definition);
ProjectCreationResult createProject(const ProjectCreationDefinition &request);
}
