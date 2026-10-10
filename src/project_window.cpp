#include "project_window.h"
#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>

namespace fontu {
ProjectWindow::ProjectWindow(QWidget *parent, QString projectFolder)
    : QMainWindow(parent), canvas_(new Canvas(this)), projectFolder_(std::move(projectFolder)) {
    setCentralWidget(canvas_);
    projectStatus_ = new QLabel(this);
    projectStatus_->setObjectName("projectStatus");
    statusBar()->addPermanentWidget(projectStatus_);
    savedContent_ = projectBytes(canvas_->projectSnapshot(), false);
    auto *file = menuBar()->addMenu("&File");
    auto *open = file->addAction("&Open...");
    open->setObjectName("openProject");
    open->setShortcuts(QKeySequence::Open);
    auto *save = file->addAction("&Save");
    save->setObjectName("saveProject");
    save->setShortcuts(QKeySequence::Save);
    auto *saveAs = file->addAction("Save &As...");
    saveAs->setObjectName("saveProjectAs");
    saveAs->setShortcuts(QKeySequence::SaveAs);
    for (auto *action : {open, save, saveAs})
        action->setToolTip("File actions cancel unfinished input. Completed drawings are preserved.");
    connect(open, &QAction::triggered, this, &ProjectWindow::openProject);
    connect(save, &QAction::triggered, this, &ProjectWindow::saveProject);
    connect(saveAs, &QAction::triggered, this, &ProjectWindow::saveProjectAs);
    connect(canvas_, &Canvas::historyChanged, this, &ProjectWindow::updateTitle);
    connect(canvas_, &Canvas::glyphsChanged, this, &ProjectWindow::updateTitle);
    updateTitle();
}
bool ProjectWindow::isProjectDirty() const {
    return projectBytes(canvas_->projectSnapshot(), false) != savedContent_;
}
void ProjectWindow::updateTitle() {
    const bool dirty = isProjectDirty();
    setWindowModified(dirty);
    projectStatus_->setText(dirty ? QStringLiteral("Unsaved changes")
                                : path_.isEmpty() ? QStringLiteral("Untitled") : QStringLiteral("Saved"));
    setWindowTitle((path_.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(path_).fileName())
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
    canvas_->cancelPendingInput();
    return path_.isEmpty() ? saveProjectAs() : saveTo(path_);
}
bool ProjectWindow::saveProjectAs() {
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
    auto project = std::get<ProjectSnapshot>(std::move(result));
    savedContent_ = projectBytes(project, false);
    path_ = QFileInfo(path).absoluteFilePath();
    canvas_->restoreProject(std::move(project));
    updateTitle();
    canvas_->setFocus();
}
void ProjectWindow::closeEvent(QCloseEvent *event) {
    canvas_->cancelPendingInput();
    if (confirmReplacement()) event->accept();
    else event->ignore();
}
}
