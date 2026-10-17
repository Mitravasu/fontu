#include "guide_preview.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <algorithm>
#include <array>
#include <cmath>

namespace fontu {
namespace {
const QColor invalidColor("#e0352b");
constexpr double hitTolerance = 6;
// Bottom-to-top order the guides must keep.
std::array<std::pair<QString, double>, 5> ordered(const GlyphDocumentDefinition &d) {
    return {{{"Descender", d.descender}, {"Baseline", d.baseline}, {"x-height", d.xHeight},
             {"Cap height", d.capHeight}, {"Ascender", d.ascender}}};
}
}
QColor guideColor(const QString &name) {
    if (name == "Ascender") return QColor("#9b5de5");
    if (name == "Cap height") return QColor("#2f80ed");
    if (name == "x-height") return QColor("#17a673");
    if (name == "Baseline") return QColor("#e08a00");
    if (name == "Descender") return QColor("#d0479a");
    if (name == "Side bearing") return QColor("#0891b2");
    return QColor("#8492a6");
}
QPixmap guideSwatch(const QString &name, qreal devicePixelRatio) {
    QPixmap pixmap(QSize(18, 12) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(guideColor(name), name == "Side bearing" ? 2 : 3, Qt::SolidLine, Qt::RoundCap));
    if (name == "Side bearing") {
        painter.drawLine(QPointF(4, 1), QPointF(4, 11));
        painter.drawLine(QPointF(14, 1), QPointF(14, 11));
    } else {
        painter.drawLine(QPointF(2, 6), QPointF(16, 6));
    }
    return pixmap;
}
GuidePreview::GuidePreview(QWidget *parent) : QWidget(parent) {
    setObjectName("guidePreview");
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setToolTip("Drag a guide line or its label up or down to set its value. Positive Y points upward. Drag a side-bearing line left or right.");
}
void GuidePreview::setDefinition(const GlyphDocumentDefinition &definition, const QStringList &invalidGuides) {
    definition_ = definition;
    invalid_ = invalidGuides;
    update();
}
GuidePreview::Frame GuidePreview::frame() const {
    Frame f;
    const auto &d = definition_;
    const QFontMetricsF metrics(font());
    // Room for the widest label, but never more than about half the widget: wide UI fonts (large
    // text settings, font substitution) must shrink the diagram, not make it disappear.
    const double labelWidth = std::min(metrics.horizontalAdvance("Cap height  -16384.5 ▲") + 14, width() * .45);
    const QRectF area = QRectF(rect()).adjusted(8, metrics.height() / 2 + 6, -labelWidth, -metrics.height() / 2 - 6);
    if (area.width() < 20 || area.height() < 20 || d.bounds.width() <= 0 || d.bounds.height() <= 0) return f;
    f.scale = std::min(area.width() / d.bounds.width(), area.height() / d.bounds.height());
    const QSizeF size(d.bounds.width() * f.scale, d.bounds.height() * f.scale);
    f.page = QRectF(area.right() - size.width(), area.center().y() - size.height() / 2, size.width(), size.height());
    f.low = d.bounds.top(); f.high = d.bounds.bottom(); // Y-up: top() is the lowest Y.
    f.valid = true;
    return f;
}
double GuidePreview::toY(const Frame &f, double value) const {
    return f.page.bottom() - (std::clamp(value, f.low, f.high) - f.low) * f.scale;
}
double GuidePreview::toX(const Frame &f, double value) const {
    return f.page.left() + (value - definition_.bounds.left()) * f.scale;
}
QPointF GuidePreview::sideBearingPoint() const {
    const auto f = frame();
    return {toX(f, definition_.sideBearingGuides()[0]), f.page.center().y()};
}
double GuidePreview::draggedSideBearing(double x) const {
    const auto f = frame();
    if (!f.valid) return 0;
    const auto &b = definition_.bounds;
    const double docX = b.left() + (x - f.page.left()) / f.scale;
    // Measured from the nearer edge, so either line drags both symmetrically.
    return std::clamp(std::round(std::min(docX - b.left(), b.right() - docX)), 0.0, std::floor(b.width() / 2));
}
bool GuidePreview::sideBearingAt(const QPointF &point) const {
    const auto f = frame();
    if (!f.valid || point.y() < f.page.top() || point.y() > f.page.bottom()) return false;
    for (const double x : definition_.sideBearingGuides())
        if (std::abs(point.x() - toX(f, x)) <= hitTolerance) return true;
    return false;
}
QPointF GuidePreview::guidePoint(const QString &name) const {
    const auto f = frame();
    for (const auto &[guide, value] : ordered(definition_))
        if (guide == name) return {f.page.center().x(), toY(f, value)};
    return {};
}
double GuidePreview::draggedValue(const QString &name, double y) const {
    const auto f = frame();
    if (!f.valid) return 0;
    const auto guides = ordered(definition_);
    for (std::size_t i = 0; i < guides.size(); ++i) {
        if (guides[i].first != name) continue;
        const double lower = i == 0 ? f.low : std::max(f.low, guides[i - 1].second);
        const double upper = i + 1 == guides.size() ? f.high : std::min(f.high, guides[i + 1].second);
        const double raw = f.low + (f.page.bottom() - y) / f.scale;
        return std::clamp(std::round(raw), lower, upper);
    }
    return 0;
}
QStringList GuidePreview::guidesAt(const QPointF &point) const {
    const auto f = frame();
    QStringList result;
    if (!f.valid || point.x() < f.page.left() - hitTolerance) return result;
    const bool onLabels = point.x() > f.page.right();
    for (const auto &[name, value] : ordered(definition_)) {
        double y = toY(f, value);
        if (onLabels)
            for (const auto &[label, labelY] : labelYs_) if (label == name) y = labelY;
        if (std::abs(point.y() - y) <= hitTolerance) result << name;
    }
    return result; // Bottom-to-top order.
}
void GuidePreview::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return QWidget::mousePressEvent(event);
    candidates_ = guidesAt(event->position());
    if (candidates_.isEmpty() && sideBearingAt(event->position())) candidates_ = {"Side bearing"};
    pressY_ = event->position().y();
    dragging_ = candidates_.size() == 1 ? candidates_.front() : QString();
    if (!candidates_.isEmpty()) { setCursor(Qt::ClosedHandCursor); event->accept(); update(); }
}
void GuidePreview::mouseMoveEvent(QMouseEvent *event) {
    const double y = event->position().y();
    if (!(event->buttons() & Qt::LeftButton) || candidates_.isEmpty()) {
        const auto hits = guidesAt(event->position());
        const bool side = hits.isEmpty() && sideBearingAt(event->position());
        const QString hover = side ? QString("Side bearing") : hits.isEmpty() ? QString() : hits.front();
        if (hover != hovered_) { hovered_ = hover; update(); }
        if (side) setCursor(Qt::SizeHorCursor); else if (hits.isEmpty()) unsetCursor(); else setCursor(Qt::SizeVerCursor);
        return;
    }
    if (dragging_ == "Side bearing") {
        const double value = draggedSideBearing(event->position().x());
        if (value != definition_.sideBearing) emit guideDragged(dragging_, value);
        return;
    }
    if (dragging_.isEmpty()) {
        // Coincident guides: moving up takes the upper one, moving down the lower one.
        if (std::abs(y - pressY_) < 1) return;
        dragging_ = y < pressY_ ? candidates_.back() : candidates_.front();
    }
    const double value = draggedValue(dragging_, y);
    for (const auto &[name, current] : ordered(definition_))
        if (name == dragging_ && current != value) emit guideDragged(dragging_, value);
}
void GuidePreview::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        setCursor(dragging_ == "Side bearing" ? Qt::SizeHorCursor : Qt::SizeVerCursor);
        candidates_.clear(); dragging_.clear(); update();
    }
    QWidget::mouseReleaseEvent(event);
}
void GuidePreview::leaveEvent(QEvent *event) {
    if (!hovered_.isEmpty()) { hovered_.clear(); update(); }
    QWidget::leaveEvent(event);
}
void GuidePreview::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto &pal = palette();
    const auto &d = definition_;
    const QFontMetricsF metrics(font());
    const auto f = frame();
    if (!f.valid) return;
    const auto &page = f.page;
    painter.fillRect(page, pal.color(QPalette::Base));
    painter.setPen(QPen(pal.color(QPalette::Mid), 1));
    painter.drawRect(page);

    // Sample "Hxgd": H reaches cap height, x the x-height, g the descender, d the ascender.
    const double em = std::max(1.0, d.capHeight - d.baseline);
    const double fit = std::min(1.0, d.bounds.width() * .85 / (em * (.42 * 4 + .14 * 3)));
    const double gw = em * .42 * fit, gap = em * .14 * fit;
    const double xh = d.xHeight, base = d.baseline, desc = d.descender;
    const double bowl = std::max(0.0, xh - base);
    QPainterPath sample;
    double x = -(gw * 4 + gap * 3) / 2;
    sample.moveTo(x, base); sample.lineTo(x, d.capHeight);
    sample.moveTo(x + gw, base); sample.lineTo(x + gw, d.capHeight);
    sample.moveTo(x, (base + d.capHeight) / 2); sample.lineTo(x + gw, (base + d.capHeight) / 2);
    x += gw + gap;
    sample.moveTo(x, base); sample.lineTo(x + gw, xh);
    sample.moveTo(x, xh); sample.lineTo(x + gw, base);
    x += gw + gap;
    if (bowl > 0) sample.addEllipse(QRectF(x, base, gw, bowl));
    const double depth = std::max(0.0, base - desc);
    sample.moveTo(x + gw, xh); sample.lineTo(x + gw, desc + depth * .4);
    sample.cubicTo(x + gw, desc, x, desc, x + gw * .05, desc + depth * .3);
    x += gw + gap;
    if (bowl > 0) sample.addEllipse(QRectF(x, base, gw, bowl));
    sample.moveTo(x + gw, base); sample.lineTo(x + gw, d.ascender);
    QTransform map;
    map.translate(page.left() - d.bounds.left() * f.scale, page.bottom() + f.low * f.scale);
    map.scale(f.scale, -f.scale);
    painter.save();
    painter.setClipRect(page);
    QColor ink = pal.color(QPalette::Text); ink.setAlphaF(.4);
    const double stroke = std::clamp(em * .07 * f.scale, 1.2, 3.0);
    painter.setPen(QPen(ink, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(map.map(sample));
    painter.restore();

    {
        const QString name("Side bearing");
        const bool invalid = invalid_.contains(name);
        const bool active = name == dragging_ || (dragging_.isEmpty() && name == hovered_);
        QColor color = invalid ? invalidColor : guideColor(name);
        if (!d.guideEnabled.sideBearing && !invalid) color.setAlphaF(.5);
        painter.setPen(QPen(color, active ? 3 : invalid ? 2 : 1.6, d.guideEnabled.sideBearing ? Qt::SolidLine : Qt::DashLine, Qt::FlatCap));
        for (const double x : d.sideBearingGuides()) {
            const double sx = std::clamp(toX(f, x), page.left(), page.right());
            painter.drawLine(QPointF(sx, page.top()), QPointF(sx, page.bottom()));
        }
    }
    struct Label { QString name, text; double lineY, y; QColor color; bool invalid; };
    std::vector<Label> labels;
    for (const auto &guide : d.guides()) {
        const QString name = QString::fromUtf8(guide.name);
        const bool invalid = invalid_.contains(name);
        const bool outside = !(guide.position >= f.low && guide.position <= f.high);
        const bool active = name == dragging_ || (dragging_.isEmpty() && name == hovered_);
        const double y = toY(f, guide.position);
        QColor color = invalid ? invalidColor : guideColor(name);
        if (!guide.enabled && !invalid) color.setAlphaF(.5);
        painter.setPen(QPen(color, active ? 3 : invalid ? 2 : 1.6, guide.enabled ? Qt::SolidLine : Qt::DashLine, Qt::FlatCap));
        painter.drawLine(QPointF(page.left(), y), QPointF(page.right(), y));
        if (outside) {
            const double dir = guide.position > f.high ? -1 : 1;
            QPainterPath arrow;
            const double ax = page.left() + 6;
            arrow.moveTo(ax - 5, y); arrow.lineTo(ax + 5, y); arrow.lineTo(ax, y + dir * 7); arrow.closeSubpath();
            painter.fillPath(arrow, invalidColor);
        }
        QString text = name + "  " + QString::number(guide.position, 'g', 12);
        if (outside) text += guide.position > f.high ? " ▲" : " ▼";
        if (!guide.enabled) text += " (off)";
        labels.push_back({name, text, y, y, color, invalid});
    }
    // Spread labels vertically so close guides stay readable.
    std::sort(labels.begin(), labels.end(), [](const Label &a, const Label &b) { return a.lineY < b.lineY; });
    const double step = metrics.height();
    const double top = metrics.height() / 2 + 1, bottom = height() - metrics.height() / 2 - 1;
    for (std::size_t i = 0; i < labels.size(); ++i) labels[i].y = std::max(labels[i].y, i ? labels[i - 1].y + step : top);
    if (!labels.empty() && labels.back().y > bottom) {
        labels.back().y = bottom;
        for (std::size_t i = labels.size() - 1; i-- > 0;) labels[i].y = std::min(labels[i].y, labels[i + 1].y - step);
    }
    labelYs_.clear();
    const double textX = page.right() + 12;
    for (const auto &label : labels) {
        labelYs_.push_back({label.name, label.y});
        painter.setPen(QPen(label.color, 1));
        painter.drawLine(QPointF(page.right(), label.lineY), QPointF(textX - 3, label.y));
        QColor textColor = label.invalid ? invalidColor
            : label.color.alphaF() < 1 ? pal.color(QPalette::PlaceholderText) : pal.color(QPalette::Text);
        painter.setPen(textColor);
        auto font = painter.font(); font.setBold(label.name == dragging_); painter.setFont(font);
        const auto text = QFontMetricsF(font).elidedText(label.text, Qt::ElideRight, std::max(0.0, width() - textX - 2));
        painter.drawText(QPointF(textX, label.y + metrics.ascent() / 2 - 1), text);
    }
}
}
