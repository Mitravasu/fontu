#include "font_export.h"
#include <QSaveFile>
#include <QTimeZone>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace fontu {
namespace {
void u8(QByteArray &out, std::uint32_t value) { out.append(char(value & 0xFF)); }
void u16(QByteArray &out, std::uint32_t value) { u8(out, value >> 8); u8(out, value); }
void i16(QByteArray &out, int value) { u16(out, std::uint16_t(std::int16_t(value))); }
void u32(QByteArray &out, std::uint32_t value) { u16(out, value >> 16); u16(out, value); }
void i64(QByteArray &out, std::int64_t value) { u32(out, std::uint32_t(std::uint64_t(value) >> 32)); u32(out, std::uint32_t(value)); }
void longDateTime(QByteArray &out, const QDateTime &time) {
    constexpr std::int64_t secondsFrom1904To1970 = 2082844800;
    i64(out, time.toSecsSinceEpoch() + secondsFrom1904To1970);
}
void putU32(QByteArray &out, int offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out[offset + i] = char((value >> (24 - 8 * i)) & 0xFF);
}
void padTo4(QByteArray &out) { while (out.size() % 4) out.append('\0'); }
std::uint32_t checksum(QByteArray data) {
    padTo4(data);
    std::uint32_t sum = 0;
    for (int i = 0; i < data.size(); i += 4)
        sum += std::uint32_t(std::uint8_t(data[i])) << 24 | std::uint32_t(std::uint8_t(data[i + 1])) << 16
             | std::uint32_t(std::uint8_t(data[i + 2])) << 8 | std::uint32_t(std::uint8_t(data[i + 3]));
    return sum;
}
int roundedFraction(int unitsPerEm, double fraction) { return int(std::lround(unitsPerEm * fraction)); }

QPainterPath outline(const FontGlyph &glyph) {
    QPainterPath path;
    for (const auto &contour : glyph.contours) {
        path.moveTo(contour.start().x, contour.start().y);
        for (const auto &segment : contour.segments()) {
            if (const auto *line = std::get_if<LineSegment>(&segment)) path.lineTo(line->end.x, line->end.y);
            else {
                const auto &cubic = std::get<CubicSegment>(segment);
                path.cubicTo(cubic.c1.x, cubic.c1.y, cubic.c2.x, cubic.c2.y, cubic.end.x, cubic.end.y);
            }
        }
    }
    return path;
}

struct GlyphBox {
    int xMin = 0, yMin = 0, xMax = 0, yMax = 0;
    bool empty = true;
};
struct Measurements {
    std::vector<GlyphBox> boxes;
    GlyphBox fontBox;
    int advanceMax = 0;
    int minLeftBearing = 0, minRightBearing = 0, xMaxExtent = 0;
    int averageAdvance = 0;
    int defaultAdvance = 0;
    bool fixedPitch = true;
    std::uint32_t firstChar = 0, lastChar = 0;
};
Measurements measure(const FontDescription &font) {
    Measurements m;
    std::map<int, int> advanceCounts;
    long long advanceTotal = 0;
    int nonzeroAdvances = 0;
    bool anyInk = false;
    for (GlyphIndex index = 0; index < font.glyphs.size(); ++index) {
        const auto &glyph = font.glyphs.at(index);
        GlyphBox box;
        if (!glyph.contours.empty()) {
            const auto rect = outline(glyph).boundingRect();
            box = {int(std::floor(rect.left())), int(std::floor(rect.top())), int(std::ceil(rect.right())), int(std::ceil(rect.bottom())), false};
            const int rightBearing = glyph.advance - box.xMax;
            if (!anyInk) {
                m.fontBox = box;
                m.minLeftBearing = box.xMin;
                m.minRightBearing = rightBearing;
                m.xMaxExtent = box.xMax;
                anyInk = true;
            }
            m.fontBox = {std::min(m.fontBox.xMin, box.xMin), std::min(m.fontBox.yMin, box.yMin),
                         std::max(m.fontBox.xMax, box.xMax), std::max(m.fontBox.yMax, box.yMax), false};
            m.minLeftBearing = std::min(m.minLeftBearing, box.xMin);
            m.minRightBearing = std::min(m.minRightBearing, rightBearing);
            m.xMaxExtent = std::max(m.xMaxExtent, box.xMax);
        }
        m.boxes.push_back(box);
        m.advanceMax = std::max<int>(m.advanceMax, glyph.advance);
        m.fixedPitch = m.fixedPitch && glyph.advance == font.glyphs.notdef.advance;
        ++advanceCounts[glyph.advance];
        if (glyph.advance) {
            advanceTotal += glyph.advance;
            ++nonzeroAdvances;
        }
    }
    m.averageAdvance = nonzeroAdvances ? int(std::lround(double(advanceTotal) / nonzeroAdvances)) : 0;
    m.defaultAdvance = std::max_element(advanceCounts.begin(), advanceCounts.end(),
                                        [](const auto &a, const auto &b) { return a.second < b.second; })->first;
    if (!font.characterMap.empty()) {
        m.firstChar = std::min<std::uint32_t>(font.characterMap.begin()->first, 0xFFFF);
        m.lastChar = std::min<std::uint32_t>(font.characterMap.rbegin()->first, 0xFFFF);
    }
    return m;
}

struct Run {
    char32_t firstCode;
    char32_t lastCode;
    GlyphIndex firstGlyph;
};
std::vector<Run> runs(const std::map<char32_t, GlyphIndex> &characterMap, char32_t limit) {
    std::vector<Run> result;
    for (const auto &[code, glyph] : characterMap) {
        if (code > limit) break;
        auto *last = result.empty() ? nullptr : &result.back();
        if (last && code == last->lastCode + 1 && glyph == last->firstGlyph + (code - last->firstCode)) last->lastCode = code;
        else result.push_back({code, code, glyph});
    }
    return result;
}
struct Segment {
    char32_t firstCode;
    std::vector<GlyphIndex> glyphs;
    char32_t lastCode() const { return firstCode + char32_t(glyphs.size()) - 1; }
    bool sequential() const {
        for (std::size_t i = 0; i < glyphs.size(); ++i)
            if (glyphs[i] == 0 || glyphs[i] != glyphs[0] + i) return false;
        return true;
    }
};
std::vector<Segment> bmpSegments(const std::map<char32_t, GlyphIndex> &characterMap) {
    constexpr std::size_t segmentHeaderBytes = 8;
    std::vector<Segment> segments;
    for (const auto &run : runs(characterMap, 0xFFFE)) {
        const std::size_t length = run.lastCode - run.firstCode + 1;
        if (!segments.empty()) {
            auto &last = segments.back();
            const std::size_t gap = run.firstCode - last.lastCode() - 1;
            if (2 * (gap + length + (last.sequential() ? last.glyphs.size() : 0)) < segmentHeaderBytes) {
                last.glyphs.insert(last.glyphs.end(), gap, 0);
                for (std::size_t i = 0; i < length; ++i) last.glyphs.push_back(GlyphIndex(run.firstGlyph + i));
                continue;
            }
        }
        Segment segment{run.firstCode, {}};
        for (std::size_t i = 0; i < length; ++i) segment.glyphs.push_back(GlyphIndex(run.firstGlyph + i));
        segments.push_back(std::move(segment));
    }
    segments.push_back({0xFFFF, {0}});
    return segments;
}
std::optional<QByteArray> cmapFormat4(const std::map<char32_t, GlyphIndex> &characterMap) {
    const auto segments = bmpSegments(characterMap);
    const int count = int(segments.size());
    int searchRange = 2, entrySelector = 0;
    while (searchRange * 2 <= count * 2) {
        searchRange *= 2;
        ++entrySelector;
    }
    std::vector<GlyphIndex> glyphIdArray;
    QByteArray deltas, rangeOffsets;
    for (int i = 0; i < count; ++i) {
        const auto &s = segments[std::size_t(i)];
        if (s.firstCode == 0xFFFF) {
            u16(deltas, 1);
            u16(rangeOffsets, 0);
        } else if (s.sequential()) {
            u16(deltas, std::uint16_t(s.glyphs[0] - s.firstCode));
            u16(rangeOffsets, 0);
        } else {
            u16(deltas, 0);
            u16(rangeOffsets, std::uint32_t(2 * (count - i + glyphIdArray.size())));
            glyphIdArray.insert(glyphIdArray.end(), s.glyphs.begin(), s.glyphs.end());
        }
    }
    const auto length = 16 + 8 * std::size_t(count) + 2 * glyphIdArray.size();
    if (length > 0xFFFF) return {};
    QByteArray out;
    u16(out, 4);
    u16(out, std::uint32_t(length));
    u16(out, 0);
    u16(out, 2 * count);
    u16(out, searchRange);
    u16(out, entrySelector);
    u16(out, 2 * count - searchRange);
    for (const auto &s : segments) u16(out, s.lastCode());
    u16(out, 0);
    for (const auto &s : segments) u16(out, s.firstCode);
    out += deltas + rangeOffsets;
    for (const auto glyph : glyphIdArray) u16(out, glyph);
    return out;
}
QByteArray cmapFormat12(const std::map<char32_t, GlyphIndex> &characterMap) {
    const auto groups = runs(characterMap, 0x10FFFF);
    QByteArray out;
    u16(out, 12);
    u16(out, 0);
    u32(out, 16 + 12 * std::uint32_t(groups.size()));
    u32(out, 0);
    u32(out, std::uint32_t(groups.size()));
    for (const auto &g : groups) {
        u32(out, g.firstCode);
        u32(out, g.lastCode);
        u32(out, g.firstGlyph);
    }
    return out;
}

struct TableInput {
    const FontDescription &font;
    const Measurements &m;
    const QByteArray &cmapFormat4;
};

QByteArray cmapTable(const TableInput &in) {
    const auto &map = in.font.characterMap;
    const bool supplementary = !map.empty() && map.rbegin()->first > 0xFFFF;
    const auto &bmp = in.cmapFormat4;
    const auto full = supplementary ? cmapFormat12(map) : QByteArray();
    const std::uint32_t records = supplementary ? 4 : 2;
    const std::uint32_t bmpOffset = 4 + 8 * records, fullOffset = bmpOffset + std::uint32_t(bmp.size());
    QByteArray out;
    u16(out, 0);
    u16(out, records);
    const auto record = [&out](int platform, int encoding, std::uint32_t offset) {
        u16(out, platform);
        u16(out, encoding);
        u32(out, offset);
    };
    record(0, 3, bmpOffset);
    if (supplementary) record(0, 4, fullOffset);
    record(3, 1, bmpOffset);
    if (supplementary) record(3, 10, fullOffset);
    return out + bmp + full;
}

constexpr int headChecksumAdjustmentOffset = 8;
QByteArray headTable(const TableInput &in) {
    QByteArray out;
    u32(out, 0x00010000);
    u32(out, 0x00010000);
    u32(out, 0);
    u32(out, 0x5F0F3CF5);
    u16(out, 0x0003);
    u16(out, in.font.unitsPerEm);
    longDateTime(out, in.font.timestamp);
    longDateTime(out, in.font.timestamp);
    i16(out, in.m.fontBox.xMin);
    i16(out, in.m.fontBox.yMin);
    i16(out, in.m.fontBox.xMax);
    i16(out, in.m.fontBox.yMax);
    u16(out, 0);
    u16(out, 8);
    i16(out, 2);
    i16(out, 0);
    i16(out, 0);
    return out;
}

QByteArray hheaTable(const TableInput &in) {
    const auto &metrics = in.font.lineMetrics;
    QByteArray out;
    u32(out, 0x00010000);
    i16(out, metrics.ascender);
    i16(out, metrics.descender);
    i16(out, 0);
    u16(out, in.m.advanceMax);
    i16(out, in.m.minLeftBearing);
    i16(out, in.m.minRightBearing);
    i16(out, in.m.xMaxExtent);
    i16(out, 1);
    i16(out, 0);
    i16(out, 0);
    for (int i = 0; i < 4; ++i) i16(out, 0);
    i16(out, 0);
    u16(out, std::uint16_t(in.font.glyphs.size()));
    return out;
}

QByteArray hmtxTable(const TableInput &in) {
    QByteArray out;
    for (GlyphIndex index = 0; index < in.font.glyphs.size(); ++index) {
        u16(out, in.font.glyphs.at(index).advance);
        i16(out, in.m.boxes[index].xMin);
    }
    return out;
}

QByteArray maxpCffTable(const TableInput &in) {
    QByteArray out;
    u32(out, 0x00005000);
    u16(out, std::uint16_t(in.font.glyphs.size()));
    return out;
}

QByteArray nameTable(const TableInput &in) {
    const auto &names = in.font.names;
    const std::vector<std::pair<int, QString>> records{
        {1, names.family}, {2, names.style}, {3, "1.000;UKWN;" + names.postScriptName},
        {4, names.family + " " + names.style}, {5, "Version 1.000"}, {6, names.postScriptName}};
    QByteArray storage, out;
    u16(out, 0);
    u16(out, std::uint16_t(records.size()));
    u16(out, std::uint16_t(6 + 12 * records.size()));
    for (const auto &[id, text] : records) {
        QByteArray encoded;
        for (const auto unit : text) u16(encoded, unit.unicode());
        u16(out, 3);
        u16(out, 1);
        u16(out, 0x0409);
        u16(out, id);
        u16(out, std::uint16_t(encoded.size()));
        u16(out, std::uint16_t(storage.size()));
        storage += encoded;
    }
    return out + storage;
}

QByteArray os2Table(const TableInput &in) {
    const auto &metrics = in.font.lineMetrics;
    const int upm = in.font.unitsPerEm;
    QByteArray out;
    u16(out, 4);
    i16(out, in.m.averageAdvance);
    u16(out, 400);
    u16(out, 5);
    u16(out, 0);
    for (const double yOffset : {0.075, 0.35}) {
        i16(out, roundedFraction(upm, 0.65));
        i16(out, roundedFraction(upm, 0.6));
        i16(out, 0);
        i16(out, roundedFraction(upm, yOffset));
    }
    i16(out, roundedFraction(upm, 0.05));
    i16(out, roundedFraction(upm, 0.25));
    i16(out, 0);
    out.append(10, '\0');
    for (int i = 0; i < 4; ++i) u32(out, 0);
    out.append("UKWN", 4);
    u16(out, 0x00C0);
    u16(out, in.m.firstChar);
    u16(out, in.m.lastChar);
    i16(out, metrics.ascender);
    i16(out, metrics.descender);
    i16(out, 0);
    u16(out, std::max({0, in.m.fontBox.yMax, int(metrics.ascender)}));
    u16(out, std::max({0, -in.m.fontBox.yMin, -int(metrics.descender)}));
    u32(out, 0);
    u32(out, 0);
    i16(out, metrics.xHeight);
    i16(out, metrics.capHeight);
    u16(out, 0);
    u16(out, 32);
    u16(out, 0);
    return out;
}

QByteArray postTable(const TableInput &in) {
    QByteArray out;
    u32(out, 0x00030000);
    u32(out, 0);
    i16(out, -roundedFraction(in.font.unitsPerEm, 0.075));
    i16(out, roundedFraction(in.font.unitsPerEm, 0.05));
    u32(out, in.m.fixedPitch ? 1 : 0);
    for (int i = 0; i < 4; ++i) u32(out, 0);
    return out;
}

QByteArray cffIndex(const std::vector<QByteArray> &items) {
    QByteArray out;
    u16(out, std::uint16_t(items.size()));
    if (items.empty()) return out;
    std::uint32_t end = 1;
    for (const auto &item : items) end += std::uint32_t(item.size());
    const int offSize = end <= 0xFF ? 1 : end <= 0xFFFF ? 2 : end <= 0xFFFFFF ? 3 : 4;
    u8(out, offSize);
    std::uint32_t offset = 1;
    const auto writeOffset = [&out, offSize](std::uint32_t value) {
        for (int shift = 8 * (offSize - 1); shift >= 0; shift -= 8) u8(out, value >> shift);
    };
    writeOffset(offset);
    for (const auto &item : items) writeOffset(offset += std::uint32_t(item.size()));
    for (const auto &item : items) out += item;
    return out;
}
void dictInteger(QByteArray &out, int value) {
    if (value >= -107 && value <= 107) u8(out, value + 139);
    else if (value >= 108 && value <= 1131) {
        u8(out, (value - 108) / 256 + 247);
        u8(out, (value - 108) % 256);
    } else if (value >= -1131 && value <= -108) {
        u8(out, (-value - 108) / 256 + 251);
        u8(out, (-value - 108) % 256);
    } else if (value >= -32768 && value <= 32767) {
        u8(out, 28);
        i16(out, value);
    } else {
        u8(out, 29);
        u32(out, std::uint32_t(value));
    }
}
void dictFixedInteger(QByteArray &out, std::uint32_t value) {
    u8(out, 29);
    u32(out, value);
}
void dictReal(QByteArray &out, double value) {
    QString text = QString::number(value, 'g', 10).toLower();
    std::vector<int> nibbles;
    for (int i = 0; i < text.size(); ++i) {
        const auto c = text[i];
        if (c.isDigit()) nibbles.push_back(c.digitValue());
        else if (c == '.') nibbles.push_back(0xA);
        else if (c == '-') nibbles.push_back(0xE);
        else if (c == 'e' && i + 1 < text.size() && text[i + 1] == '-') {
            nibbles.push_back(0xC);
            ++i;
        } else if (c == 'e') {
            nibbles.push_back(0xB);
            if (i + 1 < text.size() && text[i + 1] == '+') ++i;
        }
    }
    nibbles.push_back(0xF);
    if (nibbles.size() % 2) nibbles.push_back(0xF);
    u8(out, 30);
    for (std::size_t i = 0; i < nibbles.size(); i += 2) u8(out, nibbles[i] << 4 | nibbles[i + 1]);
}
void charstringInteger(QByteArray &out, int value) {
    if (value >= -1131 && value <= 1131) dictInteger(out, value);
    else {
        u8(out, 28);
        i16(out, value);
    }
}

QByteArray charstring(const FontGlyph &glyph, int defaultWidth, int nominalWidth) {
    enum Operator : std::uint8_t { rlineto = 5, rrcurveto = 8, endchar = 14, rmoveto = 21 };
    QByteArray out;
    std::vector<int> stack;
    if (glyph.advance != defaultWidth) stack.push_back(glyph.advance - nominalWidth);
    int pending = 0, pendingCount = 0;
    const auto flush = [&](std::uint8_t op) {
        for (const auto value : stack) charstringInteger(out, value);
        u8(out, op);
        stack.clear();
        pending = 0;
        pendingCount = 0;
    };
    const auto queue = [&](Operator op, std::initializer_list<int> operands, int maximum) {
        if (pending && pending != op) flush(std::uint8_t(pending));
        stack.insert(stack.end(), operands);
        pending = op;
        if (++pendingCount == maximum) flush(op);
    };
    QPoint current;
    for (const auto &contour : glyph.contours) {
        const auto start = contour.start();
        stack.push_back(start.x - current.x());
        stack.push_back(start.y - current.y());
        flush(rmoveto);
        current = {start.x, start.y};
        auto segments = contour.segments();
        if (std::holds_alternative<LineSegment>(segments.back())) segments.pop_back();
        for (const auto &segment : segments) {
            if (const auto *line = std::get_if<LineSegment>(&segment)) {
                queue(rlineto, {line->end.x - current.x(), line->end.y - current.y()}, 24);
                current = {line->end.x, line->end.y};
            } else {
                const auto &c = std::get<CubicSegment>(segment);
                queue(rrcurveto, {c.c1.x - current.x(), c.c1.y - current.y(), c.c2.x - c.c1.x, c.c2.y - c.c1.y,
                                  c.end.x - c.c2.x, c.end.y - c.c2.y}, 8);
                current = {c.end.x, c.end.y};
            }
        }
        if (pending) flush(std::uint8_t(pending));
    }
    flush(endchar);
    return out;
}

QByteArray cffTable(const TableInput &in) {
    const auto &font = in.font;
    const auto &box = in.m.fontBox;
    std::vector<QByteArray> strings{(font.names.family + " " + font.names.style).toUtf8(), font.names.family.toUtf8()};
    QByteArray charset;
    u8(charset, 0);
    std::vector<QByteArray> charstrings;
    for (GlyphIndex index = 0; index < font.glyphs.size(); ++index) {
        const auto &glyph = font.glyphs.at(index);
        charstrings.push_back(charstring(glyph, in.m.defaultAdvance, in.m.defaultAdvance));
        if (index == 0) continue;
        constexpr int firstCustomStringId = 391;
        u16(charset, std::uint16_t(firstCustomStringId + strings.size()));
        strings.push_back(glyph.name.toLatin1());
    }
    QByteArray privateDict;
    dictFixedInteger(privateDict, std::uint32_t(in.m.defaultAdvance));
    u8(privateDict, 20);
    dictFixedInteger(privateDict, std::uint32_t(in.m.defaultAdvance));
    u8(privateDict, 21);

    const auto topDict = [&](std::uint32_t charsetOffset, std::uint32_t charStringsOffset, std::uint32_t privateOffset) {
        QByteArray out;
        dictInteger(out, 391);
        u8(out, 2);
        dictInteger(out, 392);
        u8(out, 3);
        for (const int value : {box.xMin, box.yMin, box.xMax, box.yMax}) dictInteger(out, value);
        u8(out, 5);
        if (font.unitsPerEm != 1000) {
            const double scale = 1.0 / font.unitsPerEm;
            for (const double value : {scale, 0.0, 0.0, scale, 0.0, 0.0}) {
                if (value == 0) dictInteger(out, 0);
                else dictReal(out, value);
            }
            u8(out, 12);
            u8(out, 7);
        }
        dictFixedInteger(out, charsetOffset);
        u8(out, 15);
        dictFixedInteger(out, charStringsOffset);
        u8(out, 17);
        dictFixedInteger(out, std::uint32_t(privateDict.size()));
        dictFixedInteger(out, privateOffset);
        u8(out, 18);
        return out;
    };
    const QByteArray header = QByteArray::fromHex("01000404");
    const auto nameIndex = cffIndex({font.names.postScriptName.toLatin1()});
    const auto stringIndex = cffIndex(strings);
    const auto globalSubrIndex = cffIndex({});
    const auto charStringsIndex = cffIndex(charstrings);
    const auto topDictIndexSize = cffIndex({topDict(0, 0, 0)}).size();
    const auto charsetOffset = std::uint32_t(header.size() + nameIndex.size() + topDictIndexSize + stringIndex.size() + globalSubrIndex.size());
    const auto charStringsOffset = charsetOffset + std::uint32_t(charset.size());
    const auto privateOffset = charStringsOffset + std::uint32_t(charStringsIndex.size());
    return header + nameIndex + cffIndex({topDict(charsetOffset, charStringsOffset, privateOffset)}) + stringIndex
         + globalSubrIndex + charset + charStringsIndex + privateDict;
}

using Tag = std::array<char, 4>;
using TableBuilder = QByteArray (*)(const TableInput &);
struct TableEntry {
    Tag tag;
    TableBuilder build;
};
struct Flavor {
    std::uint32_t sfntVersion;
    std::vector<TableEntry> tables;
};
const Flavor &flavor(FontFormat format) {
    static const Flavor otf{0x4F54544F, {
        {{'C', 'F', 'F', ' '}, cffTable}, {{'O', 'S', '/', '2'}, os2Table}, {{'c', 'm', 'a', 'p'}, cmapTable},
        {{'h', 'e', 'a', 'd'}, headTable}, {{'h', 'h', 'e', 'a'}, hheaTable}, {{'h', 'm', 't', 'x'}, hmtxTable},
        {{'m', 'a', 'x', 'p'}, maxpCffTable}, {{'n', 'a', 'm', 'e'}, nameTable}, {{'p', 'o', 's', 't'}, postTable}}};
    switch (format) {
    case FontFormat::Otf: return otf;
    }
    return otf;
}

QByteArray assembleSfnt(std::uint32_t sfntVersion, std::vector<std::pair<Tag, QByteArray>> tables) {
    std::sort(tables.begin(), tables.end(), [](const auto &a, const auto &b) {
        return std::memcmp(a.first.data(), b.first.data(), 4) < 0;
    });
    const int count = int(tables.size());
    int entrySelector = 0;
    while ((2 << entrySelector) <= count) ++entrySelector;
    const int searchRange = 16 << entrySelector;
    QByteArray out;
    u32(out, sfntVersion);
    u16(out, count);
    u16(out, searchRange);
    u16(out, entrySelector);
    u16(out, count * 16 - searchRange);
    std::uint32_t offset = 12 + 16 * std::uint32_t(count);
    int headOffset = -1;
    for (const auto &[tag, data] : tables) {
        out.append(tag.data(), 4);
        u32(out, checksum(data));
        u32(out, offset);
        u32(out, std::uint32_t(data.size()));
        if (std::memcmp(tag.data(), "head", 4) == 0) headOffset = int(offset);
        offset += (std::uint32_t(data.size()) + 3) & ~3u;
    }
    for (const auto &table : tables) {
        out += table.second;
        padTo4(out);
    }
    if (headOffset >= 0) putU32(out, headOffset + headChecksumAdjustmentOffset, 0xB1B0AFBA - checksum(out));
    return out;
}
}

FontBytesResult fontBytes(const FontDescription &font, FontFormat format) {
    const auto bmp = cmapFormat4(font.characterMap);
    if (!bmp)
        return QString("The glyph labels below U+10000 are too scattered to fit an OpenType character map. "
                       "Remove some labels or group them into consecutive characters.");
    const auto measurements = measure(font);
    const TableInput input{font, measurements, *bmp};
    const auto &f = flavor(format);
    std::vector<std::pair<Tag, QByteArray>> tables;
    for (const auto &entry : f.tables) tables.emplace_back(entry.tag, entry.build(input));
    return assembleSfnt(f.sfntVersion, std::move(tables));
}

FontExportResult exportFont(const QString &path, const ProjectSnapshot &project, const FontSettings &settings, FontFormat format) {
    auto described = describeFont(project, settings);
    if (const auto *error = std::get_if<QString>(&described)) return *error;
    auto &result = std::get<DescribedFont>(described);
    const auto encoded = fontBytes(result.font, format);
    if (const auto *error = std::get_if<QString>(&encoded)) return *error;
    const auto &bytes = std::get<QByteArray>(encoded);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return file.errorString();
    if (file.write(bytes) != bytes.size()) return file.errorString();
    if (!file.commit()) return file.errorString();
    return FontExportReport{result.font.glyphs.size(), std::move(result.warnings)};
}

}
