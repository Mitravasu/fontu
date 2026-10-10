#include "project_window.h"
#include "creation_page.h"
#include "glyph_panel.h"
#include "snap_menu.h"
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QStackedWidget>
#include <QToolBar>
#include <QDockWidget>
#include <QPushButton>

namespace fontu {
ProjectWindow::ProjectWindow(QWidget *parent, QString projectFolder)
    : QMainWindow(parent), canvas_(new Canvas(this)), projectFolder_(std::move(projectFolder)) {
    pages_ = new QStackedWidget(this);
    creationPage_ = new CreationPage(pages_);
    pages_->addWidget(creationPage_);
    pages_->addWidget(canvas_);
    setCentralWidget(pages_);
    projectStatus_ = new QLabel(this);
    projectStatus_->setObjectName("projectStatus");
    statusBar()->addPermanentWidget(projectStatus_);
    savedContent_ = projectBytes(canvas_->projectSnapshot(), false);
    auto *file = menuBar()->addMenu("&File");
    auto *newAction = file->addAction("&New...");
    newAction->setObjectName("newProject");
    newAction->setShortcuts(QKeySequence::New);
    auto *open = file->addAction("&Open...");
    open->setObjectName("openProject");
    open->setShortcuts(QKeySequence::Open);
    auto *save = file->addAction("&Save");
    save->setObjectName("saveProject");
    save->setShortcuts(QKeySequence::Save);
    auto *saveAs = file->addAction("Save &As...");
    saveAction_ = save;
    saveAsAction_ = saveAs;
    saveAs->setObjectName("saveProjectAs");
    saveAs->setShortcuts(QKeySequence::SaveAs);
    for (auto *action : {newAction, open, save, saveAs})
        action->setToolTip("File actions cancel unfinished input. Completed drawings are preserved.");
    connect(open, &QAction::triggered, this, &ProjectWindow::openProject);
    connect(newAction, &QAction::triggered, this, &ProjectWindow::newProject);
    connect(save, &QAction::triggered, this, &ProjectWindow::saveProject);
    connect(saveAs, &QAction::triggered, this, &ProjectWindow::saveProjectAs);
    connect(canvas_, &Canvas::historyChanged, this, &ProjectWindow::updateTitle);
    connect(canvas_, &Canvas::glyphsChanged, this, &ProjectWindow::updateTitle);
    connect(creationPage_, &CreationPage::createRequested, this, &ProjectWindow::createFromPage);
    connect(creationPage_, &CreationPage::openRequested, this, &ProjectWindow::openProject);
    connect(creationPage_, &CreationPage::cancelRequested, this, [this] {
        if (page_ == Page::ReplacementCreation) showPage(Page::Editor);
    });
    setupEditorChrome();
    showPage(Page::Welcome);
    updateTitle();
}
void ProjectWindow::setupEditorChrome() {
    auto *canvas = canvas_;
    glyphPanel_ = fontu::addGlyphPanel(*this, *canvas);
    auto *toolbar = toolbar_ = addToolBar("Shape tools");
    toolbar->setMovable(false);
    auto *group = new QActionGroup(this);
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
    auto *undo = undoAction_ = toolbar->addAction("Undo");
    auto *redo = redoAction_ = toolbar->addAction("Redo");
    undo->setShortcuts(QKeySequence::Undo);
    redo->setShortcuts(QKeySequence::Redo);
    QObject::connect(canvas, &fontu::Canvas::historyChanged, this, &ProjectWindow::updateEditorActions);
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
    auto *status = statusBar();
    QObject::connect(canvas, &fontu::Canvas::controlsHintChanged, status,
                     [status](const QString &hint) { status->showMessage(hint); });
    status->showMessage(canvas->controlsHint());
}
bool ProjectWindow::hasProject() const { return page_ != Page::Welcome; }
void ProjectWindow::updateEditorActions() {
    const bool editing = page_ == Page::Editor;
    for (auto *action : toolbar_->actions()) action->setEnabled(editing);
    for (auto *action : toolbar_->findChildren<QAction *>()) action->setEnabled(editing);
    undoAction_->setEnabled(editing && canvas_->canUndo());
    redoAction_->setEnabled(editing && canvas_->canRedo());
    saveAction_->setEnabled(editing);
    saveAsAction_->setEnabled(editing);
}
void ProjectWindow::showPage(Page page) {
    page_ = page;
    const bool editing = page == Page::Editor;
    toolbar_->setVisible(editing);
    glyphPanel_->setVisible(editing);
    pages_->setCurrentWidget(editing ? static_cast<QWidget *>(canvas_) : static_cast<QWidget *>(creationPage_));
    creationPage_->setCanCancel(page == Page::ReplacementCreation);
    updateEditorActions();
    if (editing) {
        statusBar()->showMessage(canvas_->controlsHint());
        canvas_->setFocus();
    } else {
        statusBar()->clearMessage();
        creationPage_->findChild<QPushButton *>("createProject")->setFocus();
    }
}
void ProjectWindow::newProject() {
    if (page_ != Page::Editor) return;
    canvas_->cancelPendingInput();
    showPage(Page::ReplacementCreation);
}
void ProjectWindow::createFromPage() {
    auto result = createProject(creationPage_->request());
    if (const auto *error = std::get_if<CreationError>(&result)) {
        creationPage_->showError(*error);
        return;
    }
    if (!confirmReplacement()) return;
    installProject(std::get<ProjectSnapshot>(std::move(result)), {});
}
void ProjectWindow::installProject(ProjectSnapshot project, QString path) {
    path_ = std::move(path);
    savedContent_ = projectBytes(project, false);
    showPage(Page::Editor);
    canvas_->restoreProject(std::move(project));
    canvas_->resetView();
    updateTitle();
    canvas_->setFocus();
}
bool ProjectWindow::isProjectDirty() const {
    return hasProject() && projectBytes(canvas_->projectSnapshot(), false) != savedContent_;
}
void ProjectWindow::updateTitle() {
    const bool dirty = isProjectDirty();
    setWindowModified(dirty);
    projectStatus_->setText(!hasProject() ? QStringLiteral("Create or open a project") : dirty ? QStringLiteral("Unsaved changes")
                                : path_.isEmpty() ? QStringLiteral("Untitled") : QStringLiteral("Saved"));
    setWindowTitle((!hasProject() ? QStringLiteral("Welcome") : path_.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(path_).fileName())
                   + QStringLiteral("[*] • fontu"));
}
void ProjectWindow::showError(const QString &operation, const QString &error) {
    QMessageBox::critical(this, operation, error);
}
bool ProjectWindow::saveTo(const QString &path) {
    const auto project = canvas_->projectSnapshot();
    const auto parent = QFileInfo(path).absolutePath();
    if (!QDir(parent).exists() && !QDir().mkpath(parent)) {
        showError("Could not save project", "Could not create the selected folder.");
        return false;
    }
    const auto error = writeProject(path, project);
    if (!error.isEmpty()) {
        showError("Could not save project", error);
        return false;
    }
    path_ = QFileInfo(path).absoluteFilePath();
    savedContent_ = projectBytes(project, false);
    updateTitle();
    statusBar()->showMessage("Saved " + QFileInfo(path_).fileName(), 5000);
    canvas_->setFocus();
    return true;
}
bool ProjectWindow::saveProject() {
    if (!hasProject()) return false;
    canvas_->cancelPendingInput();
    return path_.isEmpty() ? saveProjectAs() : saveTo(path_);
}
bool ProjectWindow::saveProjectAs() {
    if (!hasProject()) return false;
    canvas_->cancelPendingInput();
    if (path_.isEmpty() && !QDir(projectFolder_).exists() && !QDir().mkpath(projectFolder_)) {
        showError("Could not save project", "Could not create the default project folder.");
        return false;
    }
    const auto start = path_.isEmpty() ? QDir(projectFolder_).filePath("Untitled.fontu") : path_;
    QFileDialog dialog(this, "Save fontu project", start, "fontu projects (*.fontu)");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix("fontu");
    if (dialog.exec() != QDialog::Accepted) return false;
    const auto path = dialog.selectedFiles().value(0);
    if (path.isEmpty()) return false;
    return saveTo(path);
}
bool ProjectWindow::confirmReplacement() {
    if (!isProjectDirty()) return true;
    const auto choice = QMessageBox::warning(this, "Unsaved changes", "Save changes to this fontu project?",
                                            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Save) return saveProject();
    return choice == QMessageBox::Discard;
}
void ProjectWindow::openProject() {
    canvas_->cancelPendingInput();
    const auto start = path_.isEmpty() ? (QDir(projectFolder_).exists() ? projectFolder_ : QFileInfo(projectFolder_).absolutePath()) : QFileInfo(path_).absolutePath();
    const auto path = QFileDialog::getOpenFileName(this, "Open fontu project", start, "fontu projects (*.fontu)");
    if (path.isEmpty()) return;
    auto result = readProject(path);
    if (const auto *error = std::get_if<QString>(&result)) {
        showError("Could not open project", *error);
        return;
    }
    if (!confirmReplacement()) return;
    installProject(std::get<ProjectSnapshot>(std::move(result)), QFileInfo(path).absoluteFilePath());
}
void ProjectWindow::closeEvent(QCloseEvent *event) {
    canvas_->cancelPendingInput();
    if (confirmReplacement()) event->accept();
    else event->ignore();
}
}
