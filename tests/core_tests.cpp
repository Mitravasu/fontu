#include "document.h"
#include "viewport.h"
#include "snapping.h"
#include <QtTest>
#include <limits>
using namespace fontu;
class CoreTests : public QObject {
    Q_OBJECT
private slots:
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
