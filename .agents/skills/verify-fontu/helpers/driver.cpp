// Execute the unchanged production entry point; register observation after QApplication starts.
#define main fontuProductionMain
#include FONTU_MAIN
#undef main
#include <QTimer>
#include <QTest>
#include <QMenu>
#include <QMenuBar>
#include <QToolButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QListWidget>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <stdexcept>

namespace {
QMainWindow *window;
fontu::Canvas *canvas;
QString output, feature;
int step = 0;
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
QRectF bounds(int index) {
    return canvas->viewport().transform().mapRect(canvas->document().shapes().at(index).path.boundingRect());
}
void record(const QString &action) {
    QApplication::processEvents();
    QString stem = QString("%1").arg(++step, 2, 10, QChar('0'));
    require(window->grab().save(output + "/" + stem + ".png"), "screenshot failed");
    QJsonArray shapes;
    for (const auto &shape : canvas->document().shapes()) {
        auto r = canvas->viewport().transform().mapRect(shape.path.boundingRect());
        QJsonArray elements;
        for (int i = 0; i < shape.path.elementCount(); ++i) {
            const auto element = shape.path.elementAt(i);
            elements.append(QJsonObject{{"type",int(element.type)},{"documentPoint",QJsonArray{element.x,element.y}}});
        }
        shapes.append(QJsonObject{{"id",double(shape.id)},{"screenBounds", QJsonArray{r.x(),r.y(),r.width(),r.height()}},
                                {"curve",shape.path.elementAt(1).isCurveTo()},{"paint",shape.paint==fontu::ShapePaint::Stroke ? "stroke" : "filled"},{"elements",elements}});
    }
    QJsonObject state{{"action",action},{"feature",feature},{"backend",QApplication::platformName()},
        {"inputBoundary","Qt events to unchanged production window, not OS input"},
        {"window",window->windowTitle()},{"modified",window->isWindowModified()},{"shapes",shapes},{"tool",int(canvas->tool())},
        {"selected",canvas->document().selection() ? double(*canvas->document().selection()) : -1},
        {"grid",canvas->gridSnapEnabled()},{"geometry",canvas->geometrySnapEnabled()},
        {"canUndo",canvas->canUndo()},{"canRedo",canvas->canRedo()},{"zoom",canvas->viewport().zoom()},{"screenshot",stem + ".png"}};
    state.insert("activeGlyphId", double(canvas->activeGlyphId().value));
    state.insert("controlsHint", window->statusBar()->currentMessage());
    for(auto *menu : window->findChildren<QMenu *>()) {
        if(menu->isVisible()) {
            QString popup=stem + "-menu.png";
            require(menu->grab().save(output + "/" + popup),"menu screenshot failed");
            state.insert("popupScreenshot",popup);
            QJsonArray options;
            for(auto *action : menu->actions()) options.append(QJsonObject{{"text",action->text()},{"checked",action->isChecked()}});
            state.insert("menuOptions",options);
        }
    }
    QFile file(output + "/actions.jsonl");
    require(file.open(QIODevice::WriteOnly|QIODevice::Append), "log open failed");
    file.write(QJsonDocument(state).toJson(QJsonDocument::Compact) + '\n');
}
void toolState(fontu::Tool expected) {
    require(canvas->tool()==expected,"canvas tool did not change");
    const QString names[]={"Select (V)","Rectangle (R)","Ellipse (E)","Pen (P)"};
    for(auto *action : window->findChild<QToolBar *>()->actions()) {
        for(int i=0;i<4;++i) if(action->text()==names[i])
            require(action->isChecked()==(i==int(expected)),"toolbar tool state mismatch");
    }
}
void toolbar(const QString &text) {
    auto *bar = window->findChild<QToolBar *>();
    QAction *action = nullptr;
    for (auto *candidate : bar->actions()) if (candidate->text() == text) action = candidate;
    require(action, "toolbar action missing");
    auto *button = bar->widgetForAction(action);
    require(button && button->isVisible(), "toolbar button unavailable");
    QTest::mouseClick(button, Qt::LeftButton);
    require(action->isChecked() || !action->isCheckable(), "toolbar selection failed");
    if(text=="Select (V)") toolState(fontu::Tool::Select);
    if(text=="Rectangle (R)") toolState(fontu::Tool::Rectangle);
    if(text=="Ellipse (E)") toolState(fontu::Tool::Ellipse);
    if(text=="Pen (P)") toolState(fontu::Tool::Pen);
    record("Toolbar click: " + text);
}
void key(Qt::Key key, const QString &label) {
    QTest::keyClick(canvas,key);
    if(key==Qt::Key_V) toolState(fontu::Tool::Select);
    if(key==Qt::Key_R) toolState(fontu::Tool::Rectangle);
    if(key==Qt::Key_E) toolState(fontu::Tool::Ellipse);
    if(key==Qt::Key_P) toolState(fontu::Tool::Pen);
    record("Canvas key: " + label);
}
void move(QPoint point, Qt::MouseButtons buttons) {
    QMouseEvent event(QEvent::MouseMove,point,canvas->mapToGlobal(point),Qt::NoButton,buttons,Qt::NoModifier);
    QApplication::sendEvent(canvas,&event);
}
void hover(QPoint point, QPoint target, const QString &label) {
    const auto count = canvas->document().shapes().size();
    const auto selection = canvas->document().selection();
    QPainterPath selectedPath;
    if (selection) selectedPath = canvas->document().find(*selection)->path;
    move(point, Qt::NoButton);
    const auto image = canvas->grab().toImage();
    int amber = 0;
    for (int y = target.y() - 7; y <= target.y() + 7; ++y)
        for (int x = target.x() - 7; x <= target.x() + 7; ++x) {
            const auto color = image.pixelColor(x, y);
            if (color.red() > 170 && color.green() > 70 && color.green() < 165 && color.blue() < 90) ++amber;
        }
    require(amber >= 12, "hover snap circle missing at expected target");
    require(canvas->document().shapes().size() == count && canvas->document().selection() == selection,
            "hover changed document or selection");
    if (selection) require(canvas->document().find(*selection)->path == selectedPath, "hover changed selected path");
    record(label + QString(" hover (%1,%2), amber target (%3,%4), no document change")
           .arg(point.x()).arg(point.y()).arg(target.x()).arg(target.y()));
}
void drag(QPoint from, QPoint to, Qt::MouseButton button=Qt::LeftButton) {
    QTest::mousePress(canvas,button,Qt::NoModifier,from);
    move(to,button);
    record(QString("Drag preview (%1,%2) to (%3,%4)").arg(from.x()).arg(from.y()).arg(to.x()).arg(to.y()));
    QTest::mouseRelease(canvas,button,Qt::NoModifier,to);
    record("Drag release");
}
void snap(const QString &name) {
    auto *bar=window->findChild<QToolBar *>();
    auto *action=bar->findChild<QAction *>(name);
    require(action && bar->actions().contains(action),"snap toolbar action missing");
    auto *button=bar->widgetForAction(action);
    require(button && button->isVisible() && bar->rect().contains(button->geometry()),"snap toolbar button unavailable");
    const bool previous=action->isChecked();
    QTest::mouseClick(button,Qt::LeftButton);
    require(action->isChecked()!=previous,"snap toggle failed");
    require(canvas->gridSnapEnabled()==bar->findChild<QAction *>("snapToGrid")->isChecked()
            && canvas->geometrySnapEnabled()==bar->findChild<QAction *>("snapToGeometry")->isChecked(),
            "snap toolbar state disagrees with canvas");
    record("Snap toolbar click: " + action->text());
}
void penClick(QPoint point) {
    QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, point);
    record(QString("Pen anchor click (%1,%2)").arg(point.x()).arg(point.y()));
}
void pathMatches(int index, const QPainterPath &expected, const char *message, fontu::ShapePaint paint = fontu::ShapePaint::Stroke) {
    require(canvas->document().shapes().at(index).paint==paint,"pen path paint mismatch");
    const auto &actual = canvas->document().shapes().at(index).path;
    require(actual.elementCount() == expected.elementCount(), message);
    for (int i = 0; i < expected.elementCount(); ++i) {
        const auto a = actual.elementAt(i), e = expected.elementAt(i);
        require(a.type == e.type && std::abs(a.x-e.x)<1e-7 && std::abs(a.y-e.y)<1e-7, message);
    }
}
void legacyView() {
    const auto oldSize=window->size();
    window->resize(window->width()+1160-canvas->width(),window->height()+1080-canvas->height());
    QApplication::processEvents();
    toolbar("Reset view (0)");
    window->resize(oldSize); QApplication::processEvents();
    drag({400,300},{400,50},Qt::MiddleButton);
    require(canvas->viewport().zoom()==1,"legacy recipe 100 percent setup failed");
}
void pen() {
    toolbar("Pen (P)");
    penClick({100,100}); penClick({220,100});
    require(canvas->document().shapes().empty(), "pen inserted before Enter");
    QPainterPath line(canvas->viewport().toDocument({100,100}));
    line.lineTo(canvas->viewport().toDocument({220,100}));
    key(Qt::Key_Return,"Enter");
    require(canvas->document().shapes().size()==1, "horizontal pen line not committed");
    pathMatches(0,line,"horizontal pen line mismatch");
    key(Qt::Key_V,"V"); key(Qt::Key_P,"P");
    penClick({300,100}); penClick({350,160}); penClick({400,100});
    QPainterPath corners(canvas->viewport().toDocument({300,100}));
    corners.lineTo(canvas->viewport().toDocument({350,160}));
    corners.lineTo(canvas->viewport().toDocument({400,100}));
    key(Qt::Key_Return,"Enter");
    require(canvas->document().shapes().size()==2,"corner pen path not committed");
    pathMatches(1,corners,"corner pen path mismatch");
    penClick({400,250}); drag({500,250},{500,290});
    require(canvas->document().shapes().size()==2,"dragged pen anchor committed early");
    penClick({600,250}); penClick({700,300});
    QPainterPath mixed(canvas->viewport().toDocument({400,250}));
    mixed.cubicTo(canvas->viewport().toDocument({400,250}),canvas->viewport().toDocument({500,210}),canvas->viewport().toDocument({500,250}));
    mixed.cubicTo(canvas->viewport().toDocument({500,290}),canvas->viewport().toDocument({600,250}),canvas->viewport().toDocument({600,250}));
    mixed.lineTo(canvas->viewport().toDocument({700,300}));
    key(Qt::Key_Return,"Enter");
    require(canvas->document().shapes().size()==3,"mixed pen path not committed");
    pathMatches(2,mixed,"mixed pen cubic controls mismatch");
    record("Mixed pen exact cubic controls and line segment verified");
    penClick({750,100}); penClick({850,150}); key(Qt::Key_Escape,"Escape"); key(Qt::Key_Return,"Enter after Escape");
    require(canvas->document().shapes().size()==3,"cancelled pen path committed");
    penClick({100,350}); penClick({220,350});
    QPainterPath navigated(canvas->viewport().toDocument({100,350}));
    navigated.lineTo(canvas->viewport().toDocument({220,350}));
    drag({800,450},{830,470},Qt::MiddleButton);
    QWheelEvent wheel(QPointF(200,350),canvas->mapToGlobal(QPoint(200,350)),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(canvas,&wheel); record("Pen draft after middle pan and wheel zoom");
    key(Qt::Key_Return,"Enter after pan and zoom");
    require(canvas->document().shapes().size()==4,"navigation lost pen draft");
    pathMatches(3,navigated,"navigation changed pen draft document points");
    key(Qt::Key_0,"0"); legacyView();
    snap("snapToGrid");
    const auto gridStart=canvas->viewport().toScreen({-300,-150}).toPoint();
    const auto gridEnd=canvas->viewport().toScreen({-200,-150}).toPoint();
    const auto gridHandle=canvas->viewport().toScreen({-200,-100}).toPoint();
    penClick(gridStart+QPoint(3,2)); drag(gridEnd+QPoint(3,2),gridHandle+QPoint(3,2));
    QPainterPath gridPath(QPointF(-300,-150));
    gridPath.cubicTo({-300,-150},{-200,-200},{-200,-150});
    key(Qt::Key_Return,"Enter grid-snapped pen");
    require(canvas->document().shapes().size()==5,"grid pen not committed");
    pathMatches(4,gridPath,"grid pen anchor or tangent snap mismatch");
    snap("snapToGrid"); snap("snapToGeometry");
    const auto geometryStart=canvas->viewport().toScreen(QPointF(line.elementAt(0).x,line.elementAt(0).y)).toPoint();
    const auto geometryEnd=canvas->viewport().toScreen(QPointF(line.elementAt(1).x,line.elementAt(1).y)).toPoint();
    penClick(geometryStart+QPoint(-2,2)); penClick(geometryEnd+QPoint(2,2));
    key(Qt::Key_Return,"Enter geometry-snapped pen");
    require(canvas->document().shapes().size()==6,"geometry pen not committed");
    pathMatches(5,line,"pen geometry snap missed committed path endpoints");
    penClick({800,350}); penClick({900,400}); snap("snapToGeometry"); key(Qt::Key_Return,"Enter after Snap toolbar toggle");
    require(canvas->document().shapes().size()==6,"Snap toolbar toggle failed to cancel pen draft");
    toolbar("Select (V)");
    const auto cornerStart=corners.elementAt(0), cornerEnd=corners.elementAt(1);
    const auto selectPoint=canvas->viewport().toScreen({(cornerStart.x+cornerEnd.x)/2,(cornerStart.y+cornerEnd.y)/2}).toPoint();
    QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,selectPoint);
    require(canvas->document().selection()==canvas->document().shapes()[1].id,"pen stroke selection failed");
    record("Select committed pen stroke");
    drag(selectPoint,selectPoint+QPoint(30,20));
    const auto delta=canvas->viewport().toDocument(selectPoint+QPoint(30,20))-canvas->viewport().toDocument(selectPoint);
    pathMatches(1,QTransform::fromTranslate(delta.x(),delta.y()).map(corners),"pen stroke move mismatch");
    key(Qt::Key_Delete,"Delete selected pen");
    require(canvas->document().shapes().size()==5 && !canvas->document().selection(),"pen stroke delete failed");

    toolbar("Pen (P)");
    penClick({100,450}); penClick({220,450}); penClick({100,450});
    require(canvas->document().shapes().size()==5,"two-anchor draft closed prematurely");
    key(Qt::Key_Escape,"Escape minimum-anchor closure draft");
    penClick({100,450}); penClick({240,450}); penClick({180,580});
    QPainterPath triangle(canvas->viewport().toDocument({100,450}));
    triangle.lineTo(canvas->viewport().toDocument({240,450}));
    triangle.lineTo(canvas->viewport().toDocument({180,580}));
    triangle.closeSubpath();
    move({105,450},Qt::NoButton); record("Hover first anchor: closing ring and filled triangle preview");
    require(canvas->document().shapes().size()==5,"closure hover inserted a shape");
    require(canvas->cursor().shape()==Qt::PointingHandCursor,"closing hover cursor missing");
    const auto closingImage=canvas->grab().toImage();
    int blueRingPixels=0;
    for(int y=441;y<=459;++y) for(int x=91;x<=109;++x) {
        const auto color=closingImage.pixelColor(x,y);
        const auto radius=QLineF(QPointF(100,450),QPointF(x,y)).length();
        if(radius>=6 && radius<=9 && color.red()<100 && color.green()<160 && color.blue()>180) ++blueRingPixels;
    }
    require(blueRingPixels>=15,"blue closing ring missing");
    const auto previewFill=closingImage.pixelColor(177,489);
    require(previewFill.red()<230 && previewFill.blue()>235,"closed hover preview lacks interior fill");
    QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,{105,450});
    require(canvas->document().shapes().size()==6,"first-anchor press did not commit immediately");
    pathMatches(5,triangle,"closed triangle elements mismatch",fontu::ShapePaint::Filled);
    require(canvas->document().selection()==canvas->document().shapes()[5].id,"closed triangle was not selected");
    record("First-anchor press commits filled triangle and selects it");
    QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,{105,450});
    key(Qt::Key_Return,"Enter after click closure");
    require(canvas->document().shapes().size()==6,"closure release or Enter committed another path");
    key(Qt::Key_V,"V"); key(Qt::Key_Escape,"Escape clear filled selection");
    QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,{180,490});
    require(canvas->document().selection()==canvas->document().shapes()[5].id,"closed contour interior selection failed");
    record("Select closed Pen triangle through its filled interior");
    key(Qt::Key_P,"P");
    penClick({450,420}); penClick({600,420}); drag({600,550},{630,550});
    QPainterPath curved(canvas->viewport().toDocument({450,420}));
    curved.lineTo(canvas->viewport().toDocument({600,420}));
    curved.cubicTo(canvas->viewport().toDocument({600,420}),canvas->viewport().toDocument({570,550}),canvas->viewport().toDocument({600,550}));
    curved.cubicTo(canvas->viewport().toDocument({630,550}),canvas->viewport().toDocument({450,420}),canvas->viewport().toDocument({450,420}));
    curved.closeSubpath();
    move({450,420},Qt::NoButton); record("Hover first anchor: filled curved closing preview");
    penClick({450,420});
    require(canvas->document().shapes().size()==7,"curved first-anchor closure failed");
    pathMatches(6,curved,"closing cubic lost final outgoing tangent",fontu::ShapePaint::Filled);
    require(canvas->document().selection()==canvas->document().shapes()[6].id,"closed curve was not selected");
    record("Closed Pen exact closing cubic controls verified");
    penClick({750,450}); penClick({900,450}); penClick({840,580});
    QPainterPath navigatedClosed(canvas->viewport().toDocument({750,450}));
    navigatedClosed.lineTo(canvas->viewport().toDocument({900,450}));
    navigatedClosed.lineTo(canvas->viewport().toDocument({840,580}));
    navigatedClosed.closeSubpath();
    drag({800,300},{820,310},Qt::MiddleButton);
    QWheelEvent closingWheel(QPointF(800,500),canvas->mapToGlobal(QPoint(800,500)),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(canvas,&closingWheel);
    const auto first=navigatedClosed.elementAt(0);
    const auto closingScreen=canvas->viewport().toScreen({first.x,first.y}).toPoint();
    move(closingScreen+QPoint(7,0),Qt::NoButton); record("Closing hover at seven screen pixels after pan and zoom");
    penClick(closingScreen+QPoint(7,0));
    require(canvas->document().shapes().size()==8,"screen-radius closure after navigation failed");
    pathMatches(7,navigatedClosed,"navigation changed closed contour document points",fontu::ShapePaint::Filled);
    record("Pan and zoom preserve click closure and exact contour coordinates");
    penClick({750,150}); penClick({900,150}); penClick({840,250}); penClick({759,150});
    require(canvas->document().shapes().size()==8,"nine-screen-pixel click closed outside radius");
    key(Qt::Key_Escape,"Escape outside-radius closure draft");
}
QAction *historyAction(const QString &text) {
    for (auto *action : window->findChild<QToolBar *>()->actions())
        if (action->text()==text) return action;
    throw std::runtime_error("history action missing");
}
void historyState(bool undo, bool redo) {
    require(historyAction("Undo")->isEnabled()==undo && historyAction("Redo")->isEnabled()==redo,
            "history button enabled state mismatch");
}
void historyShortcut(QKeySequence::StandardKey standard) {
    const auto bindings=QKeySequence::keyBindings(standard);
    require(!bindings.isEmpty(), "standard binding missing");
    for (const auto &binding : bindings) {
        QTest::keySequence(canvas,binding);
        record("Standard shortcut " + binding.toString(QKeySequence::NativeText));
        if (standard==QKeySequence::Undo) {
            require(canvas->document().shapes().empty(),"undo shortcut did not remove shape");
            toolbar("Redo");
        } else {
            require(canvas->document().shapes().size()==1,"redo shortcut did not restore shape");
            toolbar("Undo");
        }
    }
}
void history() {
    historyState(false,false);
    toolbar("Undo"); toolbar("Redo");
    require(canvas->document().shapes().empty(),"disabled history button changed document");
    toolbar("Rectangle (R)"); drag({100,100},{220,200});
    const auto original=canvas->document().shapes()[0];
    historyState(true,false);
    historyShortcut(QKeySequence::Undo);
    toolbar("Undo"); historyState(false,true);
    historyShortcut(QKeySequence::Redo);
    toolbar("Redo");
    require(canvas->document().shapes()[0].id==original.id && canvas->document().shapes()[0].path==original.path,
            "history failed to preserve shape identity/path");
    toolbar("Select (V)"); drag({150,150},{180,170});
    drag({250,220},{280,240});
    key(Qt::Key_Delete,"Delete");
    toolbar("Undo"); require(bounds(0)==QRectF(130,120,150,120),"undo delete mismatch");
    toolbar("Undo"); require(bounds(0)==QRectF(130,120,120,100),"undo resize mismatch");
    toolbar("Undo"); require(bounds(0)==QRectF(100,100,120,100),"undo move mismatch");
    drag({150,150},{150,150}); historyState(true,true);
    toolbar("Reset view (0)"); historyState(true,true); legacyView();
    toolbar("Redo"); require(bounds(0)==QRectF(130,120,120,100),"redo move mismatch");
    toolbar("Redo"); require(bounds(0)==QRectF(130,120,150,120),"redo resize mismatch");
    toolbar("Redo"); require(canvas->document().shapes().empty(),"redo delete mismatch");
    toolbar("Undo");
    toolbar("Ellipse (E)"); drag({400,100},{500,200}); historyState(true,false);
    toolbar("Undo"); toolbar("Redo");
    require(bounds(1)==QRectF(400,100,100,100),"new branch redo mismatch");
    toolbar("Pen (P)"); penClick({600,100}); drag({720,160},{750,180}); penClick({820,210});
    historyState(true,false);
    toolbar("Undo"); historyState(true,true);
    toolbar("Redo"); historyState(true,false);
    QTest::keySequence(canvas,QKeySequence(QKeySequence::Undo));
    record("Standard Undo removes final draft vertex");
    toolbar("Undo"); toolbar("Undo"); historyState(false,true);
    toolbar("Undo");
    QTest::keySequence(canvas,QKeySequence(QKeySequence::Undo));
    require(canvas->document().shapes().size()==2,"zero-vertex draft traversed document history");
    record("Zero-vertex draft blocks document Undo");
    toolbar("Redo");
    QTest::keySequence(canvas,QKeySequence(QKeySequence::Redo));
    record("Standard Redo restores curved draft vertex");
    move({840,240},Qt::NoButton); historyState(true,true);
    key(Qt::Key_Return,"Enter commits restored draft prefix");
    QPainterPath curved(canvas->viewport().toDocument({600,100}));
    curved.cubicTo(canvas->viewport().toDocument({600,100}),canvas->viewport().toDocument({690,140}),canvas->viewport().toDocument({720,160}));
    pathMatches(2,curved,"vertex history changed curved prefix");
    historyState(true,false);
    const auto stroke=canvas->document().shapes()[2];
    toolbar("Undo"); toolbar("Redo");
    require(canvas->document().shapes()[2].path==stroke.path && canvas->document().shapes()[2].paint==fontu::ShapePaint::Stroke,"pen redo mismatch");
    QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,{800,100}); move({860,180},Qt::LeftButton);
    QTest::keySequence(canvas,QKeySequence(QKeySequence::Undo));
    QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,{860,180});
    key(Qt::Key_Return,"Enter after Undo removes held vertex");
    require(canvas->document().shapes().size()==3,"held vertex Undo changed completed paths");
    historyState(false,true);
    toolbar("Redo"); penClick({930,220}); toolbar("Undo");
    penClick({960,260}); historyState(true,false);
    toolbar("Redo"); key(Qt::Key_Return,"Enter commits new draft branch");
    QPainterPath branch(canvas->viewport().toDocument({800,100}));
    branch.lineTo(canvas->viewport().toDocument({960,260}));
    pathMatches(3,branch,"new vertex did not replace draft redo branch");
    penClick({700,350}); drag({820,400},{850,420});
    QTest::keySequence(canvas,QKeySequence(QKeySequence::Undo));
    record("Undo removes dragged vertex");
    toolbar("Redo"); penClick({760,510}); toolbar("Undo"); toolbar("Redo");
    penClick({700,350});
    QPainterPath closed(canvas->viewport().toDocument({700,350}));
    closed.cubicTo(canvas->viewport().toDocument({700,350}),canvas->viewport().toDocument({790,380}),canvas->viewport().toDocument({820,400}));
    closed.cubicTo(canvas->viewport().toDocument({850,420}),canvas->viewport().toDocument({760,510}),canvas->viewport().toDocument({760,510}));
    closed.closeSubpath();
    pathMatches(4,closed,"closed draft vertex history mismatch",fontu::ShapePaint::Filled);
    toolbar("Undo"); require(canvas->document().shapes().size()==4,"closed path Undo did not remove whole edit");
    toolbar("Redo"); pathMatches(4,closed,"closed path Redo mismatch",fontu::ShapePaint::Filled);
    penClick({900,400}); toolbar("Undo"); historyState(false,true);
    key(Qt::Key_Escape,"Escape discards zero-vertex redo"); historyState(true,false);
    penClick({900,400}); toolbar("Undo"); historyState(false,true);
    toolbar("Select (V)"); historyState(true,false);
    toolbar("Undo"); require(canvas->document().shapes().size()==4,"tool change did not resume document Undo");
    toolbar("Redo");
    record("History buttons, shortcuts, draft vertices and tangents, zero boundary, branching, commits and cancellation verified");
}
void statusHint(bool closing) {
    const auto text = window->statusBar()->currentMessage();
    require(window->statusBar()->isVisible(), "bottom status bar is hidden");
    require(window->statusBar()->geometry().top() >= canvas->geometry().bottom(), "status bar overlaps canvas");
    require(text.contains("Wheel zooms") && text.contains("Space+drag pans"), "navigation hints missing");
    require(text.contains("Click first anchor to fill") == closing, "Pen closure hint does not match draft");
    require(window->statusBar()->fontMetrics().horizontalAdvance(text) + 12 <= window->findChild<QLabel *>("projectStatus")->geometry().left(),
            "status hint clips at normal window size");
}
void status() {
    statusHint(false);
    require(window->statusBar()->currentMessage().contains("Delete selection"), "initial Select hint missing");
    toolbar("Rectangle (R)"); statusHint(false);
    key(Qt::Key_E,"E"); statusHint(false);
    toolbar("Pen (P)"); statusHint(false);
    require(window->statusBar()->currentMessage().contains("Click anchors"), "initial Pen hint missing");
    penClick({100,100}); penClick({220,100}); penClick({220,200}); statusHint(true);
    toolbar("Undo"); statusHint(false);
    toolbar("Redo"); statusHint(true);
    QTest::keyPress(canvas,Qt::Key_Space);
    drag({500,350},{520,360});
    QTest::keyRelease(canvas,Qt::Key_Space); statusHint(true);
    move({120,110},Qt::NoButton); record("Closure hover keeps bottom hint and clean drawing area"); statusHint(true);
    key(Qt::Key_Return,"Enter finishes draft"); statusHint(false);
    penClick({300,100}); penClick({420,100}); penClick({420,200}); statusHint(true);
    penClick({300,100}); statusHint(false);
    penClick({500,100}); penClick({620,100}); penClick({620,200}); statusHint(true);
    key(Qt::Key_Escape,"Escape cancels draft"); statusHint(false);
    penClick({500,100}); penClick({620,100}); penClick({620,200}); statusHint(true);
    key(Qt::Key_V,"V cancels draft"); statusHint(false);
    key(Qt::Key_P,"P"); statusHint(false);
    penClick({500,100}); penClick({620,100}); penClick({620,200}); statusHint(true);
    window->resize(1000,720); QApplication::processEvents(); statusHint(false);
    record("Resize cancels draft and keeps readable bottom hint");
    toolbar("Select (V)"); statusHint(false);
    window->resize(800,600); QApplication::processEvents(); statusHint(false);
    record("Select hint fits 800-pixel window");
    key(Qt::Key_P,"P at 800 pixels"); statusHint(false);
    penClick({100,100}); penClick({220,100}); penClick({220,200}); statusHint(true);
    record("Pen closure hint fits 800-pixel window");
}
template<class Dialog, class Callback>
void whenModal(Callback callback) {
    auto *timer = new QTimer(qApp);
    QObject::connect(timer, &QTimer::timeout, [timer, callback] {
        if (auto *dialog = qobject_cast<Dialog *>(QApplication::activeModalWidget())) {
            timer->stop(); timer->deleteLater(); callback(dialog);
        }
    });
    timer->start(1);
}
void prompt(QMessageBox::StandardButton choice) {
    whenModal<QMessageBox>([choice](QMessageBox *dialog) {
        QTest::mouseClick(dialog->button(choice), Qt::LeftButton);
    });
}
void fileChoice(const QString &path, const std::function<void()> &after = {}) {
    whenModal<QFileDialog>([path, after](QFileDialog *dialog) {
        if (after) after();
        dialog->selectFile(path);
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    });
}
void fileShortcut(QKeySequence::StandardKey key) {
    window->activateWindow(); canvas->setFocus(); QApplication::processEvents();
    const auto binding = QKeySequence::keyBindings(key).front()[0];
    QTest::keyClick(canvas, Qt::Key(binding.key()), binding.keyboardModifiers());
    QApplication::processEvents();
}
void fileMenu(const QString &name) {
    auto *menu = window->menuBar()->actions().front()->menu();
    auto *action = window->findChild<QAction *>(name);
    require(menu && action, "File action missing");
    menu->popup(window->menuBar()->mapToGlobal(QPoint(0, window->menuBar()->height())));
    QApplication::processEvents(); record("File menu open for " + name);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center());
    QApplication::processEvents();
}
void persistence() {
    auto *projectWindow = qobject_cast<fontu::ProjectWindow *>(window);
    require(projectWindow, "production project window missing");
    const auto initial = output + "/initial.fontu";
    require(fontu::writeProject(initial, canvas->projectSnapshot()).isEmpty(), "fixture write failed");
    fileChoice(initial); fileShortcut(QKeySequence::Open);
    require(projectWindow->projectPath() == initial && !window->isWindowModified(), "Open shortcut failed");
    record("Open shortcut loads a blank project through QFileDialog");
    const auto screen = [](QPointF p) { return canvas->viewport().toScreen(p).toPoint(); };
    toolbar("Rectangle (R)"); drag(screen({-200,100}), screen({-100,200}));
    require(projectWindow->isProjectDirty() && window->isWindowModified(), "drawing did not mark project dirty");
    auto *picker = window->findChild<QListWidget *>("glyphPicker");
    QTest::mouseClick(picker->viewport(), Qt::LeftButton, Qt::NoModifier, picker->visualItemRect(picker->item(1)).center());
    require(canvas->activeGlyphId().value == 2, "second glyph not selected");
    toolbar("Pen (P)"); penClick(screen({-100,0})); drag(screen({100,200}),screen({140,250})); penClick(screen({200,0}));
    key(Qt::Key_Return,"Commit cubic path before Save");
    require(canvas->document().shapes()[0].path.elementAt(1).isCurveTo(), "Pen path is not cubic");
    const auto snapshot = fontu::projectBytes(canvas->projectSnapshot());
    fileShortcut(QKeySequence::Save);
    require(!projectWindow->isProjectDirty() && !window->isWindowModified(), "Save shortcut failed");
    const auto saved = fontu::readProject(initial);
    require(std::holds_alternative<fontu::ProjectSnapshot>(saved), "saved project did not parse");
    const auto &loaded = std::get<fontu::ProjectSnapshot>(saved);
    require(loaded.glyphs.size()==10 && loaded.glyphs[0].shapes.size()==1 && loaded.glyphs[1].shapes.size()==1,
            "Save omitted another glyph drawing");
    require(fontu::projectBytes(loaded)==snapshot, "Save changed geometry or metadata");
    record("Save shortcut writes all ten glyphs and completed cubic; dirty indicator clears");
    toolbar("Undo"); require(projectWindow->isProjectDirty(), "Undo away from saved content remained clean");
    toolbar("Redo"); require(!projectWindow->isProjectDirty(), "Redo to saved content remained dirty");
    const auto copy=output+"/copy.fontu";
    fileChoice(copy); fileShortcut(QKeySequence::SaveAs);
    require(projectWindow->projectPath()==copy && !projectWindow->isProjectDirty(), "Save As shortcut failed");
    record("Save As shortcut selects another file");
    toolbar("Undo"); const auto unsaved=fontu::projectBytes(canvas->projectSnapshot());
    fileChoice(initial, [] { prompt(QMessageBox::Cancel); }); fileMenu("openProject");
    require(fontu::projectBytes(canvas->projectSnapshot())==unsaved && projectWindow->projectPath()==copy,
            "Cancel on Open changed project");
    record("File Open and unsaved Cancel preserve completed drawings and filename");
    fileChoice(initial, [] { prompt(QMessageBox::Discard); }); fileShortcut(QKeySequence::Open);
    require(fontu::projectBytes(canvas->projectSnapshot())==snapshot && !canvas->canUndo() && !canvas->document().selection(),
            "Reopen failed to restore drawing or clear transients");
    record("Reopen restores active glyph, collection, metrics and exact cubic geometry");
    fileMenu("saveProject"); require(!projectWindow->isProjectDirty(), "File Save failed");
    const auto third=output+"/menu-copy.fontu";
    fileChoice(third); fileMenu("saveProjectAs"); require(projectWindow->projectPath()==third, "File Save As failed");
    record("File menu Save and Save As complete through QAction and QFileDialog");
    toolbar("Rectangle (R)"); drag(screen({-300,300}),screen({-200,400}));
    const auto beforeError=fontu::projectBytes(canvas->projectSnapshot());
    QFile invalid(output+"/invalid.fontu"); require(invalid.open(QIODevice::WriteOnly), "invalid fixture failed");
    invalid.write("{invalid"); invalid.close();
    fileChoice(invalid.fileName(), [] { prompt(QMessageBox::Ok); }); fileShortcut(QKeySequence::Open);
    require(fontu::projectBytes(canvas->projectSnapshot())==beforeError && projectWindow->isProjectDirty(), "invalid Open changed project");
    record("Invalid Open shows error and preserves dirty drawing");
    prompt(QMessageBox::Cancel); require(!window->close() && window->isVisible(), "Close Cancel failed");
    record("Unsaved Close Cancel keeps production window open");
}
void run() {
    try {
        for(auto *widget : QApplication::topLevelWidgets()) {
            auto *candidate=qobject_cast<QMainWindow *>(widget);
            if(candidate && qobject_cast<fontu::ProjectWindow *>(candidate)) window=candidate;
        }
        require(window && window->isVisible(),"production window not ready");
        canvas=qobject_cast<fontu::Canvas *>(window->centralWidget());
        require(canvas && canvas->document().shapes().empty(),"doctor: unexpected canvas");
        require(window->findChild<QToolBar *>() && window->findChild<QToolBar *>()->windowTitle()=="Shape tools" && window->findChild<QToolBar *>()->findChild<QAction *>("snapToGrid") && window->findChild<QToolBar *>()->findChild<QAction *>("snapToGeometry") && window->findChild<QAction *>("openProject") && window->findChild<QAction *>("saveProject") && window->findChild<QAction *>("saveProjectAs"),"doctor: production controls missing");
        record("Doctor passed: fresh production window, toolbar, Snap controls, empty document");
        if(feature != "glyph" && feature != "glyphs" && feature != "persistence") {
            window->resize(window->width()+1100-canvas->width(),window->height());
            QApplication::processEvents();
            legacyView();
        }
        if(feature=="persistence") {
            persistence();
        } else if(feature=="glyphs") {
            auto *picker = window->findChild<QListWidget *>("glyphPicker");
            require(picker && picker->count()==10, "glyph picker missing explicit digits");
            require(picker->currentItem()->text()=="0" && canvas->activeGlyphId().value==1, "initial digit selection wrong");
            for(int row=0;row<10;++row) require(picker->item(row)->text()==QString::number(row), "digit labels wrong");
            record("Initial digit 0 selected in right glyph panel");
            const auto choose = [picker](int row) {
                picker->scrollToItem(picker->item(row));
                QApplication::processEvents();
                QTest::mouseClick(picker->viewport(),Qt::LeftButton,Qt::NoModifier,
                                  picker->visualItemRect(picker->item(row)).center());
                require(picker->currentRow()==row, "picker selection did not change");
                require(canvas->activeGlyphId().value==picker->item(row)->data(Qt::UserRole).toULongLong(), "picker and canvas disagree");
                require(canvas->hasFocus(), "canvas focus not restored after glyph selection");
                record("Glyph row click: " + picker->item(row)->text());
            };
            const auto screen = [](QPointF point) { return canvas->viewport().toScreen(point).toPoint(); };
            choose(0);
            QTest::keyClick(QApplication::focusWidget(),Qt::Key_R);
            toolState(fontu::Tool::Rectangle);
            record("Same selected glyph click restores canvas keyboard shortcut focus");
            toolbar("Rectangle (R)");
            drag(screen({-200,100}),screen({0,300}));
            require(canvas->document().shapes().size()==1,"digit 0 drawing missing");
            const auto zero = canvas->document().shapes()[0].path;
            const auto zeroSelection = canvas->document().selection();
            toolbar("Undo");
            require(canvas->document().shapes().empty() && canvas->canRedo(), "digit 0 redo setup failed");
            choose(1);
            require(canvas->document().shapes().empty() && !canvas->canUndo() && !canvas->canRedo(), "history leaked into digit 1");
            toolbar("Ellipse (E)");
            drag(screen({100,200}),screen({300,400}));
            const auto one = canvas->document().shapes()[0].path;
            choose(0);
            require(canvas->document().shapes().empty() && canvas->canRedo(), "digit 0 redo not retained");
            toolbar("Redo");
            require(canvas->document().shapes()[0].path==zero && canvas->document().selection()==zeroSelection,"digit 0 drawing/selection changed");
            choose(1);
            require(canvas->document().shapes()[0].path==one,"digit 1 drawing changed");
            toolbar("Undo");
            choose(0);
            require(canvas->document().shapes()[0].path==zero,"digit 1 undo affected digit 0");
            toolbar("Pen (P)");
            penClick(screen({-100,400})); penClick(screen({100,400}));
            choose(2);
            require(canvas->document().shapes().empty() && !canvas->canUndo() && !canvas->canRedo(),"draft leaked into digit 2");
            key(Qt::Key_Return,"Enter on new glyph");
            choose(0);
            key(Qt::Key_Return,"Enter after returning cancels outgoing draft");
            require(canvas->document().shapes().size()==1 && canvas->document().shapes()[0].path==zero,"unfinished Pen returned");
            toolbar("Select (V)");
            const auto start=screen({-100,200}), end=screen({0,250});
            QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,start); move(end,Qt::LeftButton);
            require(canvas->document().shapes()[0].path!=zero,"move setup failed");
            choose(3);
            QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,end);
            require(canvas->document().shapes().empty(),"release leaked into new glyph");
            choose(0);
            require(canvas->document().shapes()[0].path==zero,"outgoing unfinished move not rolled back");
            window->resize(800,500); QApplication::processEvents();
            require(picker->verticalScrollBar()->maximum()>0,"glyph panel does not scroll at smaller height");
            choose(9); choose(0);
            require(canvas->document().shapes()[0].path==zero,"scroll selection lost completed work");
            toolbar("Reset view (0)");
            record("Independent digit drawings and histories survive real picker switching");
        } else if(feature=="glyph") {
            const auto screen = [](QPointF point) { return canvas->viewport().toScreen(point).toPoint(); };
            const auto page = canvas->viewport().transform().mapRect(canvas->document().definition().bounds);
            require(canvas->rect().contains(page.toAlignedRect()), "initial glyph workspace not fitted");
            record("Fitted 1000 by 1000 font-unit document with labeled typography guides");
            toolbar("Rectangle (R)");
            drag(screen({510,200}),screen({300,300}));
            require(canvas->document().shapes().empty(),"outside rectangle start accepted");
            drag(screen({350,100}),screen({650,-400}));
            require(std::abs(canvas->document().shapes()[0].path.boundingRect().right()-500)<1e-7 && std::abs(canvas->document().shapes()[0].path.boundingRect().top()+250)<1e-7,"rectangle boundary clamp failed");
            toolbar("Undo");
            toolbar("Ellipse (E)");
            drag(screen({350,100}),screen({650,-400}));
            require(std::abs(canvas->document().shapes()[0].path.boundingRect().right()-500)<1e-7 && std::abs(canvas->document().shapes()[0].path.boundingRect().top()+250)<1e-7,"ellipse boundary clamp failed");
            toolbar("Undo");
            toolbar("Rectangle (R)");
            snap("snapToGeometry");
            const auto target=screen({123,500});
            hover(target+QPoint(0,3),target,"Typography x-height guide");
            drag(target+QPoint(0,3),screen({220,600}));
            require(std::abs(canvas->document().shapes()[0].path.boundingRect().top()-500)<1e-7,"guide-snapped draw start failed");
            record("Geometry toggle snaps Rectangle to x-height 500 font units");
            snap("snapToGeometry");
            toolbar("Select (V)");
            drag(screen({170,550}),screen({800,-500}));
            const auto moved=canvas->document().shapes()[0].path.boundingRect();
            require(std::abs(moved.right()-500)<1e-7 && std::abs(moved.top()+250)<1e-7,"bounded move failed");
            drag(screen(moved.topLeft()),screen({-700,-600}));
            const auto resized=canvas->document().shapes()[0].path.boundingRect();
            require(std::abs(resized.left()+500)<1e-7 && std::abs(resized.top()+250)<1e-7,"bounded resize failed");
            toolbar("Undo"); toolbar("Undo"); toolbar("Undo");
            toolbar("Pen (P)");
            penClick(screen({510,0}));
            require(!canvas->canUndo(),"outside Pen anchor accepted");
            penClick(screen({480,0}));
            drag(screen({490,100}),screen({800,500}));
            key(Qt::Key_Return,"Enter bounded cubic Pen path");
            require(canvas->document().shapes().size()==1,"bounded Pen commit failed");
            const auto original=canvas->document().shapes()[0].path;
            require(original.elementAt(1).isCurveTo() && canvas->document().definition().bounds.contains(original.controlPointRect()),"Pen tangent escaped workspace");
            drag(screen({550,0}),screen({600,100}),Qt::MiddleButton);
            QWheelEvent wheel(QPointF(400,300),canvas->mapToGlobal(QPoint(400,300)),{}, {0,120},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QApplication::sendEvent(canvas,&wheel);
            require(canvas->document().shapes()[0].path==original,"pan or zoom changed bounded document");
            snap("snapToGeometry");
            const auto baseline=screen({113,0});
            hover(baseline+QPoint(0,3),baseline,"Baseline after pan and zoom");
            record("Bounded Pen remains cubic; navigation preserves font coordinates; transformed baseline snaps");
            toolbar("Reset view (0)");
            require(canvas->rect().contains(canvas->viewport().transform().mapRect(canvas->document().definition().bounds).toAlignedRect()),"reset failed to fit document");
            require(canvas->document().shapes()[0].path==original,"reset changed glyph geometry");
            record("Reset view fits complete glyph document and typography guides");
        } else if(feature=="drawing") {
            toolbar("Rectangle (R)"); drag({100,100},{220,190});
            require(canvas->document().shapes().size()==1 && bounds(0)==QRectF(100,100,120,90),"rectangle mismatch");
            key(Qt::Key_E,"E"); drag({300,100},{430,200});
            require(canvas->document().shapes().size()==2 && bounds(1)==QRectF(300,100,130,100),"ellipse mismatch");
            require(canvas->document().shapes()[1].path.elementAt(1).isCurveTo(),"ellipse lacks curve");
            toolbar("Rectangle (R)"); toolbar("Ellipse (E)"); key(Qt::Key_R,"R");
            drag({500,100},{501,101}); require(canvas->document().shapes().size()==2,"tiny shape committed");
            QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,{500,250});move({600,350},Qt::LeftButton);record("Unfinished draw preview");
            key(Qt::Key_Escape,"Escape");QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,{600,350});
            require(canvas->document().shapes().size()==2,"cancelled draw committed");
        } else if(feature=="status") {
            status();
        } else if(feature=="history") {
            history();
        } else if(feature=="editing") {
            toolbar("Rectangle (R)");drag({100,100},{220,200});
            toolbar("Select (V)");drag({150,150},{180,170});require(bounds(0)==QRectF(130,120,120,100),"move mismatch");
            drag({250,220},{280,240});require(bounds(0)==QRectF(130,120,150,120),"resize mismatch");
            key(Qt::Key_R,"R");key(Qt::Key_V,"V");key(Qt::Key_Escape,"Escape");require(!canvas->document().selection(),"selection uncleared");
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,{180,170});record("Select shape interior");
            key(Qt::Key_Delete,"Delete");require(canvas->document().shapes().empty(),"delete failed");
            key(Qt::Key_R,"R");drag({100,100},{220,200});key(Qt::Key_Backspace,"Backspace");require(canvas->document().shapes().empty(),"backspace failed");
        } else if(feature=="navigation") {
            toolbar("Rectangle (R)");drag({100,100},{220,200});auto original=canvas->document().shapes()[0].path;
            QWheelEvent wheel(QPointF(200,150),canvas->mapToGlobal(QPoint(200,150)),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
            QApplication::sendEvent(canvas,&wheel);require(std::abs(canvas->viewport().zoom()-1.2)<1e-9,"zoom failed");record("Wheel +120 at (200,150)");
            auto before=bounds(0);drag({500,350},{530,370},Qt::MiddleButton);require(bounds(0).topLeft()==before.topLeft()+QPointF(30,20),"pan failed");
            QTest::keyPress(canvas,Qt::Key_Space);before=bounds(0);drag({500,350},{520,360});QTest::keyRelease(canvas,Qt::Key_Space);require(bounds(0).topLeft()==before.topLeft()+QPointF(20,10),"space pan failed");
            toolbar("Reset view (0)");const auto fitZoom=canvas->viewport().zoom();require(fitZoom<1 && canvas->rect().contains(canvas->viewport().transform().mapRect(canvas->document().definition().bounds).toAlignedRect()),"toolbar fit reset failed");
            QApplication::sendEvent(canvas,&wheel);key(Qt::Key_0,"0");require(canvas->viewport().zoom()==fitZoom && canvas->document().shapes()[0].path==original,"key reset changed document");
        } else if(feature=="pen") {
            pen();
        } else if(feature=="snapping") {
            window->resize(800,600); QApplication::processEvents();
            record("Snap toolbar at 800 by 600");
            snap("snapToGrid"); snap("snapToGrid");
            snap("snapToGeometry"); snap("snapToGeometry");
            window->resize(1100,760); QApplication::processEvents();
            record("Snap toolbar at 1100 by 760");
            snap("snapToGrid");require(canvas->gridSnapEnabled() && !canvas->geometrySnapEnabled(),"grid mode mismatch");
            snap("snapToGeometry");require(canvas->gridSnapEnabled() && canvas->geometrySnapEnabled(),"both modes mismatch");
            snap("snapToGrid");require(!canvas->gridSnapEnabled() && canvas->geometrySnapEnabled(),"geometry mode mismatch");
            snap("snapToGeometry");require(!canvas->gridSnapEnabled() && !canvas->geometrySnapEnabled(),"off mode mismatch");
            toolbar("Rectangle (R)");
            drag({112,100},{180,200});
            snap("snapToGeometry");
            hover({115,125},{112,125},"Geometry drawing before press");
            snap("snapToGrid");
            hover({115,125},{112,125},"Both options choose nearer geometry before press");
            snap("snapToGrid");
            drag({115,125},{225,225});
            require(bounds(1)==QRectF(112,125,113,100),"hover target disagrees with drawn start");
            record("Snapped drawing bounds match pre-press geometry marker");
            snap("snapToGeometry");
            snap("snapToGrid");
            const auto gridTarget = canvas->viewport().toScreen({-300,150}).toPoint();
            hover(gridTarget+QPoint(3,2),gridTarget,"Grid drawing before press");
        } else throw std::runtime_error("unknown feature");
        record("PASS: all assertions for " + feature);
        if (window->isWindowModified()) prompt(QMessageBox::Discard);
        window->close();QApplication::exit(0);
    } catch(const std::exception &error) {
        QFile file(output+"/failure.txt");if(file.open(QIODevice::WriteOnly)) file.write(error.what());
        if(window) {
            if (window->isWindowModified()) prompt(QMessageBox::Discard);
            window->close();
        }
        QApplication::exit(1);
    }
}
void started() { QTimer::singleShot(0,[] { QTimer::singleShot(50,run); }); }
}
Q_COREAPP_STARTUP_FUNCTION(started)
int main(int argc,char **argv) {
    output=qEnvironmentVariable("FONTU_VERIFY_EVIDENCE");feature=qEnvironmentVariable("FONTU_VERIFY_FEATURE");
    if(output.isEmpty() || !QDir(output).exists()) return 2;
    return fontuProductionMain(argc,argv);
}
