#pragma once
#include "document.h"
#include <QFont>
#include <QString>

namespace fontu {
QFont defaultReferenceFont();
// Outline of a single-character label in document space: baseline on the baseline guide, cap height
// on the cap-height guide, advance centered in the workspace. Empty for multi-character labels.
QPainterPath referenceGlyphPath(const QString &label, const GlyphDocumentDefinition &definition, const QFont &font);
}
