#include "reference_glyph.h"
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QTransform>

namespace fontu {
namespace {
constexpr int referencePixelSize = 1000;
// Registers the bundled Noto Sans subset once and returns its family, or an empty string if it
// cannot be loaded. Bundling keeps the overlay identical everywhere and independent of system fonts,
// which minimal Linux installs and Qt's offscreen platform on Windows may not provide.
QString bundledFamily() {
    static const QString family = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/NotoSans-Regular-subset.ttf"));
        const auto families = id < 0 ? QStringList() : QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QString() : families.front();
    }();
    return family;
}
}
QFont defaultReferenceFont() {
    QStringList families{QStringLiteral("Helvetica"), QStringLiteral("Arial"), QStringLiteral("DejaVu Sans")};
    // System fonts remain fallbacks for characters outside the bundled subset.
    if (const auto bundled = bundledFamily(); !bundled.isEmpty()) families.prepend(bundled);
    QFont font;
    font.setFamilies(families);
    font.setStyleHint(QFont::SansSerif);
    return font;
}
QPainterPath referenceGlyphPath(const QString &label, const GlyphDocumentDefinition &definition, const QFont &font) {
    if (label.toUcs4().size() != 1) return {};
    QFont sized(font);
    sized.setPixelSize(referencePixelSize);
    sized.setHintingPreference(QFont::PreferNoHinting);
    QPainterPath outline;
    outline.addText(QPointF(0, 0), sized, label);
    if (outline.isEmpty()) return {};
    // Matching cap heights lines the reference up with the guides; em scaling is the fallback.
    const QFontMetricsF metrics(sized);
    const double capHeight = definition.capHeight - definition.baseline;
    const double scale = metrics.capHeight() > 0 && capHeight > 0 ? capHeight / metrics.capHeight()
                                                                  : definition.unitsPerEm / referencePixelSize;
    const double left = definition.bounds.center().x() - metrics.horizontalAdvance(label) * scale / 2;
    return QTransform(scale, 0, 0, -scale, left, definition.baseline).map(outline);
}
}
