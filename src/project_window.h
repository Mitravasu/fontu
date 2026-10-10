#pragma once
#include "canvas.h"
#include <QMainWindow>
class QLabel;
class QStackedWidget;
class QToolBar;
class QDockWidget;
class QAction;

namespace fontu {
class CreationPage;
class ProjectWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ProjectWindow(QWidget *parent = nullptr, QString projectFolder = defaultProjectFolder());
    Canvas &canvas() const { return *canvas_; }
    QString projectPath() const { return path_; }
    bool isProjectDirty() const;
    bool saveProject();
    bool saveProjectAs();
    bool exportFontAs();
    void openProject();
    void newProject();
    bool hasProject() const;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    enum class Page { Welcome, Editor, ReplacementCreation };
    void showPage(Page page);
    void createFromPage();
    void installProject(ProjectSnapshot project, QString path);
    void setupEditorChrome();
    void updateEditorActions();
    bool saveTo(const QString &path);
    bool confirmReplacement();
    void updateTitle();
    void showError(const QString &operation, const QString &error);
    Canvas *canvas_;
    CreationPage *creationPage_;
    QStackedWidget *pages_;
    QToolBar *toolbar_;
    QDockWidget *glyphPanel_;
    QAction *saveAction_;
    QAction *saveAsAction_;
    QAction *exportOtfAction_;
    QAction *undoAction_;
    QAction *redoAction_;
    Page page_ = Page::Welcome;
    QLabel *projectStatus_;
    QString path_;
    QString projectFolder_;
    QByteArray savedContent_;
};
}
