#pragma once
#include "document.h"
#include <QColor>
#include <QStringList>
#include <QWidget>
#include <vector>

namespace fontu {
// Distinct colour per typography guide (keyed by TypographyGuide::name), chosen to read on light and dark backgrounds.
QColor guideColor(const QString &name);
QPixmap guideSwatch(const QString &name, qreal devicePixelRatio);

// Diagram of a workspace and its guides with a sample "Hxgd" run, Y pointing up. Guide lines
// (or their labels) can be dragged vertically; drags stay inside the workspace and between the
// neighbouring guides, so they can never produce an out-of-range or out-of-order value. Either
// side-bearing line drags horizontally, between the workspace edge and its centre.
class GuidePreview : public QWidget {
    Q_OBJECT
public:
    explicit GuidePreview(QWidget *parent = nullptr);
    void setDefinition(const GlyphDocumentDefinition &definition, const QStringList &invalidGuides);
    const GlyphDocumentDefinition &definition() const { return definition_; }
    const QStringList &invalidGuides() const { return invalid_; }
    // Widget position on the named guide's line (centre of the workspace), for tests and tooling.
    QPointF guidePoint(const QString &name) const;
    // The value a drag of the named guide to widget y would produce.
    double draggedValue(const QString &name, double y) const;
    // Widget position on the left side-bearing guide (centre of the workspace height).
    QPointF sideBearingPoint() const;
    // The side bearing a drag of either side-bearing guide to widget x would produce.
    double draggedSideBearing(double x) const;
    QSize sizeHint() const override { return {320, 200}; }
    QSize minimumSizeHint() const override { return {240, 160}; }
signals:
    void guideDragged(const QString &name, double value);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
private:
    struct Frame { QRectF page; double scale = 0, low = 0, high = 0; bool valid = false; };
    Frame frame() const;
    double toY(const Frame &f, double value) const;
    double toX(const Frame &f, double value) const;
    QStringList guidesAt(const QPointF &point) const;
    bool sideBearingAt(const QPointF &point) const;
    GlyphDocumentDefinition definition_;
    QStringList invalid_;
    QStringList candidates_;
    QString dragging_, hovered_;
    double pressY_ = 0;
    std::vector<std::pair<QString, double>> labelYs_;
};
}
