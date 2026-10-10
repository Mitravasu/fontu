#include "canvas.h"
#include "snap_menu.h"
#include <QActionGroup>
#include <QApplication>
#include <QMainWindow>
#include <QStatusBar>
#include <QToolBar>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("fontu");
    QMainWindow window;
    window.setWindowTitle("fontu • Shape canvas");
    auto *canvas = new fontu::Canvas;
    window.setCentralWidget(canvas);
    fontu::addSnapMenu(*window.menuBar(), *canvas);
    auto *toolbar = window.addToolBar("Shape tools");
    toolbar->setMovable(false);
    auto *group = new QActionGroup(&window);
    const std::pair<QString, fontu::Tool> tools[] = {
        {"Select (V)", fontu::Tool::Select}, {"Rectangle (R)", fontu::Tool::Rectangle},
        {"Ellipse (E)", fontu::Tool::Ellipse}, {"Pen (P)", fontu::Tool::Pen}};
    for (const auto &entry : tools) {
        const auto tool = entry.second;
        auto *action = toolbar->addAction(entry.first);
        action->setCheckable(true);
        action->setChecked(tool == fontu::Tool::Select);
        group->addAction(action);
        QObject::connect(action, &QAction::triggered, canvas, [canvas, tool] {
            canvas->setTool(tool);
            canvas->setFocus();
        });
        QObject::connect(canvas, &fontu::Canvas::toolChanged, action, [action, tool](fontu::Tool current) {
            action->setChecked(current == tool);
        });
    }
    toolbar->addSeparator();
    auto *reset = toolbar->addAction("Reset view (0)");
    QObject::connect(reset, &QAction::triggered, canvas, [canvas] {
        canvas->resetView();
        canvas->setFocus();
    });
    const auto showControls = [&window](fontu::Tool tool) {
        window.statusBar()->showMessage(tool == fontu::Tool::Pen
            ? "Click adds anchors • Drag sets curve handles • Enter finishes • Esc cancels • Wheel zooms • Space+drag pans"
            : "Drag draws or moves shapes • Handles resize • Wheel zooms • Space+drag pans • Delete removes selection • Esc cancels or clears selection");
    };
    QObject::connect(canvas, &fontu::Canvas::toolChanged, &window, showControls);
    showControls(canvas->tool());
    window.resize(1100, 760);
    window.show();
    canvas->setFocus();
    return app.exec();
}
