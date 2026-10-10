#include "snap_menu.h"
#include "canvas.h"
#include <QMenuBar>
#include <QMenu>

namespace fontu {
void addSnapMenu(QMenuBar &menuBar, Canvas &canvas) {
    auto *menu = menuBar.addMenu("Snap");
    auto *grid = menu->addAction("To grid");
    grid->setObjectName("snapToGrid");
    grid->setCheckable(true);
    grid->setChecked(canvas.gridSnapEnabled());
    auto *geometry = menu->addAction("To geometry");
    geometry->setObjectName("snapToGeometry");
    geometry->setCheckable(true);
    geometry->setChecked(canvas.geometrySnapEnabled());
    QObject::connect(grid, &QAction::toggled, &canvas, &Canvas::setGridSnapEnabled);
    QObject::connect(geometry, &QAction::toggled, &canvas, &Canvas::setGeometrySnapEnabled);
    QObject::connect(&canvas, &Canvas::snapOptionsChanged, menu, [grid, geometry, &canvas] {
        grid->setChecked(canvas.gridSnapEnabled());
        geometry->setChecked(canvas.geometrySnapEnabled());
    });
}
}
