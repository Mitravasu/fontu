#pragma once
#include "project.h"
#include <array>

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
struct CreationError { CreationField field; QString message; };
using ProjectCreationResult = std::variant<ProjectSnapshot, CreationError>;
const std::array<CreationPreset, 3> &creationPresets();
QString builtinCharacterText(BuiltinCharacters characters);
QString characterCoverageDescription(BuiltinCharacters characters);
ProjectCreationResult createProject(const ProjectCreationDefinition &request);
}
