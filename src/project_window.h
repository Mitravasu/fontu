#pragma once
#include "canvas.h"
#include <QMainWindow>
class QLabel;

namespace fontu {
class ProjectWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ProjectWindow(QWidget *parent = nullptr, QString projectFolder = defaultProjectFolder());
    Canvas &canvas() const { return *canvas_; }
    QString projectPath() const { return path_; }
    bool isProjectDirty() const;
    bool saveProject();
    bool saveProjectAs();
    void openProject();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    bool saveTo(const QString &path);
    bool confirmReplacement();
    void updateTitle();
    void showError(const QString &operation, const QString &error);
    Canvas *canvas_;
    QLabel *projectStatus_;
    QString path_;
    QString projectFolder_;
    QByteArray savedContent_;
};
}
