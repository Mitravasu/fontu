#include "project_window.h"
#include "glyph_panel.h"
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QClipboard>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <memory>
using namespace fontu;
namespace {
template<class Dialog, class Callback>
void whenModal(Callback callback) {
    auto *timer = new QTimer(qApp);
    auto attempts = std::make_shared<int>(0);
    QObject::connect(timer, &QTimer::timeout, [timer, callback, attempts] {
        if (auto *dialog = qobject_cast<Dialog *>(QApplication::activeModalWidget())) {
            timer->stop();
            timer->deleteLater();
            callback(dialog);
        } else if (++*attempts > 2000) qFatal("Expected modal dialog did not appear");
    });
    timer->start(1);
}
void chooseFile(const QString &path, bool cancel = false, const QString &expectedFolder = {}) {
    whenModal<QFileDialog>([path, cancel, expectedFolder](QFileDialog *dialog) {
        if (!expectedFolder.isEmpty()) {
            QCOMPARE(dialog->directory().absolutePath(), expectedFolder);
        }
        if (cancel) dialog->reject();
        else {
            dialog->selectFile(path);
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
    });
}
void choosePrompt(QMessageBox::StandardButton button, const std::function<void()> &next = {}) {
    whenModal<QMessageBox>([button, next](QMessageBox *dialog) {
        if (next) next();
        QTest::mouseClick(dialog->button(button), Qt::LeftButton);
    });
}
void openFile(ProjectWindow &window, const QString &path, const std::function<void()> &after = {}) {
    whenModal<QFileDialog>([path, after](QFileDialog *dialog) {
        if (after) after();
        dialog->selectFile(path);
        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
    });
    window.openProject();
}
void draw(Canvas &canvas) {
    QTest::keyClick(&canvas, Qt::Key_R);
    const auto from = canvas.viewport().toScreen({-200, 100}).toPoint();
    const auto to = canvas.viewport().toScreen({-100, 200}).toPoint();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, from);
    QMouseEvent move(QEvent::MouseMove, to, canvas.mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&canvas, &move);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, to);
}
void shortcut(QWidget *widget, QKeySequence::StandardKey key) {
    widget->window()->activateWindow();
    widget->setFocus();
    QApplication::processEvents();
    const auto binding = QKeySequence::keyBindings(key).front()[0];
    QTest::keyClick(widget, Qt::Key(binding.key()), binding.keyboardModifiers());
    QApplication::processEvents();
}
void prepare(ProjectWindow &window) {
    window.resize(1100, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    QTest::mouseClick(window.findChild<QPushButton *>("createProject"), Qt::LeftButton);
    window.canvas().setFocus();
    QApplication::processEvents();
}
}
class ProjectWindowTests : public QObject {
    Q_OBJECT
private slots:
    void startupCustomCreationAndOpen() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        window.resize(800, 600); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(!window.hasProject());
        QVERIFY(!window.findChild<QAction *>("saveProject")->isEnabled());
        auto *create = window.findChild<QPushButton *>("createProject");
        auto *set = window.findChild<QComboBox *>("characterSet");
        set->setCurrentIndex(3);
        QTest::mouseClick(create, Qt::LeftButton);
        QVERIFY(!window.hasProject());
        QVERIFY(window.findChild<QLabel *>("creationError")->text().contains("at least one"));
        window.findChild<QLineEdit *>("customCharacters")->setText(QString::fromUtf8("A😀Aé😀 "));
        window.findChild<QSpinBox *>("workspaceWidth")->setValue(1200);
        window.findChild<QSpinBox *>("workspaceHeight")->setValue(1400);
        window.findChild<QSpinBox *>("unitsPerEm")->setValue(2048);
        window.findChild<QDoubleSpinBox *>("xHeight")->setValue(480);
        window.findChild<QCheckBox *>("xHeightEnabled")->setChecked(false);
        QTest::mouseClick(create, Qt::LeftButton);
        QApplication::processEvents();
        QVERIFY(window.hasProject());
        QCOMPARE(window.canvas().glyphs().size(), std::size_t(4));
        QCOMPARE(window.findChild<QListWidget *>("glyphPicker")->item(3)->text(), QString("Space"));
        QCOMPARE(window.canvas().document().definition().bounds, QRectF(-600, -350, 1200, 1400));
        QCOMPARE(window.canvas().document().definition().unitsPerEm, 2048.);
        QVERIFY(!window.canvas().document().definition().guideEnabled.xHeight);
        draw(window.canvas());
        chooseFile(directory.filePath("custom.fontu")); QVERIFY(window.saveProject());
        ProjectWindow reopened(nullptr, directory.filePath("fontu"));
        reopened.resize(800, 600); reopened.show();
        QVERIFY(QTest::qWaitForWindowExposed(&reopened));
        chooseFile(directory.filePath("custom.fontu"));
        QTest::mouseClick(reopened.findChild<QPushButton *>("welcomeOpen"), Qt::LeftButton);
        QVERIFY(reopened.hasProject());
        QCOMPARE(projectBytes(reopened.canvas().projectSnapshot()), projectBytes(window.canvas().projectSnapshot()));
        QVERIFY(reopened.close()); QVERIFY(window.close());
    }
    void oversizedCustomPasteIsRejected() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.findChild<QComboBox *>("characterSet")->setCurrentIndex(3);
        auto *field = window.findChild<QLineEdit *>("customCharacters");
        const auto pasted = QString(200000, QChar('a')) + 'b';
        QApplication::clipboard()->setText(pasted);
        shortcut(field, QKeySequence::Paste);
        QCOMPARE(field->text(), pasted);
        QTest::mouseClick(window.findChild<QPushButton *>("createProject"), Qt::LeftButton);
        QVERIFY(!window.hasProject());
        QVERIFY(window.findChild<QLabel *>("creationError")->text().contains("200000"));
        QVERIFY(window.close());
    }
    void newCancellationPreservesHistoryAndDirtyGates() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu")); prepare(window);
        draw(window.canvas());
        window.canvas().setActiveGlyph({2}); draw(window.canvas());
        const auto before = projectBytes(window.canvas().projectSnapshot());
        const auto view = window.canvas().viewport().transform();
        window.newProject(); QApplication::processEvents();
        QVERIFY(!window.findChild<QAction *>("saveProject")->isEnabled());
        auto *create = window.findChild<QPushButton *>("createProject");
        const auto undoBinding = QKeySequence::keyBindings(QKeySequence::Undo).front()[0];
        QTest::keyClick(create, Qt::Key(undoBinding.key()), undoBinding.keyboardModifiers());
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        window.findChild<QDoubleSpinBox *>("ascender")->setValue(900);
        QTest::mouseClick(create, Qt::LeftButton);
        QVERIFY(window.findChild<QLabel *>("creationError")->text().contains("Ascender"));
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        window.findChild<QDoubleSpinBox *>("ascender")->setValue(750);
        choosePrompt(QMessageBox::Cancel); QTest::mouseClick(create, Qt::LeftButton);
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        choosePrompt(QMessageBox::Save, [] { chooseFile({}, true); }); QTest::mouseClick(create, Qt::LeftButton);
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        QVERIFY(window.projectPath().isEmpty());
        QTest::mouseClick(window.findChild<QPushButton *>("cancelCreation"), Qt::LeftButton);
        QApplication::processEvents();
        QCOMPARE(window.canvas().viewport().transform(), view);
        QCOMPARE(window.canvas().activeGlyphId().value, std::uint64_t(2));
        window.canvas().undo(); QCOMPARE(window.canvas().document().shapes().size(), std::size_t(0));
        window.canvas().redo(); QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        window.newProject(); choosePrompt(QMessageBox::Save, [&] { chooseFile(directory.filePath("old.fontu")); });
        QTest::mouseClick(create, Qt::LeftButton);
        QVERIFY(QFile::exists(directory.filePath("old.fontu")));
        QVERIFY(window.projectPath().isEmpty()); QVERIFY(!window.isProjectDirty());
        QCOMPARE(window.canvas().glyphs().size(), std::size_t(10));
        QVERIFY(!window.canvas().canUndo());
        draw(window.canvas()); window.newProject();
        choosePrompt(QMessageBox::Discard); QTest::mouseClick(create, Qt::LeftButton);
        QCOMPARE(window.canvas().document().shapes().size(), std::size_t(0));
        QVERIFY(!window.isProjectDirty()); QVERIFY(window.close());
    }
    void saveDirtyUndoAndShortcuts() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        prepare(window);
        auto &canvas = window.canvas();
        QVERIFY(!window.isProjectDirty());
        draw(canvas);
        QCOMPARE(canvas.document().shapes().size(), std::size_t(1));
        QVERIFY(window.isProjectDirty());
        QVERIFY(window.isWindowModified());
        QCOMPARE(window.findChild<QLabel *>("projectStatus")->text(), QString("Unsaved changes"));
        chooseFile(directory.filePath("first"), false, directory.filePath("fontu"));
        shortcut(&canvas, QKeySequence::Save);
        QCOMPARE(window.projectPath(), directory.filePath("first.fontu"));
        QVERIFY(!window.isProjectDirty());
        QVERIFY(QFile::exists(window.projectPath()));
        QCOMPARE(window.findChild<QLabel *>("projectStatus")->text(), QString("Saved"));
        shortcut(&canvas, QKeySequence::Undo);
        QVERIFY(window.isProjectDirty());
        shortcut(&canvas, QKeySequence::Redo);
        QVERIFY(!window.isProjectDirty());
        canvas.setActiveGlyph({2});
        QVERIFY(!window.isProjectDirty());
        draw(canvas);
        QVERIFY(window.isProjectDirty());
        shortcut(&canvas, QKeySequence::Save);
        QVERIFY(!window.isProjectDirty());
        const auto result = readProject(window.projectPath());
        QVERIFY(std::holds_alternative<ProjectSnapshot>(result));
        QCOMPARE(std::get<ProjectSnapshot>(result).glyphs[1].shapes.size(), std::size_t(1));
        chooseFile(directory.filePath("copy.fontu"));
        shortcut(&canvas, QKeySequence::SaveAs);
        QCOMPARE(window.projectPath(), directory.filePath("copy.fontu"));
        QVERIFY(window.close());
    }
    void reopenCollectionMetricsAndInvalidFiles() {
        QTemporaryDir directory;
        ProjectSnapshot project{GlyphDocumentDefinition{}, {{{{99}, "custom A"}, {{8, makeShape(ShapeKind::Ellipse, {0, 0, 100, 100}), ShapePaint::Filled}}}, {{{7}, "B"}, {}}}, {99}};
        project.definition.unitsPerEm = 2048;
        QVERIFY(writeProject(directory.filePath("load.fontu"), project).isEmpty());
        QFile invalid(directory.filePath("invalid.fontu"));
        QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("invalid"); invalid.close();
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        prepare(window);
        chooseFile(directory.filePath("load.fontu"));
        shortcut(&window.canvas(), QKeySequence::Open);
        QCOMPARE(window.canvas().activeGlyphId().value, std::uint64_t(99));
        QCOMPARE(window.canvas().document().definition().unitsPerEm, 2048.);
        QCOMPARE(window.canvas().document().shapes()[0].id, ShapeId(8));
        QVERIFY(!window.canvas().canUndo());
        QVERIFY(!window.canvas().document().selection());
        auto *picker = window.findChild<QListWidget *>("glyphPicker");
        QCOMPARE(picker->count(), 2);
        QCOMPARE(picker->currentItem()->text(), QString("custom A"));
        draw(window.canvas());
        const auto before = projectBytes(window.canvas().projectSnapshot());
        const auto path = window.projectPath();
        openFile(window, invalid.fileName(), [] { choosePrompt(QMessageBox::Ok); });
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        QCOMPARE(window.projectPath(), path);
        QVERIFY(window.isProjectDirty());
        QVERIFY(window.canvas().canUndo());
        choosePrompt(QMessageBox::Discard);
        QVERIFY(window.close());
    }
    void unsavedOpenAndCloseGates() {
        QTemporaryDir directory;
        Canvas blank;
        QVERIFY(writeProject(directory.filePath("blank.fontu"), blank.projectSnapshot()).isEmpty());
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        prepare(window);
        draw(window.canvas());
        const auto before = projectBytes(window.canvas().projectSnapshot());
        openFile(window, directory.filePath("blank.fontu"), [] { choosePrompt(QMessageBox::Cancel); });
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        openFile(window, directory.filePath("blank.fontu"), [] {
            choosePrompt(QMessageBox::Save, [] { chooseFile({}, true); });
        });
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        QVERIFY(window.projectPath().isEmpty());
        choosePrompt(QMessageBox::Cancel);
        QVERIFY(!window.close());
        QVERIFY(window.isVisible());
        choosePrompt(QMessageBox::Save, [&] { chooseFile(directory.filePath("close.fontu")); });
        QVERIFY(window.close());
        QVERIFY(QFile::exists(directory.filePath("close.fontu")));
        QVERIFY(!window.isProjectDirty());
    }
    void saveFailurePreservesContentAndDirtyState() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        prepare(window);
        chooseFile(directory.filePath("blocked.fontu"));
        QVERIFY(window.saveProject());
        const auto path = window.projectPath();
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path));
        draw(window.canvas());
        const auto before = projectBytes(window.canvas().projectSnapshot());
        choosePrompt(QMessageBox::Ok);
        QVERIFY(!window.saveProject());
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        QCOMPARE(window.projectPath(), path);
        QVERIFY(window.isProjectDirty());
        choosePrompt(QMessageBox::Discard);
        QVERIFY(window.close());
    }
};
QTEST_MAIN(ProjectWindowTests)
#include "project_window_tests.moc"
