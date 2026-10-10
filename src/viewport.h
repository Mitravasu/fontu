#pragma once
#include <QPointF>
#include <QTransform>
namespace fontu {
class Viewport {
public:
    QPointF toScreen(const QPointF &point) const;
    QPointF toDocument(const QPointF &point) const;
    QTransform transform() const;
    void pan(const QPointF &screenDelta) { offset_ += screenDelta; }
    void zoomAt(const QPointF &screenAnchor, double factor);
    void reset(const QPointF &screenOrigin) { zoom_ = 1; offset_ = screenOrigin; }
    double zoom() const { return zoom_; }
private:
    double zoom_ = 1;
    QPointF offset_;
};
}
