#include "glyph_panel.h"
#include "canvas.h"
#include <QDockWidget>
#include <QListWidget>
#include <QMainWindow>
#include <QSignalBlocker>

namespace fontu {
QDockWidget *addGlyphPanel(QMainWindow &window, Canvas &canvas) {
    auto *dock = new QDockWidget("Glyphs", &window);
    dock->setObjectName("glyphPanel");
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setAllowedAreas(Qt::RightDockWidgetArea);
    dock->setMinimumWidth(140);
    auto *list = new QListWidget(dock);
    list->setObjectName("glyphPicker");
    list->setAccessibleName("Glyphs");
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setMinimumWidth(140);
    list->setMaximumWidth(200);
    const auto rebuild = [&canvas, list] {
        const QSignalBlocker blocker(list);
        list->clear();
        for (const auto &glyph : canvas.glyphs()) {
            auto *item = new QListWidgetItem(glyph.label, list);
            item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(glyph.id.value));
            item->setToolTip(glyph.label);
            item->setSizeHint(QSize(120, 48));
            auto font = item->font();
            font.setPointSize(20);
            item->setFont(font);
        }
    };
    rebuild();
    QObject::connect(&canvas, &Canvas::glyphsChanged, list, rebuild);
    const auto syncSelection = [&canvas, list] {
        const QSignalBlocker blocker(list);
        for (int row = 0; row < list->count(); ++row) {
            auto *item = list->item(row);
            if (item->data(Qt::UserRole).toULongLong() == canvas.activeGlyphId().value) {
                list->setCurrentItem(item);
                break;
            }
        }
    };
    syncSelection();
    QObject::connect(&canvas, &Canvas::activeGlyphChanged, list, syncSelection);
    QObject::connect(list, &QListWidget::currentItemChanged, &canvas,
                     [&canvas](QListWidgetItem *item) {
        if (item) canvas.setActiveGlyph({item->data(Qt::UserRole).toULongLong()});
        canvas.setFocus();
    });
    QObject::connect(list, &QListWidget::itemClicked, &canvas,
                     [&canvas] { canvas.setFocus(); });
    dock->setWidget(list);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);
    return dock;
}
}
