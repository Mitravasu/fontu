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
        shapes.append(QJsonObject{{"id",double(shape.id)},{"screenBounds", QJsonArray{r.x(),r.y(),r.width(),r.height()}},
                                {"curve",shape.path.elementAt(1).isCurveTo()}});
    }
    QJsonObject state{{"action",action},{"feature",feature},{"backend",QApplication::platformName()},
        {"inputBoundary","Qt events to unchanged production window, not OS input"},
        {"window",window->windowTitle()},{"shapes",shapes},{"tool",int(canvas->tool())},
        {"selected",canvas->document().selection() ? double(*canvas->document().selection()) : -1},
        {"grid",canvas->gridSnapEnabled()},{"geometry",canvas->geometrySnapEnabled()},
        {"zoom",canvas->viewport().zoom()},{"screenshot",stem + ".png"}};
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
    const QString names[]={"Select (V)","Rectangle (R)","Ellipse (E)"};
    for(auto *action : window->findChild<QToolBar *>()->actions()) {
        for(int i=0;i<3;++i) if(action->text()==names[i])
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
    record("Toolbar click: " + text);
}
void key(Qt::Key key, const QString &label) {
    QTest::keyClick(canvas,key);
    if(key==Qt::Key_V) toolState(fontu::Tool::Select);
    if(key==Qt::Key_R) toolState(fontu::Tool::Rectangle);
    if(key==Qt::Key_E) toolState(fontu::Tool::Ellipse);
    record("Canvas key: " + label);
}
void move(QPoint point, Qt::MouseButtons buttons) {
    QMouseEvent event(QEvent::MouseMove,point,canvas->mapToGlobal(point),Qt::NoButton,buttons,Qt::NoModifier);
    QApplication::sendEvent(canvas,&event);
}
void drag(QPoint from, QPoint to, Qt::MouseButton button=Qt::LeftButton) {
    QTest::mousePress(canvas,button,Qt::NoModifier,from);
    move(to,button);
    record(QString("Drag preview (%1,%2) to (%3,%4)").arg(from.x()).arg(from.y()).arg(to.x()).arg(to.y()));
    QTest::mouseRelease(canvas,button,Qt::NoModifier,to);
    record("Drag release");
}
void snap(const QString &name) {
    auto *bar=window->menuBar();
    bar->setNativeMenuBar(false); // Offscreen has no OS menu; expose the same production menu locally.
    auto *menu=bar->actions().at(0)->menu();
    auto *action=menu->findChild<QAction *>(name);
    require(action,"snap action missing");
    bool previous=action->isChecked();
    QTest::mouseClick(bar,Qt::LeftButton,Qt::NoModifier,bar->actionGeometry(menu->menuAction()).center());
    QApplication::processEvents();
    require(menu->isVisible(),"Snap menu did not open");
    record("Open Snap menu");
    QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,menu->actionGeometry(action).center());
    require(action->isChecked()!=previous,"snap toggle failed");
    record("Snap menu click: " + action->text());
}
void run() {
    try {
        for(auto *widget : QApplication::topLevelWidgets()) {
            auto *candidate=qobject_cast<QMainWindow *>(widget);
            if(candidate && candidate->windowTitle()=="fontu • Shape canvas") window=candidate;
        }
        require(window && window->isVisible(),"production window not ready");
        canvas=qobject_cast<fontu::Canvas *>(window->centralWidget());
        require(canvas && canvas->document().shapes().empty(),"doctor: unexpected canvas");
        require(window->findChild<QToolBar *>() && window->findChild<QToolBar *>()->windowTitle()=="Shape tools" && window->menuBar()->actions().size()==1 && window->menuBar()->actions().at(0)->text()=="Snap","doctor: production controls missing");
        record("Doctor passed: fresh production window, toolbar, Snap menu, empty document");
        if(feature=="drawing") {
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
            toolbar("Reset view (0)");require(canvas->viewport().zoom()==1,"toolbar reset failed");
            QApplication::sendEvent(canvas,&wheel);key(Qt::Key_0,"0");require(canvas->viewport().zoom()==1 && canvas->document().shapes()[0].path==original,"key reset changed document");
        } else if(feature=="snapping") {
            snap("snapToGrid");require(canvas->gridSnapEnabled() && !canvas->geometrySnapEnabled(),"grid mode mismatch");
            snap("snapToGeometry");require(canvas->gridSnapEnabled() && canvas->geometrySnapEnabled(),"both modes mismatch");
            snap("snapToGrid");require(!canvas->gridSnapEnabled() && canvas->geometrySnapEnabled(),"geometry mode mismatch");
            snap("snapToGeometry");require(!canvas->gridSnapEnabled() && !canvas->geometrySnapEnabled(),"off mode mismatch");
        } else throw std::runtime_error("unknown feature");
        record("PASS: all assertions for " + feature);
        window->close();QApplication::exit(0);
    } catch(const std::exception &error) {
        QFile file(output+"/failure.txt");if(file.open(QIODevice::WriteOnly)) file.write(error.what());
        if(window) window->close();QApplication::exit(1);
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
