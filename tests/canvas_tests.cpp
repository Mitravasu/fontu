#include "canvas.h"
#include "snap_menu.h"
#include "glyph_panel.h"
#include <QMainWindow>
#include <QListWidget>
#include <QScrollBar>
#include <QToolBar>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtTest>
using namespace fontu;
namespace {
void movePointer(Canvas &canvas, QPoint position, Qt::MouseButton button = Qt::LeftButton) {
    QMouseEvent event(QEvent::MouseMove, position, canvas.mapToGlobal(position),
                       Qt::NoButton, button, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &event);
}
bool markerAt(Canvas &canvas, QPoint center) {
    const auto image = canvas.grab().toImage();
    int amber = 0;
    for (int y = center.y() - 7; y <= center.y() + 7; ++y)
        for (int x = center.x() - 7; x <= center.x() + 7; ++x) {
            const auto color = image.pixelColor(x, y);
            if (color.red() > 170 && color.green() > 70 && color.green() < 165 && color.blue() < 90) ++amber;
        }
    return amber >= 12;
}
void drag(Canvas &canvas, QPoint start, QPoint end, Qt::MouseButton button = Qt::LeftButton) {
    QTest::mousePress(&canvas, button, Qt::NoModifier, start);
    movePointer(canvas, end, button);
    QTest::mouseRelease(&canvas, button, Qt::NoModifier, end);
}
void zoom(Canvas &canvas, QPoint position, int delta) {
    QWheelEvent event(position, canvas.mapToGlobal(position), {}, {0, delta},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&canvas, &event);
}
void prepare(Canvas &canvas) {
    // Events are sent directly to the widget. Desktop focus must not cancel test drags.
    canvas.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    canvas.setAttribute(Qt::WA_ShowWithoutActivating);
    canvas.resize(1160, 1080);
    canvas.show();
    QVERIFY(QTest::qWaitForWindowExposed(&canvas));
    QApplication::processEvents();
    canvas.resetView();
    canvas.resize(800, 600);
    QApplication::processEvents();
    drag(canvas, {400, 300}, {400, 50}, Qt::MiddleButton);
}
}
class CanvasTests : public QObject {
    Q_OBJECT
private slots:
    void multiSelectedMoveAndCancellation() {
        Canvas canvas;
        prepare(canvas);
        QTest::keyClick(&canvas, Qt::Key_R);
        drag(canvas, {100, 100}, {200, 200});
        drag(canvas, {250, 100}, {350, 200});
        QTest::keyClick(&canvas, Qt::Key_V);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {150, 150});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ShiftModifier, {300, 150});
        const auto original = canvas.document().shapes();
        drag(canvas, {150, 150}, {170, 180});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()), QRectF(120, 130, 100, 100));
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect()), QRectF(270, 130, 100, 100));
        QCOMPARE(canvas.document().selectedIds().size(), std::size_t(2));
        canvas.undo();
        QCOMPARE(canvas.document().shapes()[0].path, original[0].path);
        QCOMPARE(canvas.document().shapes()[1].path, original[1].path);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 150});
        movePointer(canvas, {320, 180});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {320, 180});
        QCOMPARE(canvas.document().shapes()[0].path, original[0].path);
        QCOMPARE(canvas.document().shapes()[1].path, original[1].path);
        QVERIFY(canvas.canRedo());
    }
    void builderEventsMergeSubtractUndoAndCancel() {
        Canvas canvas;
        prepare(canvas);
        QTest::keyClick(&canvas, Qt::Key_R);
        drag(canvas, {100, 100}, {200, 200});
        drag(canvas, {150, 100}, {250, 200});
        const auto original = canvas.document().shapes();
        QTest::keyClick(&canvas, Qt::Key_V);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {125, 150});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::ShiftModifier, {225, 150});
        QCOMPARE(canvas.document().selectedIds().size(), std::size_t(2));
        QTest::keyClick(&canvas, Qt::Key_B);
        QCOMPARE(canvas.tool(), Tool::ShapeBuilder);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {125, 150});
        movePointer(canvas, {225, 150});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QCOMPARE(canvas.projectSnapshot().glyphs[0].shapes[0].path, original[0].path);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {225, 150});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()), QRectF(100, 100, 150, 100));
        canvas.undo();
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QCOMPARE(canvas.document().shapes()[0].path, original[0].path);
        QCOMPARE(canvas.document().shapes()[1].path, original[1].path);
        QCOMPARE(canvas.document().selectedIds().size(), std::size_t(2));
        canvas.redo();
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.undo();
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::AltModifier, {175, 150});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        const auto intersection = canvas.viewport().toDocument({175, 150});
        for (const auto &shape : canvas.document().shapes()) QVERIFY(!shape.path.contains(intersection));
        canvas.undo();
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {125, 150});
        movePointer(canvas, {225, 150});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {225, 150});
        QCOMPARE(canvas.document().shapes()[0].path, original[0].path);
        QCOMPARE(canvas.document().shapes()[1].path, original[1].path);
        QVERIFY(canvas.canRedo());
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {125, 150});
        QVERIFY(canvas.setActiveGlyph({2}));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {225, 150});
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(canvas.setActiveGlyph({1}));
        QCOMPARE(canvas.document().shapes()[0].path, original[0].path);
    }
    void glyphDrawingsHistoryAndSelectionStayIndependent() {
        Canvas canvas({{{42}, "zero"}, {{900}, "alternate one"}});
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100,100}, {200,200});
        const auto zero = canvas.document().shapes()[0].path;
        const auto selection = canvas.document().selection();
        canvas.undo();
        QVERIFY(canvas.canRedo());
        QVERIFY(canvas.setActiveGlyph({900}));
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(!canvas.canUndo());
        QVERIFY(!canvas.canRedo());
        drag(canvas, {300,100}, {450,250});
        const auto one = canvas.document().shapes()[0].path;
        const auto transform = canvas.viewport().transform();
        QVERIFY(canvas.setActiveGlyph({42}));
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(canvas.canRedo());
        canvas.redo();
        QCOMPARE(canvas.document().shapes()[0].path, zero);
        QCOMPARE(canvas.document().selection(), selection);
        QVERIFY(canvas.setActiveGlyph({900}));
        QCOMPARE(canvas.document().shapes()[0].path, one);
        QCOMPARE(canvas.viewport().transform(), transform);
        QCOMPARE(canvas.tool(), Tool::Rectangle);
        canvas.undo();
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(canvas.setActiveGlyph({42}));
        QCOMPARE(canvas.document().shapes()[0].path, zero);
        QVERIFY(!canvas.setActiveGlyph({77}));
        QCOMPARE(canvas.activeGlyphId().value, std::uint64_t(42));
    }
    void switchingCancelsDraftsAndRollsBackGestures() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100,100}, {200,200});
        const auto original = canvas.document().shapes()[0].path;
        canvas.setTool(Tool::Select);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {150,150});
        movePointer(canvas, {250,250});
        QVERIFY(canvas.document().shapes()[0].path != original);
        QVERIFY(canvas.setActiveGlyph({2}));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {250,250});
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(canvas.setActiveGlyph({1}));
        QCOMPARE(canvas.document().shapes()[0].path, original);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300,100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {400,100});
        canvas.undo();
        QVERIFY(canvas.canRedo());
        QVERIFY(canvas.setActiveGlyph({2}));
        QVERIFY(!canvas.canUndo());
        QVERIFY(!canvas.canRedo());
        QTest::keyClick(&canvas, Qt::Key_Return);
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(canvas.setActiveGlyph({1}));
        QVERIFY(!canvas.canRedo());
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.setTool(Tool::Rectangle);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {300,100});
        movePointer(canvas, {400,200});
        QVERIFY(canvas.setActiveGlyph({2}));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {400,200});
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(canvas.setActiveGlyph({1}));
        QCOMPARE(canvas.document().shapes()[0].path, original);
    }
    void arbitraryGlyphCollectionScrollsAndPickerTracksSelection() {
        std::vector<GlyphSeed> seeds;
        for (std::uint64_t i = 0; i < 80; ++i)
            seeds.push_back({{1000 + i * 7}, QString("alternate %1").arg(i)});
        QMainWindow window;
        auto *canvas = new Canvas(seeds);
        window.setCentralWidget(canvas);
        addGlyphPanel(window, *canvas);
        window.resize(800,600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *picker = window.findChild<QListWidget *>("glyphPicker");
        QVERIFY(picker);
        QCOMPARE(picker->count(), 80);
        QCOMPARE(picker->currentItem()->text(), QString("alternate 0"));
        QVERIFY(picker->verticalScrollBar()->maximum() > 0);
        picker->scrollToItem(picker->item(79));
        QApplication::processEvents();
        QTest::mouseClick(picker->viewport(), Qt::LeftButton, Qt::NoModifier,
                          picker->visualItemRect(picker->item(79)).center());
        QCOMPARE(canvas->activeGlyphId().value, std::uint64_t(1553));
        QVERIFY(canvas->hasFocus());
        QTest::mouseClick(picker->viewport(), Qt::LeftButton, Qt::NoModifier,
                          picker->visualItemRect(picker->item(79)).center());
        QVERIFY(canvas->hasFocus());
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_R);
        QCOMPARE(canvas->tool(), Tool::Rectangle);
        QVERIFY(canvas->setActiveGlyph({1021}));
        QCOMPARE(picker->currentItem()->text(), QString("alternate 3"));
    }
    void boundedDrawingEditingAndHistory() {
        Canvas canvas;
        prepare(canvas);
        const auto screen = [&](QPointF point) { return canvas.viewport().toScreen(point).toPoint(); };
        for (auto tool : {Tool::Rectangle, Tool::Ellipse}) {
            canvas.setTool(tool);
            drag(canvas, screen({501, 100}), screen({300, 200}));
            QVERIFY(canvas.document().shapes().empty());
            drag(canvas, screen({350, 100}), screen({600, -400}));
            QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(350, -250, 150, 350));
            canvas.undo();
            QVERIFY(canvas.document().shapes().empty());
        }
        canvas.setTool(Tool::Rectangle);
        drag(canvas, screen({350, 100}), screen({450, 200}));
        canvas.setTool(Tool::Select);
        drag(canvas, screen({400, 150}), screen({700, -500}));
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(400, -250, 100, 100));
        drag(canvas, screen({400, -150}), screen({300, 900}));
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(300, -250, 200, 1000));
        canvas.undo();
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(400, -250, 100, 100));
        canvas.undo();
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(350, 100, 100, 100));
        canvas.redo(); canvas.redo();
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(300, -250, 200, 1000));
    }
    void boundedPenClosureHover() {
        Canvas canvas;
        prepare(canvas);
        drag(canvas, {400,300}, {100,300}, Qt::MiddleButton);
        const auto screen = [&](QPointF point) { return canvas.viewport().toScreen(point).toPoint(); };
        canvas.setTool(Tool::Pen);
        for (auto point : {QPointF(500,100), QPointF(450,100), QPointF(450,200)})
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, screen(point));
        movePointer(canvas, screen({501,100}), Qt::NoButton);
        QCOMPARE(canvas.cursor().shape(), Qt::CrossCursor);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, screen({501,100}));
        QVERIFY(canvas.document().shapes().empty());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, screen({498,100}));
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.document().shapes()[0].paint, ShapePaint::Filled);
    }
    void boundedPenAndNavigation() {
        Canvas canvas;
        prepare(canvas);
        const auto screen = [&](QPointF point) { return canvas.viewport().toScreen(point).toPoint(); };
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, screen({501, 0}));
        QVERIFY(!canvas.canUndo());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, screen({480, 0}));
        drag(canvas, screen({490, 100}), screen({800, 500}));
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        const auto path = canvas.document().shapes()[0].path;
        QVERIFY(path.elementAt(1).isCurveTo());
        QCOMPARE(QPointF(path.elementAt(3)), QPointF(490, 100));
        QVERIFY(canvas.document().definition().bounds.contains(path.controlPointRect()));
        drag(canvas, screen({600, 0}), screen({650, 100}), Qt::MiddleButton);
        zoom(canvas, {400, 300}, 120);
        QCOMPARE(canvas.document().shapes()[0].path, path);
        canvas.undo();
        QVERIFY(canvas.document().shapes().empty());
        canvas.redo();
        QCOMPARE(canvas.document().shapes()[0].path, path);
    }
    void undoRedoCompletedEdits() {
        Canvas canvas;
        prepare(canvas);
        QVERIFY(!canvas.canUndo());
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {220, 200});
        const auto id = canvas.document().shapes()[0].id;
        const auto initial = canvas.document().shapes()[0].path;
        canvas.setTool(Tool::Select);
        drag(canvas, {150, 150}, {180, 170});
        const auto moved = canvas.document().shapes()[0].path;
        drag(canvas, {250, 220}, {280, 240});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(130, 120, 150, 120));
        QTest::keyClick(&canvas, Qt::Key_Delete);
        QVERIFY(canvas.document().shapes().empty());
        canvas.undo();
        QCOMPARE(canvas.document().selection(), std::optional<ShapeId>(id));
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(130, 120, 150, 120));
        canvas.undo();
        QCOMPARE(canvas.document().shapes()[0].path, moved);
        canvas.undo();
        QCOMPARE(canvas.document().shapes()[0].path, initial);
        canvas.undo();
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(!canvas.canUndo());
        canvas.redo();
        QCOMPARE(canvas.document().shapes()[0].id, id);
        QCOMPARE(canvas.document().shapes()[0].path, initial);
        canvas.redo(); canvas.redo(); canvas.redo();
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(!canvas.canRedo());
    }
    void historyIgnoresNoOpsAndNavigation() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {220, 200});
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {300, 100}, {420, 200});
        canvas.undo();
        QVERIFY(canvas.canRedo());
        canvas.setTool(Tool::Select);
        drag(canvas, {150, 150}, {150, 150});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::keyClick(&canvas, Qt::Key_Delete);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {300, 100}, {301, 101});
        drag(canvas, {400, 300}, {450, 320}, Qt::MiddleButton);
        zoom(canvas, {400, 300}, 120);
        canvas.resetView();
        canvas.setGridSnapEnabled(true);
        QVERIFY(canvas.canRedo());
        canvas.redo();
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        canvas.undo();
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {500, 100}, {600, 200});
        QVERIFY(!canvas.canRedo());
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        canvas.undo(); canvas.redo();
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect()),
                 QRectF(500, 100, 100, 100));
    }
    void historyCancelsUnfinishedGestures() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {220, 200});
        const auto original = canvas.document().shapes()[0].path;
        canvas.setTool(Tool::Select);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {150, 150});
        movePointer(canvas, {190, 180});
        canvas.undo();
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {190, 180});
        QVERIFY(canvas.document().shapes().empty());
        canvas.redo();
        QCOMPARE(canvas.document().shapes()[0].path, original);
        canvas.setTool(Tool::Rectangle);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        movePointer(canvas, {400, 200});
        canvas.undo();
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {400, 200});
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {400, 200});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        canvas.redo();
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.document().shapes()[0].path, original);
    }
    void penVertexHistory() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {500, 100}, {600, 200});
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        drag(canvas, {220, 160}, {250, 180});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {320, 210});
        canvas.undo();
        QVERIFY(canvas.canUndo());
        QVERIFY(canvas.canRedo());
        canvas.undo();
        canvas.undo();
        QVERIFY(!canvas.canUndo());
        QVERIFY(canvas.canRedo());
        canvas.undo();
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.redo();
        canvas.redo();
        movePointer(canvas, {450, 240}, Qt::NoButton);
        QVERIFY(canvas.canRedo());
        QTest::keyClick(&canvas, Qt::Key_Return);
        QPainterPath expected;
        expected.moveTo(100, 100);
        expected.cubicTo(100, 100, 190, 140, 220, 160);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QCOMPARE(canvas.viewport().transform().map(canvas.document().shapes()[1].path), expected);
        QVERIFY(!canvas.canRedo());
        canvas.undo();
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.redo();
        QCOMPARE(canvas.viewport().transform().map(canvas.document().shapes()[1].path), expected);
    }
    void penVertexBranchAndCancellation() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {500, 100}, {600, 200});
        canvas.undo();
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 100});
        QVERIFY(!canvas.canRedo());
        canvas.redo();
        QVERIFY(canvas.document().shapes().empty());
        canvas.undo();
        QVERIFY(canvas.canRedo());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {250, 200});
        QVERIFY(!canvas.canRedo());
        canvas.redo();
        QTest::keyClick(&canvas, Qt::Key_Return);
        QPainterPath expected;
        expected.moveTo(100, 100);
        expected.lineTo(250, 200);
        QCOMPARE(canvas.viewport().transform().map(canvas.document().shapes()[0].path), expected);
        QVERIFY(!canvas.canRedo());
        for (int cancel = 0; cancel < 3; ++cancel) {
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
            canvas.undo();
            QVERIFY(!canvas.canUndo());
            QVERIFY(canvas.canRedo());
            if (cancel == 0) QTest::keyClick(&canvas, Qt::Key_Escape);
            else if (cancel == 1) canvas.setTool(Tool::Select);
            else {
                QFocusEvent event(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
                QApplication::sendEvent(&canvas, &event);
            }
            QVERIFY(canvas.canUndo());
            QVERIFY(!canvas.canRedo());
            canvas.undo();
            QVERIFY(canvas.document().shapes().empty());
            canvas.redo();
            QCOMPARE(canvas.viewport().transform().map(canvas.document().shapes()[0].path), expected);
            canvas.setTool(Tool::Pen);
        }
    }
    void penVertexHistoryDuringHandleDrag() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {220, 160});
        movePointer(canvas, {250, 180});
        canvas.undo();
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {290, 220});
        canvas.redo();
        QTest::keyClick(&canvas, Qt::Key_Return);
        QPainterPath expected;
        expected.moveTo(100, 100);
        expected.cubicTo(100, 100, 190, 140, 220, 160);
        QCOMPARE(canvas.viewport().transform().map(canvas.document().shapes()[0].path), expected);
    }
    void historyPenAndCancellation() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        drag(canvas, {220, 160}, {250, 180});
        QTest::keyClick(&canvas, Qt::Key_Return);
        const auto stroke = canvas.document().shapes()[0];
        canvas.undo(); canvas.redo();
        QCOMPARE(canvas.document().shapes()[0].paint, ShapePaint::Stroke);
        QCOMPARE(canvas.document().shapes()[0].path, stroke.path);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {450, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {400, 240});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        const auto filled = canvas.document().shapes()[1];
        canvas.undo(); canvas.redo();
        QCOMPARE(canvas.document().shapes()[1].paint, ShapePaint::Filled);
        QCOMPARE(canvas.document().shapes()[1].path, filled.path);
        canvas.setTool(Tool::Select);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {380, 150});
        movePointer(canvas, {410, 180});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {410, 180});
        canvas.undo();
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.document().shapes()[0].path, stroke.path);
    }
    void penClosedFilledShape() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 300});
        movePointer(canvas, {106, 100}, Qt::NoButton);
        QCOMPARE(canvas.cursor().shape(), Qt::PointingHandCursor);
        QTest::keyPress(&canvas, Qt::Key_Space);
        QCOMPARE(canvas.cursor().shape(), Qt::OpenHandCursor);
        QTest::keyRelease(&canvas, Qt::Key_Space);
        QCOMPARE(canvas.cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(canvas.document().shapes().empty());
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {106, 100});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        const auto shape = canvas.document().shapes()[0];
        QCOMPARE(shape.paint, ShapePaint::Filled);
        QCOMPARE(shape.path.elementCount(), 4);
        QCOMPARE(QPointF(shape.path.elementAt(3)), QPointF(-300, 200));
        QCOMPARE(canvas.document().selection(), std::optional<ShapeId>(shape.id));
        QCOMPARE(canvas.tool(), Tool::Pen);
        movePointer(canvas, {150, 150});
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {150, 150});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.document().shapes()[0].path, shape.path);
        canvas.setTool(Tool::Select);
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 180});
        QCOMPARE(canvas.document().selection(), std::optional<ShapeId>(shape.id));
        drag(canvas, {200, 180}, {250, 200});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(150, 120, 200, 200));
    }
    void penCurvedClosingSegment() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        drag(canvas, {200, 300}, {230, 280});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        const auto &shape = canvas.document().shapes()[0];
        QCOMPARE(shape.paint, ShapePaint::Filled);
        QCOMPARE(shape.path.elementCount(), 8);
        QVERIFY(shape.path.elementAt(5).isCurveTo());
        QCOMPARE(QPointF(shape.path.elementAt(5)), QPointF(-170, 20));
        QCOMPARE(QPointF(shape.path.elementAt(6)), QPointF(-300, 200));
        QCOMPARE(QPointF(shape.path.elementAt(7)), QPointF(-300, 200));
        QCOMPARE(shape.path.currentPosition(), QPointF(-300, 200));
    }
    void penClosureMinimumAndCaptureDistance() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QVERIFY(canvas.document().shapes().empty());
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        movePointer(canvas, {100, 100}, Qt::NoButton);
        QCOMPARE(canvas.cursor().shape(), Qt::CrossCursor);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QVERIFY(canvas.document().shapes().empty());
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.document().shapes()[0].paint, ShapePaint::Stroke);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 300});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {109, 100});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QTest::keyClick(&canvas, Qt::Key_Enter);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QCOMPARE(canvas.document().shapes()[1].paint, ShapePaint::Stroke);
        QCOMPARE(canvas.document().shapes()[1].path.elementCount(), 4);
        QCOMPARE(QPointF(canvas.document().shapes()[1].path.elementAt(3)), QPointF(-291, 200));
    }
    void penClosureAfterNavigation() {
        for (const auto delta : {-240, 240}) {
            Canvas canvas;
            prepare(canvas);
            canvas.setGridSnapEnabled(true);
            canvas.setTool(Tool::Pen);
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {103, 102});
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {298, 102});
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {198, 298});
            drag(canvas, {400, 300}, {450, 320}, Qt::MiddleButton);
            zoom(canvas, {400, 300}, delta);
            const auto first = canvas.viewport().toScreen({-300, 200});
            movePointer(canvas, first.toPoint() + QPoint(7, 0), Qt::NoButton);
            QCOMPARE(canvas.cursor().shape(), Qt::PointingHandCursor);
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, first.toPoint() + QPoint(7, 0));
            QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
            const auto &shape = canvas.document().shapes()[0];
            QCOMPARE(shape.paint, ShapePaint::Filled);
            QCOMPARE(shape.path.elementCount(), 4);
            QCOMPARE(QPointF(shape.path.elementAt(0)), QPointF(-300, 200));
            QCOMPARE(QPointF(shape.path.elementAt(1)), QPointF(-100, 200));
            QCOMPARE(QPointF(shape.path.elementAt(2)), QPointF(-200, 0));
            QCOMPARE(QPointF(shape.path.elementAt(3)), QPointF(-300, 200));
        }
    }
    void penClosureDraftLifecycle() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        for (int path = 0; path < 2; ++path) {
            const int offset = path * 250;
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100 + offset, 100});
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300 + offset, 100});
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200 + offset, 300});
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100 + offset, 100});
            QCOMPARE(canvas.document().shapes().size(), std::size_t(path + 1));
            QTest::keyClick(&canvas, Qt::Key_Return);
            QCOMPARE(canvas.document().shapes().size(), std::size_t(path + 1));
        }
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 400});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 400});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 500});
        movePointer(canvas, {100, 400}, Qt::NoButton);
        QCOMPARE(canvas.cursor().shape(), Qt::PointingHandCursor);
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QCOMPARE(canvas.cursor().shape(), Qt::CrossCursor);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 400});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QTest::keyClick(&canvas, Qt::Key_Escape);
    }
    void penStraightAndZigzag() {
        Canvas canvas;
        prepare(canvas);
        QTest::keyClick(&canvas, Qt::Key_P);
        QCOMPARE(canvas.tool(), Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QVERIFY(canvas.document().shapes().empty());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        movePointer(canvas, {400, 200}, Qt::NoButton);
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        const auto &shape = canvas.document().shapes()[0];
        QCOMPARE(shape.paint, ShapePaint::Stroke);
        QCOMPARE(shape.path.elementCount(), 3);
        QCOMPARE(QPointF(shape.path.elementAt(0)), QPointF(-300, 200));
        QCOMPARE(QPointF(shape.path.elementAt(1)), QPointF(-200, 100));
        QCOMPARE(QPointF(shape.path.elementAt(2)), QPointF(-100, 200));
        QCOMPARE(canvas.tool(), Tool::Pen);
        QVERIFY(!canvas.document().hitTest({-200, 175}, 5));
        QCOMPARE(canvas.document().hitTest({-200, 100}, 5), std::optional<ShapeId>(shape.id));
        canvas.setTool(Tool::Select);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 125});
        QVERIFY(!canvas.document().selection());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        QCOMPARE(canvas.document().selection(), std::optional<ShapeId>(shape.id));
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {350, 300});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {450, 300});
        QTest::keyClick(&canvas, Qt::Key_Enter);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QCOMPARE(canvas.document().shapes()[1].path.boundingRect(), QRectF(-50, 0, 100, 0));
    }
    void penCubicAndCommitTiming() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        drag(canvas, {100, 100}, {130, 130});
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        movePointer(canvas, {230, 180});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QVERIFY(canvas.document().shapes().empty());
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {230, 180});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        const auto &path = canvas.document().shapes()[0].path;
        QCOMPARE(path.elementCount(), 7);
        QCOMPARE(QPointF(path.elementAt(0)), QPointF(-300, 200));
        QCOMPARE(QPointF(path.elementAt(1)), QPointF(-300, 200));
        QCOMPARE(QPointF(path.elementAt(2)), QPointF(-230, 80));
        QCOMPARE(QPointF(path.elementAt(3)), QPointF(-200, 100));
        QCOMPARE(QPointF(path.elementAt(4)), QPointF(-170, 120));
        QCOMPARE(QPointF(path.elementAt(5)), QPointF(-100, 200));
        QCOMPARE(QPointF(path.elementAt(6)), QPointF(-100, 200));
        QVERIFY(path.elementAt(1).isCurveTo());
        QVERIFY(path.elementAt(4).isCurveTo());
    }
    void penSnappedAnchorsAndHandles() {
        Canvas canvas;
        prepare(canvas);
        canvas.setGridSnapEnabled(true);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {103, 102});
        drag(canvas, {198, 199}, {227, 176});
        QTest::keyClick(&canvas, Qt::Key_Return);
        const auto &gridPath = canvas.document().shapes()[0].path;
        QCOMPARE(QPointF(gridPath.elementAt(0)), QPointF(-300, 200));
        QCOMPARE(QPointF(gridPath.elementAt(2)), QPointF(-225, 75));
        QCOMPARE(QPointF(gridPath.elementAt(3)), QPointF(-200, 100));
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {303, 302});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {398, 399});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes()[1].path.elementCount(), 2);
        canvas.setGridSnapEnabled(false);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {500, 100}, {600, 200});
        canvas.setGeometrySnapEnabled(true);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {497, 130});
        drag(canvas, {602, 180}, {602, 97});
        QTest::keyClick(&canvas, Qt::Key_Return);
        const auto &geometryPath = canvas.document().shapes()[3].path;
        QCOMPARE(QPointF(geometryPath.elementAt(0)), QPointF(100, 170));
        QCOMPARE(QPointF(geometryPath.elementAt(2)), QPointF(200, 40));
        QCOMPARE(QPointF(geometryPath.elementAt(3)), QPointF(200, 120));
    }
    void penCancellation() {
        Canvas canvas;
        prepare(canvas);
        for (int cancel = 0; cancel < 7; ++cancel) {
            canvas.setTool(Tool::Pen);
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
            QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
            switch (cancel) {
            case 0: QTest::keyClick(&canvas, Qt::Key_Escape); break;
            case 1: canvas.setTool(Tool::Select); canvas.setTool(Tool::Pen); break;
            case 2: canvas.setGridSnapEnabled(!canvas.gridSnapEnabled()); break;
            case 3: canvas.setGeometrySnapEnabled(!canvas.geometrySnapEnabled()); break;
            case 4: canvas.resetView(); break;
            case 5: { QFocusEvent lost(QEvent::FocusOut); QApplication::sendEvent(&canvas, &lost); break; }
            case 6: canvas.resize(820, 620); QApplication::processEvents(); break;
            }
            QTest::keyClick(&canvas, Qt::Key_Return);
            QVERIFY(canvas.document().shapes().empty());
            if (cancel == 4) prepare(canvas);
        }
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        movePointer(canvas, {230, 180});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {230, 180});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QVERIFY(canvas.document().shapes().empty());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
    }
    void penDraftSurvivesNavigation() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        drag(canvas, {400, 300}, {450, 320}, Qt::MiddleButton);
        zoom(canvas, {400, 300}, 120);
        const auto screen = canvas.viewport().toScreen({-200, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, screen.toPoint());
        QTest::keyClick(&canvas, Qt::Key_Return);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(QPointF(canvas.document().shapes()[0].path.elementAt(0)), QPointF(-300, 200));
        QVERIFY(QLineF(QPointF(canvas.document().shapes()[0].path.elementAt(1)), QPointF(-200, 100)).length() < 1e-7);
    }
    void penAxisAlignedMoveResizeDelete() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {100, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 100});
        QTest::keyClick(&canvas, Qt::Key_Return);
        canvas.setTool(Tool::Select);
        drag(canvas, {150, 100}, {170, 130});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()), QRectF(120, 130, 100, 0));
        drag(canvas, {220, 130}, {270, 150});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()), QRectF(120, 130, 150, 0));
        QTest::keyClick(&canvas, Qt::Key_Delete);
        QVERIFY(canvas.document().shapes().empty());
        canvas.setTool(Tool::Pen);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 100});
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 200});
        QTest::keyClick(&canvas, Qt::Key_Return);
        canvas.setTool(Tool::Select);
        drag(canvas, {300, 150}, {320, 180});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()), QRectF(320, 130, 0, 100));
        drag(canvas, {320, 230}, {350, 280});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()), QRectF(320, 130, 0, 150));
        QTest::keyClick(&canvas, Qt::Key_Backspace);
        QVERIFY(canvas.document().shapes().empty());
    }
    void drawingHoverAndCleanup() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        movePointer(canvas, {103, 102}, Qt::NoButton);
        QVERIFY(!markerAt(canvas, {100, 100}));
        canvas.setGridSnapEnabled(true);
        QVERIFY(markerAt(canvas, {100, 100}));
        QVERIFY(canvas.document().shapes().empty());
        QVERIFY(!canvas.document().selection());
        movePointer(canvas, {113, 113}, Qt::NoButton);
        QVERIFY(!markerAt(canvas, {100, 100}));
        movePointer(canvas, {103, 102}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {100, 100}));
        canvas.setGridSnapEnabled(false);
        QVERIFY(!markerAt(canvas, {100, 100}));
        canvas.setGridSnapEnabled(true);
        QVERIFY(markerAt(canvas, {100, 100}));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&canvas, &leave);
        QVERIFY(!markerAt(canvas, {100, 100}));
        canvas.setGeometrySnapEnabled(true);
        QVERIFY(!markerAt(canvas, {100, 100}));
        movePointer(canvas, {103, 102}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {100, 100}));
        canvas.setTool(Tool::Ellipse);
        QVERIFY(!markerAt(canvas, {100, 100}));
        movePointer(canvas, {103, 102}, Qt::NoButton);
        QTest::keyPress(&canvas, Qt::Key_Space);
        QVERIFY(!markerAt(canvas, {100, 100}));
        QTest::keyRelease(&canvas, Qt::Key_Space);
        QVERIFY(markerAt(canvas, {100, 100}));
        QTest::mousePress(&canvas, Qt::MiddleButton, Qt::NoModifier, {103, 102});
        QVERIFY(!markerAt(canvas, {100, 100}));
        QTest::mouseRelease(&canvas, Qt::MiddleButton, Qt::NoModifier, {103, 102});
        QVERIFY(markerAt(canvas, {100, 100}));
        QFocusEvent lost(QEvent::FocusOut);
        QApplication::sendEvent(&canvas, &lost);
        QVERIFY(!markerAt(canvas, {100, 100}));
        movePointer(canvas, {103, 102}, Qt::NoButton);
        drag(canvas, {103, 102}, {198, 199});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(100, 100, 100, 100));
        QVERIFY(markerAt(canvas, {200, 200}));
    }
    void geometryHoverCompetitionAndZoom() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {112, 100}, {180, 200});
        const auto original = canvas.document().shapes()[0].path;
        const auto selected = canvas.document().selection();
        canvas.setGeometrySnapEnabled(true);
        movePointer(canvas, {115, 125}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {112, 125}));
        canvas.setGridSnapEnabled(true);
        QVERIFY(markerAt(canvas, {112, 125}));
        QCOMPARE(canvas.document().shapes()[0].path, original);
        QCOMPARE(canvas.document().selection(), selected);
        drag(canvas, {115, 125}, {225, 225});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect()),
                 QRectF(112, 125, 113, 100));
        movePointer(canvas, {98, 248}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {100, 250}));
        canvas.setGridSnapEnabled(false);
        zoom(canvas, {400, 300}, 120);
        const auto target = canvas.viewport().toScreen(canvas.document().shapes()[0].path.boundingRect().topLeft());
        movePointer(canvas, (target + QPointF(-7, 0)).toPoint(), Qt::NoButton);
        QVERIFY(markerAt(canvas, target.toPoint()));
        movePointer(canvas, (target + QPointF(-10, 0)).toPoint(), Qt::NoButton);
        QVERIFY(!markerAt(canvas, target.toPoint()));
    }
    void selectedHoverTargets() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {102, 103}, {198, 197});
        const auto original = canvas.document().shapes()[0].path;
        canvas.setTool(Tool::Select);
        canvas.setGeometrySnapEnabled(true);
        movePointer(canvas, {150, 150}, Qt::NoButton);
        QVERIFY(!markerAt(canvas, {150, 150}));
        canvas.setGridSnapEnabled(true);
        movePointer(canvas, {202, 199}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {200, 200}));
        QCOMPARE(canvas.document().shapes()[0].path, original);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {202, 199});
        QCOMPARE(canvas.document().shapes()[0].path, original);
        movePointer(canvas, {202, 150}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {200, 150}));
        movePointer(canvas, {150, 140}, Qt::NoButton);
        QVERIFY(markerAt(canvas, {150, 150}));
        QCOMPARE(canvas.document().shapes()[0].path, original);
        movePointer(canvas, {500, 400}, Qt::NoButton);
        QVERIFY(!markerAt(canvas, {500, 400}));
        QVERIFY(!markerAt(canvas, {150, 150}));
        drag(canvas, {202, 199}, {226, 227});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(102, 103, 123, 122));
    }
    void snapToolbarToggles() {
        Canvas canvas;
        prepare(canvas);
        QToolBar bar;
        addSnapControls(bar, canvas);
        auto *grid = bar.findChild<QAction *>("snapToGrid");
        auto *geometry = bar.findChild<QAction *>("snapToGeometry");
        QVERIFY(grid && geometry);
        QVERIFY(grid->isCheckable() && geometry->isCheckable());
        QVERIFY(!grid->isChecked() && !geometry->isChecked());
        bar.show();
        QTest::keyClick(bar.widgetForAction(grid), Qt::Key_Space);
        QVERIFY(canvas.gridSnapEnabled() && !canvas.geometrySnapEnabled());
        geometry->trigger();
        QVERIFY(canvas.gridSnapEnabled() && canvas.geometrySnapEnabled());
        grid->trigger();
        QVERIFY(!canvas.gridSnapEnabled() && canvas.geometrySnapEnabled());
        geometry->trigger();
        QVERIFY(!canvas.gridSnapEnabled() && !canvas.geometrySnapEnabled());
        canvas.setGridSnapEnabled(true);
        QVERIFY(grid->isChecked());
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {103, 102}, {198, 199});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(100, 100, 100, 100));
        canvas.setTool(Tool::Select);
        const auto original = canvas.document().shapes()[0].path;
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {150, 150});
        movePointer(canvas, {174, 174});
        grid->trigger();
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {174, 174});
        QCOMPARE(canvas.document().shapes()[0].path, original);
    }
    void gridDrawMoveResize() {
        Canvas canvas;
        prepare(canvas);
        canvas.setGridSnapEnabled(true);
        canvas.setTool(Tool::Ellipse);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {103, 102});
        movePointer(canvas, {198, 199});
        if (qEnvironmentVariableIsSet("FONTU_SNAP_PREVIEW"))
            QVERIFY(canvas.grab().save(qEnvironmentVariable("FONTU_SNAP_PREVIEW")));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {198, 199});
        canvas.setTool(Tool::Select);
        drag(canvas, {141, 143}, {164, 166});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(125, 125, 100, 100));
        // Offset from the handle center is retained, including a click without motion.
        const auto before = canvas.document().shapes()[0].path;
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {229, 228});
        QCOMPARE(canvas.document().shapes()[0].path, before);
        drag(canvas, {229, 228}, {251, 250});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(125, 125, 125, 125));
        QVERIFY(canvas.document().shapes()[0].path.elementAt(1).isCurveTo());
        // Edge resize only changes its adjustable axis, regardless of pointer drift.
        drag(canvas, {250, 187}, {274, 198});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(125, 125, 150, 125));
    }
    void geometryDrawMoveResize() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {200, 200});
        canvas.setGeometrySnapEnabled(true);
        drag(canvas, {203, 129}, {300, 230});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect()),
                 QRectF(200, 129, 100, 101));
        canvas.setGeometrySnapEnabled(false);
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {350, 100}, {450, 200});
        canvas.setGeometrySnapEnabled(true);
        // Draw starts on the ellipse's actual curve, away from bounds snap points.
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {437, 113}, {500, 70});
        const auto curveBounds = canvas.viewport().transform().mapRect(canvas.document().shapes()[3].path.boundingRect());
        QVERIFY(std::abs(curveBounds.left() - 435.35) < 0.5);
        QVERIFY(std::abs(curveBounds.bottom() - 114.65) < 0.5);
        canvas.setGeometrySnapEnabled(false);
        drag(canvas, {250, 300}, {310, 360});
        canvas.setTool(Tool::Select);
        canvas.setGeometrySnapEnabled(true);
        // Moving snaps a shape corner to an outline, not the mouse grab point.
        drag(canvas, {267, 319}, {215, 210});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[4].path.boundingRect()),
                 QRectF(200, 191, 60, 60));
        // The other rectangle still attracts this shape to its left outline.
        drag(canvas, {226, 220}, {227, 221});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[4].path.boundingRect()),
                 QRectF(200, 192, 60, 60));
        drag(canvas, {260, 252}, {299, 229});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[4].path.boundingRect()),
                 QRectF(200, 192, 99, 38));
    }
    void bothModesChooseNearest() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {112, 100}, {180, 200});
        canvas.setGridSnapEnabled(true);
        canvas.setGeometrySnapEnabled(true);
        drag(canvas, {115, 125}, {225, 225});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect()),
                 QRectF(112, 125, 113, 100));
        // Grid intersection is closer than either the rectangle outline or its bounds snap points.
        drag(canvas, {98, 248}, {50, 300});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[2].path.boundingRect()),
                 QRectF(50, 250, 50, 50));
    }
    void geometryToleranceAfterZoom() {
        Canvas canvas;
        prepare(canvas);
        zoom(canvas, {400, 300}, 1200);
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {100, 100}, {300, 300});
        canvas.setGeometrySnapEnabled(true);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {307, 200}, {380, 250});
        auto bounds = canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect());
        QVERIFY(std::abs(bounds.left() - 300) < 1e-7);
        drag(canvas, {309, 150}, {370, 50});
        bounds = canvas.viewport().transform().mapRect(canvas.document().shapes()[2].path.boundingRect());
        QVERIFY(std::abs(bounds.left() - 309) < 1e-7);
    }
    void geometryExclusionAndCurveResize() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {350, 100}, {450, 200});
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 110}, {200, 160});
        canvas.setTool(Tool::Select);
        canvas.setGeometrySnapEnabled(true);
        drag(canvas, {140, 135}, {141, 136});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect()),
                 QRectF(101, 111, 100, 50));
        // The right handle's fixed y is 136; the pointer can drift vertically.
        drag(canvas, {201, 136}, {447, 155});
        auto screen = canvas.viewport().transform().mapRect(canvas.document().shapes()[1].path.boundingRect());
        QCOMPARE(screen.left(), 101.0);
        QCOMPARE(screen.top(), 111.0);
        QCOMPARE(screen.height(), 50.0);
        QVERIFY(std::abs(screen.right() - 447.99) < 0.5);
    }
    void snappingAfterZoomAndPan() {
        Canvas canvas;
        prepare(canvas);
        zoom(canvas, {400, 300}, -1200);
        drag(canvas, {400, 300}, {427, 319}, Qt::MiddleButton);
        canvas.setGridSnapEnabled(true);
        canvas.setTool(Tool::Rectangle);
        const double step = gridStep(canvas.viewport().zoom());
        const auto start = canvas.viewport().toScreen({-step, step});
        const auto end = canvas.viewport().toScreen({step, -step});
        movePointer(canvas, (start + QPointF(2, 2)).toPoint(), Qt::NoButton);
        QVERIFY(markerAt(canvas, start.toPoint()));
        QVERIFY(canvas.document().shapes().empty());
        drag(canvas, (start + QPointF(2, 2)).toPoint(), (end + QPointF(-2, -2)).toPoint());
        QVERIFY(QLineF(canvas.document().shapes()[0].path.boundingRect().topLeft(), QPointF(-step, -step)).length() < 1e-7);
        QVERIFY(std::abs(canvas.document().shapes()[0].path.boundingRect().width() - 2 * step) < 1e-7);
    }
    void drawing() {
        Canvas canvas;
        prepare(canvas);
        QTest::keyClick(&canvas, Qt::Key_R);
        drag(canvas, {150, 200}, {50, 100});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect(), QRectF(-350, 100, 100, 100));
        QTest::keyClick(&canvas, Qt::Key_E);
        drag(canvas, {200, 150}, {320, 230});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        const auto &ellipse = canvas.document().shapes()[1].path;
        QCOMPARE(ellipse.boundingRect(), QRectF(-200, 70, 120, 80));
        QVERIFY(ellipse.elementAt(1).isCurveTo());
        drag(canvas, {400, 300}, {401, 301});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {350, 350});
        movePointer(canvas, {450, 450});
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {450, 450});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(2));
        // Exercise painting with both a selected curve and the grid.
        const auto rendered = canvas.grab();
        QVERIFY(!rendered.isNull());
        if (qEnvironmentVariableIsSet("FONTU_TEST_PREVIEW"))
            QVERIFY(rendered.save(qEnvironmentVariable("FONTU_TEST_PREVIEW")));
    }
    void selectionMoveDelete() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {250, 250});
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {150, 150}, {300, 300});
        const auto ellipseId = canvas.document().shapes()[1].id;
        canvas.setTool(Tool::Select);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        QCOMPARE(canvas.document().selection(), std::optional<ShapeId>(ellipseId));
        drag(canvas, {210, 210}, {260, 230});
        QCOMPARE(canvas.document().find(ellipseId)->path.boundingRect(), QRectF(-200, -20, 150, 150));
        QTest::keyClick(&canvas, Qt::Key_Delete);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QVERIFY(!canvas.document().selection());
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {175, 175});
        QVERIFY(canvas.document().selection());
        QTest::keyClick(&canvas, Qt::Key_Backspace);
        QVERIFY(canvas.document().shapes().empty());
        QTest::keyClick(&canvas, Qt::Key_Delete);
        QVERIFY(canvas.document().shapes().empty());
    }
    void resizeHandles_data() {
        QTest::addColumn<QPoint>("start");
        QTest::addColumn<QPoint>("end");
        QTest::addColumn<QRectF>("expectedScreen");
        QTest::newRow("top left") << QPoint(100, 100) << QPoint(80, 70) << QRectF(80, 70, 220, 230);
        QTest::newRow("top") << QPoint(200, 100) << QPoint(200, 70) << QRectF(100, 70, 200, 230);
        QTest::newRow("top right") << QPoint(300, 100) << QPoint(330, 70) << QRectF(100, 70, 230, 230);
        QTest::newRow("right") << QPoint(300, 200) << QPoint(330, 200) << QRectF(100, 100, 230, 200);
        QTest::newRow("bottom right") << QPoint(300, 300) << QPoint(330, 340) << QRectF(100, 100, 230, 240);
        QTest::newRow("bottom") << QPoint(200, 300) << QPoint(200, 340) << QRectF(100, 100, 200, 240);
        QTest::newRow("bottom left") << QPoint(100, 300) << QPoint(70, 340) << QRectF(70, 100, 230, 240);
        QTest::newRow("left") << QPoint(100, 200) << QPoint(70, 200) << QRectF(70, 100, 230, 200);
        QTest::newRow("cross anchor") << QPoint(100, 100) << QPoint(350, 350) << QRectF(300, 300, 50, 50);
    }
    void resizeHandles() {
        QFETCH(QPoint, start);
        QFETCH(QPoint, end);
        QFETCH(QRectF, expectedScreen);
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Ellipse);
        drag(canvas, {100, 100}, {300, 300});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.setTool(Tool::Select);
        drag(canvas, start, end);
        const auto &path = canvas.document().shapes()[0].path;
        QCOMPARE(canvas.viewport().transform().mapRect(path.boundingRect()), expectedScreen);
        QVERIFY(path.elementAt(1).isCurveTo());
    }
    void offCenterHandlePress() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {300, 300});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.setTool(Tool::Select);
        const auto original = canvas.document().shapes()[0].path;
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {304, 303});
        QCOMPARE(canvas.document().shapes()[0].path, original);
        drag(canvas, {304, 303}, {324, 323});
        QCOMPARE(canvas.viewport().transform().mapRect(canvas.document().shapes()[0].path.boundingRect()),
                 QRectF(100, 100, 220, 220));
    }
    void navigationAndEditing() {
        Canvas canvas;
        prepare(canvas);
        const QPoint anchor(200, 150);
        const auto anchorDoc = canvas.viewport().toDocument(anchor);
        zoom(canvas, anchor, 120);
        QCOMPARE(canvas.viewport().zoom(), 1.2);
        QVERIFY(QLineF(anchor, canvas.viewport().toScreen(anchorDoc)).length() < 1e-8);
        drag(canvas, {400, 300}, {460, 340}, Qt::MiddleButton);
        QVERIFY(QLineF(canvas.viewport().toScreen(anchorDoc), QPointF(260, 190)).length() < 1e-8);
        QTest::keyPress(&canvas, Qt::Key_Space);
        drag(canvas, {500, 350}, {520, 370});
        QTest::keyRelease(&canvas, Qt::Key_Space);
        QVERIFY(QLineF(canvas.viewport().toScreen(anchorDoc), QPointF(280, 210)).length() < 1e-8);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {220, 220});
        const auto &initial = canvas.document().shapes()[0].path;
        QCOMPARE(initial.boundingRect().size(), QSizeF(100, 100));
        const auto original = initial.boundingRect();
        canvas.setTool(Tool::Select);
        drag(canvas, {160, 160}, {184, 184});
        auto moved = canvas.document().shapes()[0].path.boundingRect();
        QVERIFY(QLineF(moved.topLeft(), original.topLeft() + QPointF(20, -20)).length() < 1e-8);
        drag(canvas, {244, 244}, {304, 304});
        QCOMPARE(canvas.document().shapes()[0].path.boundingRect().size(), QSizeF(150, 150));
        const auto afterResize = canvas.document().shapes()[0].path;
        QTest::keyClick(&canvas, Qt::Key_0);
        QCOMPARE(canvas.viewport().zoom(), 0.52);
        QVERIFY(canvas.rect().contains(canvas.viewport().transform().mapRect(canvas.document().definition().bounds).toAlignedRect()));
        QCOMPARE(canvas.document().shapes()[0].path, afterResize);
    }
    void focusLossCancelsEdit() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {300, 300});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.setTool(Tool::Select);
        const auto original = canvas.document().shapes()[0].path;
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        movePointer(canvas, {250, 250});
        QVERIFY(canvas.document().shapes()[0].path != original);
        QFocusEvent lostFocus(QEvent::FocusOut);
        QApplication::sendEvent(&canvas, &lostFocus);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {250, 250});
        QCOMPARE(canvas.document().shapes()[0].path, original);
    }
    void cancellation() {
        Canvas canvas;
        prepare(canvas);
        canvas.setTool(Tool::Rectangle);
        drag(canvas, {100, 100}, {300, 300});
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        canvas.setTool(Tool::Select);
        const auto original = canvas.document().shapes()[0].path;
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {200, 200});
        movePointer(canvas, {250, 250});
        QVERIFY(canvas.document().shapes()[0].path != original);
        QTest::keyClick(&canvas, Qt::Key_Escape);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {250, 250});
        QCOMPARE(canvas.document().shapes()[0].path, original);
        QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, {300, 300});
        movePointer(canvas, {350, 350});
        canvas.setTool(Tool::Ellipse);
        QCOMPARE(canvas.document().shapes()[0].path, original);
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, {350, 350});
        canvas.setTool(Tool::Select);
        QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, {600, 450});
        QVERIFY(!canvas.document().selection());
    }
};
QTEST_MAIN(CanvasTests)
#include "canvas_tests.moc"
