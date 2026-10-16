#include "reference_glyph.h"
#include <QFontMetricsF>
#include <QTransform>

namespace fontu {
namespace {
constexpr int referencePixelSize = 1000;
}
QFont defaultReferenceFont() {
    QFont font;
    font.setFamilies({QStringLiteral("Helvetica"), QStringLiteral("Arial"), QStringLiteral("DejaVu Sans")});
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
