#include "document.h"
#include "viewport.h"
#include "snapping.h"
#include <QtTest>
#include <limits>
using namespace fontu;
class CoreTests : public QObject {
    Q_OBJECT
private slots:
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
