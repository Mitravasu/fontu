#include "project_creation.h"
#include <cmath>
#include <unordered_set>

namespace fontu {
const std::array<CreationPreset, 3> &creationPresets() {
    static const auto presets = [] {
        GlyphDocumentDefinition scaled;
        scaled.bounds = {-1024, -512, 2048, 2048};
        scaled.unitsPerEm = 2048;
        scaled.xHeight = 1024;
        scaled.capHeight = 1434;
        scaled.ascender = 1536;
        scaled.descender = -512;
        GlyphDocumentDefinition roomy;
        roomy.bounds = {-600, -350, 1200, 1400};
        return std::array<CreationPreset, 3>{{
            {"Classic 1000", "1000 × 1000 units at 1000 UPM. The familiar compact glyph workspace.", {}},
            {"2048 UPM", "2048 × 2048 units with scaled guide proportions. An example at a larger design scale.", scaled},
            {"Roomy 1000", "1200 × 1400 units at 1000 UPM. Extra drawing room beyond the guides for accents and overshoot.", roomy}}};
    }();
    return presets;
}
QString builtinCharacterText(BuiltinCharacters characters) {
    QString result;
    if (characters == BuiltinCharacters::Numbers) {
        for (char16_t c = u'0'; c <= u'9'; ++c) result.append(QChar(c));
    } else if (characters == BuiltinCharacters::Letters) {
        for (char16_t c = u'A'; c <= u'Z'; ++c) result.append(QChar(c));
        for (char16_t c = u'a'; c <= u'z'; ++c) result.append(QChar(c));
    } else {
        for (char16_t c = 0x20; c <= 0x7e; ++c) result.append(QChar(c));
    }
    return result;
}
QString characterCoverageDescription(BuiltinCharacters characters) {
    switch (characters) {
    case BuiltinCharacters::Numbers: return "10 glyphs. Digits 0 through 9.";
    case BuiltinCharacters::Letters: return "52 glyphs. A through Z, then a through z. Basic Latin letters only.";
    case BuiltinCharacters::Programming: return "95 glyphs. Every printable ASCII character U+0020 through U+007E, including space, letters, digits, punctuation and symbols.";
    }
    return {};
}
ProjectCreationResult createProject(const ProjectCreationDefinition &request) {
    const auto &d = request.definition;
    const auto wholeUnit = [](double value) {
        return std::isfinite(value) && value >= 16 && value <= 16384 && std::floor(value) == value;
    };
    if (!wholeUnit(d.bounds.width()) || !wholeUnit(d.bounds.height())
        || d.bounds.width() > 8192 || d.bounds.height() > 8192
        || d.bounds.x() != -d.bounds.width() / 2 || d.bounds.y() != -d.bounds.height() / 4)
        return CreationError{CreationField::Dimensions, "Width and height must be whole font units from 16 to 8192."};
    if (!wholeUnit(d.unitsPerEm))
        return CreationError{CreationField::UnitsPerEm, "Units per em must be a whole number from 16 to 16384."};
    for (const auto &guide : d.guides()) {
        if (!std::isfinite(guide.position) || guide.position < d.bounds.top() || guide.position > d.bounds.bottom())
            return CreationError{CreationField::Guides, QString::fromUtf8(guide.name) + " must be inside the workspace, even when disabled."};
    }
    if (!(d.descender <= d.baseline && d.baseline <= d.xHeight && d.xHeight <= d.capHeight && d.capHeight <= d.ascender))
        return CreationError{CreationField::Guides, "Guide values must be ordered. Descender ≤ baseline ≤ x-height ≤ cap height ≤ ascender."};
    const auto text = std::holds_alternative<CustomCharacters>(request.coverage)
        ? std::get<CustomCharacters>(request.coverage).text : builtinCharacterText(std::get<BuiltinCharacters>(request.coverage));
    if (text.isEmpty()) return CreationError{CreationField::Characters, "Enter at least one custom character."};
    if (text.size() > 200000) return CreationError{CreationField::Characters, "Custom text is limited to 200000 UTF-16 units."};
    ProjectSnapshot project{d, {}, {1}};
    std::unordered_set<char32_t> seen;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const auto c = text[index];
        char32_t scalar = c.unicode();
        if (c.isHighSurrogate()) {
            if (index + 1 >= text.size() || !text[index + 1].isLowSurrogate())
                return CreationError{CreationField::Characters, "Custom text contains an unpaired Unicode surrogate."};
            scalar = QChar::surrogateToUcs4(c, text[++index]);
        } else if (c.isLowSurrogate()) {
            return CreationError{CreationField::Characters, "Custom text contains an unpaired Unicode surrogate."};
        }
        if (!seen.insert(scalar).second) continue;
        if (seen.size() > 4096)
            return CreationError{CreationField::Characters, "New projects can contain at most 4096 distinct characters."};
        project.glyphs.push_back({{{std::uint64_t(project.glyphs.size() + 1)}, QString::fromUcs4(&scalar, 1)}, {}});
    }
    return project;
}
}
