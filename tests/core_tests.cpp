#include "document.h"
#include "shape_builder.h"
#include "viewport.h"
#include "snapping.h"
#include <QtTest>
#include <algorithm>
#include <limits>
#include <cmath>
using namespace fontu;
class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void builderOverlapExtractSubtractAndAtomicAdmission() {
        Document document;
        const auto a = *document.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        const auto b = *document.addPath(makeShape(ShapeKind::Rectangle, {50, 0, 100, 100}));
        document.selectAll();
        ShapeBuilder builder(document);
        QCOMPARE(builder.regions().size(), std::size_t(3));
        const auto overlap = builder.hit({75, 50});
        QVERIFY(overlap);
        QVERIFY(builder.apply(document, {*overlap}, BuilderOperation::Combine));
        QCOMPARE(document.shapes().size(), std::size_t(3));
        QCOMPARE(document.find(*document.selection())->path.boundingRect(), QRectF(50, 0, 50, 100));
        QVERIFY(document.shapes()[0].path.contains({25, 50}));
        QVERIFY(document.shapes()[1].path.contains({125, 50}));
        QVERIFY(!document.shapes()[0].path.contains({75, 50}));
        QVERIFY(!document.shapes()[1].path.contains({75, 50}));
        const auto saved = document.shapes();
        QVERIFY(!document.replaceShapes({document.shapes()[0].id}, {makeShape(ShapeKind::Rectangle, {490, 0, 30, 30})}));
        QCOMPARE(document.shapes()[0].path, saved[0].path);
        Document subtraction;
        subtraction.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        subtraction.addPath(makeShape(ShapeKind::Rectangle, {50, 0, 100, 100}));
        subtraction.selectAll();
        ShapeBuilder remove(subtraction);
        QVERIFY(remove.apply(subtraction, {*remove.hit({75, 50})}, BuilderOperation::Subtract));
        QCOMPARE(subtraction.shapes().size(), std::size_t(2));
        for (const auto &shape : subtraction.shapes()) QVERIFY(!shape.path.contains({75, 50}));
        QVERIFY(!subtraction.find(a));
        QVERIFY(!subtraction.find(b));
    }
    void builderDisconnectedHolesWindingAndSweep() {
        Document split;
        split.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        split.addPath(makeShape(ShapeKind::Rectangle, {40, -10, 20, 120}));
        split.selectAll();
        ShapeBuilder builder(split);
        QCOMPARE(builder.regions().size(), std::size_t(5));
        QVERIFY(builder.hit({10, 50}) != builder.hit({90, 50}));
        QCOMPARE(builder.crossed({10, 50}, {90, 50}).size(), std::size_t(3));
        Document joined;
        joined.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        joined.addPath(makeShape(ShapeKind::Rectangle, {50, 0, 100, 100}));
        joined.selectAll();
        ShapeBuilder combine(joined);
        QVERIFY(combine.apply(joined, combine.crossed({25, 50}, {125, 50}), BuilderOperation::Combine));
        QCOMPARE(joined.shapes().size(), std::size_t(1));
        ShapeBuilder unified(joined);
        QCOMPARE(unified.regions().size(), std::size_t(1));
        QCOMPARE(unified.hit({25, 50}), unified.hit({125, 50}));

        QCOMPARE(builder.crossed({-10, 0}, {110, 0}).size(), std::size_t(0));
        QPainterPath ring;
        ring.addRect({0, 0, 100, 100});
        ring.addRect({25, 25, 50, 50});
        Document holes;
        holes.addPath(ring);
        holes.selectAll();
        ShapeBuilder donut(holes);
        QCOMPARE(donut.regions().size(), std::size_t(1));
        QVERIFY(donut.hit({10, 50}));
        QVERIFY(!donut.hit({50, 50}));
        QVERIFY(donut.crossed({30, 50}, {70, 50}).empty());
        ring.setFillRule(Qt::WindingFill);
        Document winding;
        winding.addPath(ring);
        winding.selectAll();
        ShapeBuilder filled(winding);
        QCOMPARE(filled.regions().size(), std::size_t(1));
        QVERIFY(filled.hit({50, 50}));
        Document duplicate;
        duplicate.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        duplicate.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        duplicate.selectAll();
        ShapeBuilder coincident(duplicate);
        QCOMPARE(coincident.regions().size(), std::size_t(1));
        QCOMPARE(coincident.regions()[0].sources.size(), std::size_t(2));
    }
    void builderCurvedCapsuleMergeHasNoDisconnectedSeam_data() {
        QTest::addColumn<double>("dx");
        QTest::addColumn<double>("dy");
        QTest::newRow("integer") << 0.0 << 0.0;
        QTest::newRow("fractional") << 1.48 << 0.76;
    }
    void builderCurvedCapsuleMergeHasNoDisconnectedSeam() {
        QFETCH(double, dx);
        QFETCH(double, dy);
        Document document;
        document.addPath(makeShape(ShapeKind::Rectangle, {-250 + dx, -150, 380, 800}));
        const auto capsule = makePenPath({{{-280, 50 + dy}, {}}, {{110 + dx, 50 + dy}, {}},
                                         {{240 + dx, 200 + dy}, {0, 100}}, {{110 + dx, 350 + dy}, {}},
                                         {{-280, 350 + dy}, {}}, {{-400, 200 + dy}, {0, -100}}}, true);
        document.addPath(capsule, ShapePaint::Filled);
        document.selectAll();
        ShapeBuilder builder(document);
        auto touched = builder.crossed({-340, 200}, {200, 200});
        for (const auto index : builder.crossed({200, 200}, {-50, 500}))
            if (std::find(touched.begin(), touched.end(), index) == touched.end()) touched.push_back(index);
        for (const auto index : builder.crossed({-50, 500}, {-50, -50}))
            if (std::find(touched.begin(), touched.end(), index) == touched.end()) touched.push_back(index);
        QVERIFY(builder.apply(document, touched, BuilderOperation::Combine));
        QCOMPARE(document.shapes().size(), std::size_t(1));
        ShapeBuilder merged(document);
        QCOMPARE(merged.regions().size(), std::size_t(1));
        QCOMPARE(merged.hit({0, 200}), merged.hit({200, 200}));
        QVERIFY(merged.hit({0, 500}));
        QVERIFY(merged.hit({0, -50}));
        const auto path = document.shapes()[0].path;
        QVERIFY(merged.apply(document, {*merged.hit({0, 200})}, BuilderOperation::Combine));
        QCOMPARE(document.shapes()[0].path, path);
    }
    void builderRepeatedPartialMergeHasNoRetracedEdges() {
        Document document;
        document.addPath(makeShape(ShapeKind::Rectangle, {-76, 0, 189, 440.5}));
        document.addPath(makeShape(ShapeKind::Rectangle, {-233, 126, 471.5, 157}));
        document.addPath(makeShape(ShapeKind::Ellipse, {-139, 0, 315, 440.5}));
        document.selectAll();
        ShapeBuilder first(document);
        QVERIFY(first.apply(document, {1, 2, 5, 6, 8, 10, 11, 12, 14}, BuilderOperation::Combine));
        document.addPath(translated(document.shapes().back().path, {-47.5, 44}));
        document.selectAll();
        ShapeBuilder second(document);
        QVERIFY(second.apply(document, {1, 2, 3, 4, 5, 6, 10, 11}, BuilderOperation::Combine));
        document.selectAll();
        ShapeBuilder third(document);
        const auto touched = third.crossed({-140, 128}, {86, 58});
        QVERIFY(third.apply(document, touched, BuilderOperation::Combine));
        QCOMPARE(document.shapes().size(), std::size_t(4));
        const auto covered = [&](QPointF point) {
            return std::any_of(document.shapes().begin(), document.shapes().end(), [&](const Shape &shape) {
                return shape.path.contains(point);
            });
        };
        QVERIFY(covered({0, 200}));
        QVERIFY(covered({0, 10}));
        QVERIFY(covered({-200, 200}));
        QVERIFY(covered({230, 200}));
        QVERIFY(!covered({160, 400}));
        QVERIFY(!covered({-220, 430}));
        for (const auto &shape : document.shapes()) {
            for (auto polygon : shape.path.toSubpathPolygons()) {
                if (polygon.front() == polygon.back()) polygon.removeLast();
                for (int i = 0; i < polygon.size(); ++i) {
                    const auto a = polygon[(i + polygon.size() - 1) % polygon.size()];
                    const auto b = polygon[i], c = polygon[(i + 1) % polygon.size()];
                    const auto u = b - a, v = c - b;
                    const auto cross = u.x() * v.y() - u.y() * v.x();
                    QVERIFY2(std::abs(cross) >= 1e-7 || QPointF::dotProduct(u, v) >= -1e-6,
                             "Committed filled boundary contains a zero-area retraced edge");
                }
            }
        }
    }
    void builderEllipseBoundaryRemainsWithinSubpixelBandAtMaximumZoom() {
        Document document;
        document.addPath(makeShape(ShapeKind::Ellipse, {0, 0, 100, 100}));
        document.selectAll();
        ShapeBuilder builder(document);
        QVERIFY(builder.apply(document, {*builder.hit({50, 50})}, BuilderOperation::Combine));
        const auto &path = document.shapes()[0].path;
        for (int angle = 0; angle < 720; ++angle) {
            const auto radians = angle * 3.14159265358979323846 / 360;
            const QPointF direction(std::cos(radians), std::sin(radians));
            QVERIFY(path.contains(QPointF(50, 50) + direction * 49.9));
            QVERIFY(!path.contains(QPointF(50, 50) + direction * 50.1));
        }
    }
    void builderPinchesAndUntouchedSources() {
        QPainterPath pinched;
        pinched.moveTo(0, 0);
        pinched.lineTo(100, 0); pinched.lineTo(100, 100); pinched.lineTo(0, 100);
        pinched.lineTo(0, 0); pinched.lineTo(-100, 0); pinched.lineTo(-100, -100);
        pinched.lineTo(0, -100); pinched.closeSubpath();
        Document contact;
        contact.addPath(pinched);
        contact.selectAll();
        ShapeBuilder pieces(contact);
        QCOMPARE(pieces.regions().size(), std::size_t(2));
        QVERIFY(pieces.hit({50, 50}) != pieces.hit({-50, -50}));
        Document preserve;
        const auto a = *preserve.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        const auto ellipse = makeShape(ShapeKind::Ellipse, {200, 0, 100, 100});
        const auto b = *preserve.addPath(ellipse);
        const auto stroke = *preserve.addPath(makePenPath({{{0, 200}, {}}, {{100, 200}, {}}}), ShapePaint::Stroke);
        preserve.selectAll();
        ShapeBuilder builder(preserve);
        QCOMPARE(builder.regions().size(), std::size_t(2));
        QVERIFY(builder.apply(preserve, {*builder.hit({50, 50})}, BuilderOperation::Subtract));
        QVERIFY(!preserve.find(a));
        QCOMPARE(preserve.find(b)->path, ellipse);
        QCOMPARE(preserve.find(stroke)->paint, ShapePaint::Stroke);
    }
    void builderReappliesToFragmentsSharingFlattenedCurves_data() {
        QTest::addColumn<QPointF>("offset");
        QTest::newRow("aligned") << QPointF(0, 0);
        QTest::newRow("offset") << QPointF(37.5, 20);
        QTest::newRow("fractional") << QPointF(61.25, 45.5);
    }
    void builderReappliesToFragmentsSharingFlattenedCurves() {
        QFETCH(QPointF, offset);
        Document document;
        document.addPath(makeShape(ShapeKind::Ellipse, {0, 0, 300, 300}));
        document.addPath(makeShape(ShapeKind::Rectangle, QRectF(QPointF(150, 100) + offset, QSizeF(200, 300))));
        document.selectAll();
        ShapeBuilder first(document);
        const auto rectangleOnly = first.hit(QPointF(330, 380));
        QVERIFY(rectangleOnly);
        QVERIFY(first.apply(document, {*rectangleOnly}, BuilderOperation::Combine));
        // The flattened fragment now shares the ellipse's curved boundary.
        document.selectAll();
        ShapeBuilder again(document);
        for (const auto &region : again.regions()) {
            QVERIFY(!region.path.isEmpty());
            QVERIFY(region.path.boundingRect().width() > 1 && region.path.boundingRect().height() > 1);
        }
        std::vector<std::size_t> touched;
        for (const auto point : {QPointF(50, 150), QPointF(330, 380), QPointF(200, 150) + offset})
            if (const auto index = again.hit(point);
                index && std::find(touched.begin(), touched.end(), *index) == touched.end())
                touched.push_back(*index);
        QVERIFY(again.apply(document, touched, BuilderOperation::Combine));
        QCOMPARE(document.shapes().size(), std::size_t(1));
        document.selectAll();
        ShapeBuilder merged(document);
        QCOMPARE(merged.regions().size(), std::size_t(1));
        QCOMPARE(merged.hit({50, 150}), merged.hit({330, 380}));
    }
    void builderAbsorbsTangentSlivers_data() {
        QTest::addColumn<bool>("flattened");
        QTest::newRow("curve") << false;
        QTest::newRow("builder output") << true;
    }
    void builderAbsorbsTangentSlivers() {
        QFETCH(bool, flattened);
        Document document;
        document.addPath(makeShape(ShapeKind::Ellipse, {0, 0, 200, 200}));
        if (flattened) {
            document.selectAll();
            ShapeBuilder flatten(document);
            QVERIFY(flatten.apply(document, {0}, BuilderOperation::Combine));
        }
        // The stem's edge sits on the bowl's extreme, leaving hairline gaps beside the tangent.
        document.addPath(makeShape(ShapeKind::Rectangle, {90, 0, 20, 50}));
        document.selectAll();
        ShapeBuilder builder(document);
        for (const auto &region : builder.regions()) {
            double area = 0;
            const auto polygons = region.path.toSubpathPolygons();
            for (int i = 0; i < polygons.size(); ++i) {
                double contour = 0;
                for (int j = 1; j < polygons[i].size(); ++j)
                    contour += polygons[i][j - 1].x() * polygons[i][j].y() - polygons[i][j].x() * polygons[i][j - 1].y();
                area += (i == 0 ? 0.5 : -0.5) * std::abs(contour);
            }
            QVERIFY2(2 * area >= 0.5 * region.path.length(), "Shape Builder offered an untouchable sliver region");
        }
        QVERIFY(builder.hit({100, 25}));
        const auto touched = builder.crossed({20, 25}, {180, 25});
        QVERIFY(builder.apply(document, touched, BuilderOperation::Combine));
        QCOMPARE(document.shapes().size(), std::size_t(1));
        QVERIFY(document.shapes()[0].path.contains({100, 0.2}));
        document.selectAll();
        ShapeBuilder merged(document);
        QCOMPARE(merged.regions().size(), std::size_t(1));
    }
    void boundedDocumentAdmission() {
        Document document;
        const auto id = document.addPath(makeShape(ShapeKind::Rectangle, {400, -250, 100, 100}));
        QVERIFY(id);
        QVERIFY(!document.addPath(makeShape(ShapeKind::Rectangle, {490, 0, 20, 20})));
        QVERIFY(!document.replacePath(*id, translated(document.find(*id)->path, {1, 0})));
        QCOMPARE(document.find(*id)->path.boundingRect(), QRectF(400, -250, 100, 100));
        const auto line = document.addPath(makePenPath({{{-500, 750}, {}}, {{500, 750}, {}}}), ShapePaint::Stroke);
        QVERIFY(line);
        QCOMPARE(document.find(*line)->path.boundingRect(), QRectF(-500, 750, 1000, 0));
        QPainterPath curve;
        curve.moveTo(400, 0);
        curve.cubicTo(550, 100, 550, 200, 400, 300);
        QVERIFY(!document.addPath(curve, ShapePaint::Stroke));
    }
    void typographyGuideSnapping() {
        Document document;
        Viewport viewport;
        viewport.reset({200, 800});
        auto snap = snapPoint({313, 303}, document, viewport, {false, true});
        QVERIFY(snap);
        QCOMPARE(snap->point, QPointF(313, 300));
        QVERIFY(!snapPoint({313, 303}, document, viewport, {}));
        QVERIFY(!snapPoint({313, 303}, document, viewport, {true, false}));
        QVERIFY(!snapPoint({313, 303}, document, viewport, {false, true}, {}, SnapAxes::Horizontal));
        viewport.zoomAt({200, 800}, 0.5);
        viewport.pan({80, -90});
        snap = snapPoint({333, 363}, document, viewport, {false, true});
        QVERIFY(snap);
        QCOMPARE(snap->point, QPointF(333, 360));
        QVERIFY(!snapPoint(viewport.toScreen({520, 503}), document, viewport, {false, true}));
        viewport.reset({550,350});
        viewport.zoomAt({312.5,267.3},0.62);
        for (int i=0; i<7; ++i) viewport.zoomAt({301.2,253.7},1.1972173631218102);
        viewport.pan({31.4,-18.9});
        const auto boundary = viewport.toScreen({100,750});
        snap = snapPoint(boundary + QPointF(0,2), document, viewport, {false,true});
        QVERIFY(snap);
        QVERIFY(QLineF(snap->point, boundary).length()<1e-7);
    }
    void guideIntersectionSnapping() {
        GlyphDocumentDefinition definition;
        definition.xHeight = 497;
        const Document document(definition, {});
        Viewport view;
        view.reset({300, 600});
        const auto target = view.toScreen({0, 497});
        auto result = snapPoint(target + QPointF(3, 3), document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->point, target);
        const auto gridCrossing = view.toScreen({200, 497});
        result = snapPoint(gridCrossing + QPointF(3, 1), document, view, {true, true});
        QVERIFY(result);
        QCOMPARE(result->point, gridCrossing);
        result = snapPoint(view.toScreen({3, 103}), document, view, {true, true});
        QVERIFY(result);
        QCOMPARE(result->point, view.toScreen({0, 100}));
        result = snapPoint(gridCrossing + QPointF(3, 1), document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->point, gridCrossing + QPointF(3, 0));
        QVERIFY(!snapPoint(target + QPointF(3, 3), document, view, {}));
        result = snapPoint(gridCrossing + QPointF(3, 1), document, view, {true, false});
        QVERIFY(result);
        QCOMPARE(result->point, view.toScreen({200, 500}));
        definition.guideEnabled.xHeight = false;
        QVERIFY(!snapPoint(gridCrossing + QPointF(3, 1), Document(definition, {}), view, {false, true}));
        result = snapPoint(target + QPointF(8, 0), document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->point, target);
        result = snapPoint(target + QPointF(8.01, 0), document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->point, target + QPointF(8.01, 0));
        result = snapPoint(target + QPointF(6, 6), document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->point, target + QPointF(6, 0));
        result = snapPoint(target + QPointF(3, 3), document, view, {false, true}, {}, SnapAxes::Horizontal);
        QVERIFY(result);
        QCOMPARE(result->point, target + QPointF(0, 3));
        result = snapPoint(target + QPointF(3, 3), document, view, {false, true}, {}, SnapAxes::Vertical);
        QVERIFY(result);
        QCOMPARE(result->point, target + QPointF(3, 0));
        result = snapPoint(target + QPointF(3, 0), document, view, {false, true}, {}, SnapAxes::Horizontal);
        QVERIFY(result);
        QCOMPARE(result->point, target);
        QVERIFY(!snapPoint(view.toScreen({520, 497}), document, view, {true, true}));
        view.zoomAt({300, 600}, 0.5);
        view.pan({73, -41});
        const auto transformed = view.toScreen({0, 497});
        result = snapPoint(transformed + QPointF(3, 3), document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->point, transformed);
        QCOMPARE(result->distance, std::sqrt(18.));
        const auto zoomedGridCrossing = view.toScreen({200, 497});
        result = snapPoint(zoomedGridCrossing + QPointF(3, 0), document, view, {true, true});
        QVERIFY(result);
        QCOMPARE(result->point, zoomedGridCrossing);
    }
    void boundsGuideIntersectionSnapping() {
        GlyphDocumentDefinition definition;
        definition.xHeight = 497;
        Document document(definition, {});
        Viewport view;
        view.reset({300, 600});
        auto result = snapBounds({303, 106, 40, 40}, document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->first.point, QPointF(300, 103));
        QCOMPARE(result->second, QPointF(-3, -3));
        result = snapBounds({301, 120, 40, 40}, document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->first.point, QPointF(300, 120));
        QCOMPARE(result->second, QPointF(-1, 0));
        QVERIFY(!snapBounds({303, 106, 40, 40}, document, view, {}));
        result = snapBounds({301, 106, 50, 44}, document, view, {true, true});
        QVERIFY(result);
        QCOMPARE(result->first.point, QPointF(350, 150));
        QCOMPARE(result->second, QPointF(-1, 0));
        QPainterPath target;
        target.moveTo(43, 475);
        target.lineTo(43, 473);
        const auto id = document.addPath(target, ShapePaint::Stroke);
        QVERIFY(id);
        result = snapBounds({301, 106, 40, 40}, document, view, {false, true});
        QVERIFY(result);
        QCOMPARE(result->first.point, QPointF(343, 126));
        QCOMPARE(result->second, QPointF(2, 0));
        result = snapBounds({301, 106, 40, 40}, document, view, {false, true}, {}, {*id});
        QVERIFY(result);
        QCOMPARE(result->first.point, QPointF(300, 103));
        QCOMPARE(result->second, QPointF(-1, -3));
    }
    void penStrokeGeometry() {
        Document document;
        Viewport view;
        view.reset({0, 0});
        const auto path = makePenPath({{{0, 0}, {}}, {{50, 100}, {}}, {{100, 0}, {}}});
        const auto id = document.addPath(path, ShapePaint::Stroke);
        QVERIFY(id);
        QVERIFY(!document.hitTest({50, 25}, 3));
        QCOMPARE(document.hitTest({50, 100}, 3), id);
        QCOMPARE(snapPoint({25, 0}, document, view, {false, true})->point, QPointF(25, 0));
        auto snap = snapPoint({25, -50}, document, view, {false, true});
        QVERIFY(snap);
        QCOMPARE(snap->point, QPointF(25, -50));
        const auto horizontal = makePenPath({{{0, 10}, {}}, {{100, 10}, {}}});
        QCOMPARE(resized(horizontal, {20, 30, 200, 0}).boundingRect(), QRectF(20, 30, 200, 0));
        const auto vertical = makePenPath({{{10, 0}, {}}, {{10, 100}, {}}});
        QCOMPARE(resized(vertical, {30, 20, 0, 200}).boundingRect(), QRectF(30, 20, 0, 200));
        QVERIFY(resized(horizontal, {20, 30, 0, 0}).isEmpty());
        QVERIFY(resized(vertical, {20, 30, 0, 0}).isEmpty());
    }
    void geometry() {
        auto rectangle = makeShape(ShapeKind::Rectangle, QRectF(40, 60, -30, -40));
        QCOMPARE(rectangle.boundingRect(), QRectF(10, 20, 30, 40));
        QVERIFY(rectangle.contains({20, 30}));
        auto ellipse = makeShape(ShapeKind::Ellipse, {10, 20, 30, 40});
        QVERIFY(ellipse.contains({25, 40}));
        QVERIFY(!ellipse.contains({10, 20}));
        QVERIFY(ellipse.elementAt(1).isCurveTo());
        QCOMPARE(translated(ellipse, {-15, 9}).boundingRect(), QRectF(-5, 29, 30, 40));
        const auto scaled = resized(ellipse, {-50, -20, 60, 80});
        QCOMPARE(scaled.boundingRect(), QRectF(-50, -20, 60, 80));
        QCOMPARE(scaled.elementCount(), ellipse.elementCount());
        QVERIFY(scaled.elementAt(1).isCurveTo());
        QVERIFY(resized(ellipse, {0, 0, 0, 10}).isEmpty());
        QVERIFY(makeShape(ShapeKind::Rectangle, {0, 0, 1, 0}).isEmpty());
    }
    void documentOperations() {
        Document document;
        auto first = document.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 100, 100}));
        auto second = document.addPath(makeShape(ShapeKind::Ellipse, {25, 25, 100, 100}));
        QVERIFY(first && second && first != second);
        QCOMPARE(document.hitTest({50, 50}, 0), second);
        QCOMPARE(document.hitTest({5, 5}, 0), first);
        QVERIFY(!document.hitTest({200, 200}, 3));
        QCOMPARE(document.hitTest({-2, 50}, 3), first);
        document.select(first);
        QVERIFY(document.replacePath(*first, translated(document.find(*first)->path, {200, -20})));
        QCOMPARE(document.find(*first)->path.boundingRect(), QRectF(200, -20, 100, 100));
        QVERIFY(document.deleteSelection());
        QVERIFY(!document.selection());
        QVERIFY(!document.find(*first));
        QCOMPARE(document.shapes().size(), std::size_t(1));
        document.select(999);
        QVERIFY(!document.selection());
        QVERIFY(!document.addPath({}));
        QVERIFY(!document.replacePath(*second, {}));
        auto third = document.addPath(makeShape(ShapeKind::Rectangle, {0, 0, 10, 10}));
        QVERIFY(*third > *second);
    }
    void snapping() {
        Viewport view;
        view.reset({0, 0});
        Document doc;
        const auto rectangle = doc.addPath(makeShape(ShapeKind::Rectangle, {10, 10, 30, 30}));
        QVERIFY(!snapPoint({14, -25}, doc, view, {}));
        auto edge = snapPoint({14, -25}, doc, view, {false, true});
        QVERIFY(edge);
        QCOMPARE(edge->point, QPointF(10, -25));
        QVERIFY(!snapPoint({14, -25}, doc, view, {false, true}, rectangle));
        auto grid = snapPoint({23, -26}, doc, view, {true, false});
        QVERIFY(grid);
        QCOMPARE(grid->point, QPointF(25, -25));
        auto both = snapPoint({23, -26}, doc, view, {true, true});
        QVERIFY(both);
        QCOMPARE(both->point, QPointF(25, -25));
        auto geometryWins = snapPoint({12, -25}, doc, view, {true, true});
        QVERIFY(geometryWins);
        QCOMPARE(geometryWins->point, QPointF(10, -25));
        QVERIFY(!snapPoint({12, -12}, Document{}, view, {true, false}));
        auto horizontal = snapPoint({14, -27}, doc, view, {false, true}, {}, SnapAxes::Horizontal);
        QVERIFY(horizontal);
        QCOMPARE(horizontal->point, QPointF(10, -27));
        auto gridEdge = snapPoint({23, -12}, doc, view, {true, false}, {}, SnapAxes::Horizontal);
        QVERIFY(gridEdge);
        QCOMPARE(gridEdge->point, QPointF(25, -12));
        Document tieDocument;
        tieDocument.addPath(makeShape(ShapeKind::Rectangle, {10, 10, 11, 30}));
        auto tie = snapPoint({23, -25}, tieDocument, view, {true, true});
        QVERIFY(tie);
        QCOMPARE(tie->point, QPointF(25, -25)); // Grid wins an exact distance tie.
        view.zoomAt({0, 0}, 0.1);
        QCOMPARE(gridStep(view.zoom()), 200.0);
        auto coarseGrid = snapPoint({19, -21}, Document{}, view, {true, false});
        QVERIFY(coarseGrid);
        QCOMPARE(coarseGrid->point, QPointF(20, -20));
        view.reset({0, 0});
        view.zoomAt({0, 0}, 4);
        QCOMPARE(gridStep(view.zoom()), 12.5);
        auto distant = snapPoint({49, -100}, doc, view, {false, true});
        QVERIFY(!distant); // Nine screen pixels from the rectangle's left edge.
        auto near = snapPoint({47, -100}, doc, view, {false, true});
        QVERIFY(near);
        QCOMPARE(near->point, QPointF(40, -100));
    }
    void curveSnapping() {
        Document doc;
        Viewport view;
        view.reset({0, 0});
        auto ellipse = makeShape(ShapeKind::Ellipse, {0, 0, 100, 100});
        doc.addPath(ellipse);
        const auto curvePoint = view.toScreen(ellipse.pointAtPercent(0.125));
        auto result = snapPoint(curvePoint + QPointF(3, -3), doc, view, {false, true});
        QVERIFY(result);
        QVERIFY(QLineF(result->point, curvePoint).length() < 0.4);
        QVERIFY(QLineF(result->point, QPointF(100, -100)).length() > 15);
        QCOMPARE(doc.shapes()[0].path, ellipse);
        auto edgeResize = snapPoint({89, -85}, doc, view, {false, true}, {}, SnapAxes::Horizontal);
        QVERIFY(edgeResize);
        QCOMPARE(edgeResize->point.y(), -85.0);
        QVERIFY(std::abs(edgeResize->point.x() - 85.7) < 0.5);
    }
    void viewportMapping() {
        Viewport view;
        view.reset({400, 300});
        QCOMPARE(view.toScreen({20, 30}), QPointF(420, 270));
        QCOMPARE(view.toDocument({420, 270}), QPointF(20, 30));
        const QPointF anchor(153, 97);
        const auto point = view.toDocument(anchor);
        view.zoomAt(anchor, 2.5);
        QCOMPARE(view.toScreen(point), anchor);
        QCOMPARE(view.transform().map(point), anchor);
        view.pan({-70, 35});
        QCOMPARE(view.toScreen(point), anchor + QPointF(-70, 35));
        const QPointF arbitrary(-120.25, 208.75);
        QVERIFY(QLineF(arbitrary, view.toDocument(view.toScreen(arbitrary))).length() < 1e-10);
        view.zoomAt(anchor, 1e20);
        QCOMPARE(view.zoom(), 32.0);
        QVERIFY(QLineF(view.toScreen(view.toDocument(anchor)), anchor).length() < 1e-10);
        view.zoomAt(anchor, 1e-20);
        QCOMPARE(view.zoom(), 0.05);
        view.zoomAt(anchor, std::numeric_limits<double>::infinity());
        QCOMPARE(view.zoom(), 0.05);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
