#include "snap_menu.h"
#include "canvas.h"
#include <QAction>
#include <QLabel>
#include <QToolBar>

namespace fontu {
void addSnapControls(QToolBar &toolbar, Canvas &canvas) {
    toolbar.addWidget(new QLabel("Snap", &toolbar));
    auto *grid = toolbar.addAction("To grid");
    grid->setObjectName("snapToGrid");
    grid->setCheckable(true);
    grid->setChecked(canvas.gridSnapEnabled());
    auto *geometry = toolbar.addAction("To shape geometry");
    geometry->setObjectName("snapToGeometry");
    geometry->setCheckable(true);
    geometry->setChecked(canvas.geometrySnapEnabled());
    QObject::connect(grid, &QAction::toggled, &canvas, &Canvas::setGridSnapEnabled);
    QObject::connect(geometry, &QAction::toggled, &canvas, &Canvas::setGeometrySnapEnabled);
    QObject::connect(&canvas, &Canvas::snapOptionsChanged, &toolbar, [grid, geometry, &canvas] {
        grid->setChecked(canvas.gridSnapEnabled());
        geometry->setChecked(canvas.geometrySnapEnabled());
    });
}
}
