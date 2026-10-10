#include "project_window.h"
#include <QApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("fontu");
    fontu::ProjectWindow window;
    window.resize(1100, 760);
    window.show();
    return app.exec();
}
