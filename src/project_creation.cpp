#include "project_creation.h"
#include <algorithm>
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
        scaled.sideBearing = 205;
        GlyphDocumentDefinition roomy;
        roomy.bounds = {-600, -350, 1200, 1400};
        roomy.sideBearing = 200; // The same space between the side guides as Classic.
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
namespace {
QString units(double value) { return QString::number(value, 'g', 12); }
QString joinNames(const QStringList &names) {
    if (names.size() < 2) return names.join(QString());
    return QStringList(names.mid(0, names.size() - 1)).join(", ") + " and " + names.back();
}
QString listGuides(const std::vector<TypographyGuide> &guides) {
    QStringList parts;
    for (const auto &guide : guides) parts << QString::fromUtf8(guide.name) + " (" + units(guide.position) + ")";
    return joinNames(parts);
}
}
std::optional<CreationError> validateWorkspace(const GlyphDocumentDefinition &d) {
    const auto wholeUnit = [](double value) {
        return std::isfinite(value) && value >= 16 && value <= 16384 && std::floor(value) == value;
    };
    if (!wholeUnit(d.bounds.width()) || !wholeUnit(d.bounds.height())
        || d.bounds.width() > 8192 || d.bounds.height() > 8192
        || d.bounds.x() != -d.bounds.width() / 2 || d.bounds.y() != -d.bounds.height() / 4)
        return CreationError{CreationField::Dimensions, "Width and height must be whole font units from 16 to 8192."};
    if (!wholeUnit(d.unitsPerEm))
        return CreationError{CreationField::UnitsPerEm, "Units per em must be a whole number from 16 to 16384."};
    // Bounds are stored Y-up: bounds.top() is the lowest Y and bounds.bottom() the highest.
    const double low = d.bounds.top(), high = d.bounds.bottom();
    std::vector<TypographyGuide> above, below;
    QStringList offending;
    bool anyDisabled = false;
    for (const auto &guide : d.guides()) {
        if (!std::isfinite(guide.position))
            return CreationError{CreationField::Guides, QString::fromUtf8(guide.name) + " must be a finite number.", {QString::fromUtf8(guide.name)}};
        if (guide.position >= low && guide.position <= high) continue;
        (guide.position > high ? above : below).push_back(guide);
        offending << QString::fromUtf8(guide.name);
        anyDisabled = anyDisabled || !guide.enabled;
    }
    if (!offending.isEmpty()) {
        // The workspace spans -height/4 to 3*height/4, so a taller workspace can always reach further.
        double neededHeight = d.bounds.height();
        for (const auto &guide : above) neededHeight = std::max(neededHeight, std::ceil(guide.position / .75));
        for (const auto &guide : below) neededHeight = std::max(neededHeight, std::ceil(-guide.position * 4));
        QStringList sentences;
        const auto plural = [](const std::vector<TypographyGuide> &g) { return g.size() > 1; };
        if (!above.empty())
            sentences << listGuides(above) + (plural(above) ? " are" : " is") + " above the workspace top (" + units(high) + ").";
        if (!below.empty())
            sentences << listGuides(below) + (plural(below) ? " are" : " is") + " below the workspace bottom (" + units(low) + ").";
        const QString guideWord = offending.size() > 1 ? "those guides" : offending.front();
        if (neededHeight <= 8192)
            sentences << "Raise Height to at least " + units(neededHeight) + " units, or move " + guideWord + " inside "
                + units(low) + " to " + units(high) + ".";
        else
            sentences << "Move " + guideWord + " inside " + units(low) + " to " + units(high) + ".";
        if (anyDisabled) sentences << "Unchecked guides must fit too, because their values are kept in the project.";
        return CreationError{CreationField::Guides, sentences.join(' '), offending};
    }
    const std::array<std::pair<const char *, double>, 5> ordered{{{"Descender", d.descender}, {"Baseline", d.baseline},
        {"x-height", d.xHeight}, {"Cap height", d.capHeight}, {"Ascender", d.ascender}}};
    for (std::size_t i = 0; i + 1 < ordered.size(); ++i) {
        const auto &[lowerName, lower] = ordered[i];
        const auto &[upperName, upper] = ordered[i + 1];
        if (lower <= upper) continue;
        return CreationError{CreationField::Guides,
            QString("%1 (%2) is above %3 (%4). Guides must stay in order from bottom to top: Descender ≤ Baseline ≤ x-height ≤ Cap height ≤ Ascender.")
                .arg(QString::fromUtf8(lowerName), units(lower), QString::fromUtf8(upperName), units(upper)),
            {QString::fromUtf8(lowerName), QString::fromUtf8(upperName)}};
    }
    const double halfWidth = d.bounds.width() / 2;
    if (!(std::isfinite(d.sideBearing) && d.sideBearing >= 0 && d.sideBearing <= halfWidth))
        return CreationError{CreationField::Guides,
            QString("Side bearing (%1) must be from 0 to %2, half the workspace width, so the left and right guides do not cross.")
                .arg(units(d.sideBearing), units(halfWidth)),
            {"Side bearing"}};
    return std::nullopt;
}
ProjectCreationResult createProject(const ProjectCreationDefinition &request) {
    const auto &d = request.definition;
    if (auto error = validateWorkspace(d)) return *error;
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
