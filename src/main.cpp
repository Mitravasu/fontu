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
    auto *undo = toolbar->addAction("Undo");
    auto *redo = toolbar->addAction("Redo");
    undo->setShortcuts(QKeySequence::Undo);
    redo->setShortcuts(QKeySequence::Redo);
    const auto updateHistory = [canvas, undo, redo] {
        undo->setEnabled(canvas->canUndo());
        redo->setEnabled(canvas->canRedo());
    };
    QObject::connect(canvas, &fontu::Canvas::historyChanged, &window, updateHistory);
    updateHistory();
    QObject::connect(undo, &QAction::triggered, canvas, [canvas] { canvas->undo(); canvas->setFocus(); });
    QObject::connect(redo, &QAction::triggered, canvas, [canvas] { canvas->redo(); canvas->setFocus(); });
    toolbar->addSeparator();
    fontu::addSnapControls(*toolbar, *canvas);
    toolbar->addSeparator();
    auto *reset = toolbar->addAction("Reset view (0)");
    QObject::connect(reset, &QAction::triggered, canvas, [canvas] {
        canvas->resetView();
        canvas->setFocus();
    });
    auto *status = window.statusBar();
    QObject::connect(canvas, &fontu::Canvas::controlsHintChanged, status,
                     [status](const QString &hint) { status->showMessage(hint); });
    status->showMessage(canvas->controlsHint());
    window.resize(1100, 760);
    window.show();
    canvas->setFocus();
    return app.exec();
}
