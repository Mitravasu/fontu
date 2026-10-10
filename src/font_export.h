#pragma once
#include "font_description.h"
#include <QByteArray>
#include <QString>
#include <variant>
#include <vector>

namespace fontu {

enum class FontFormat { Otf };

using FontBytesResult = std::variant<QByteArray, QString>;
FontBytesResult fontBytes(const FontDescription &font, FontFormat format);

struct FontExportReport {
    std::size_t glyphCount;
    std::vector<ExportWarning> warnings;
};
using FontExportResult = std::variant<FontExportReport, QString>;

FontExportResult exportFont(const QString &path, const ProjectSnapshot &project,
                            const FontSettings &settings, FontFormat format);

}
