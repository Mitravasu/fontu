#include "project_window.h"
#include "glyph_panel.h"
#include "guide_preview.h"
#include "project_creation.h"
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QClipboard>
#include <QTemporaryDir>
#include <QTimer>
#include <QRawFont>
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
        window.findChild<QSpinBox *>("unitsPerEm")->setValue(2048);
        window.findChild<QSpinBox *>("workspaceWidth")->setValue(1200);
        window.findChild<QSpinBox *>("workspaceHeight")->setValue(1400);
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
    void referenceGlyphToggleIsViewOnly() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        prepare(window);
        auto *reference = window.findChild<QAction *>("referenceGlyph");
        QVERIFY(reference && reference->isCheckable() && reference->isEnabled());
        QVERIFY(reference->isChecked() && window.canvas().referenceVisible());
        QVERIFY(!window.canvas().referencePath().isEmpty());
        reference->trigger();
        QVERIFY(!window.canvas().referenceVisible());
        QVERIFY(!window.isProjectDirty());
        QVERIFY(!window.canvas().canUndo());
        window.canvas().setReferenceVisible(true);
        QVERIFY(reference->isChecked());
        QVERIFY(window.close());
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
    void guidesStayValidWhileEditingAndDragging() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        window.resize(800, 600); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *preset = window.findChild<QComboBox *>("creationPreset");
        auto *preview = window.findChild<GuidePreview *>("guidePreview");
        auto *error = window.findChild<QLabel *>("creationError");
        auto *note = window.findChild<QLabel *>("guideNote");
        auto *height = window.findChild<QSpinBox *>("workspaceHeight");
        auto *upm = window.findChild<QSpinBox *>("unitsPerEm");
        auto *proportional = window.findChild<QCheckBox *>("keepProportions");
        const auto value = [&](const char *name) { return window.findChild<QDoubleSpinBox *>(name)->value(); };
        const auto setGuide = [&](const char *name, double v) { window.findChild<QDoubleSpinBox *>(name)->setValue(v); };
        const auto choose = [&](int index) { preset->setCurrentIndex(index); emit preset->activated(index); };
        QVERIFY(preview && preview->isVisible() && proportional->isChecked());
        // Every preset is valid, and the preview mirrors it.
        for (int index = 0; index < preset->count(); ++index) {
            choose(index);
            const auto &expected = creationPresets()[std::size_t(index)].definition;
            QCOMPARE(preview->definition().bounds, expected.bounds);
            QCOMPARE(preview->definition().ascender, expected.ascender);
            QVERIFY(preview->invalidGuides().isEmpty());
            QVERIFY(!error->isVisible());
        }
        // UPM scales the workspace and guides together: Classic 1000 at 2048 UPM matches the 2048 preset.
        choose(0);
        upm->setValue(2048);
        QCOMPARE(preview->definition().bounds, QRectF(-1024, -512, 2048, 2048));
        QCOMPARE(value("ascender"), 1536.); QCOMPARE(value("capHeight"), 1434.);
        QCOMPARE(value("xHeight"), 1024.); QCOMPARE(value("descender"), -512.);
        upm->setValue(20); upm->setValue(1000); // Scaling works from an anchor, so it never drifts.
        QCOMPARE(preview->definition().bounds, QRectF(-500, -250, 1000, 1000));
        QCOMPARE(value("capHeight"), 700.);
        // Height moves guides proportionally, losslessly.
        choose(1);
        height->setValue(4096);
        QCOMPARE(value("ascender"), 3072.); QCOMPARE(value("capHeight"), 2868.); QCOMPARE(value("descender"), -1024.);
        height->setValue(16); height->setValue(2048);
        QCOMPARE(value("ascender"), 1536.); QCOMPARE(value("capHeight"), 1434.);
        QVERIFY(!validateWorkspace(preview->definition()));
        // Out-of-workspace typing is clamped with a gentle note, never an error.
        setGuide("ascender", 1800);
        QCOMPARE(value("ascender"), 1536.);
        QVERIFY(note->isVisible() && note->text().contains("Ascender"));
        QVERIFY(!error->isVisible());
        // Without proportions, guides hold their values; a smaller workspace limits them only temporarily.
        proportional->setChecked(false);
        height->setValue(1000);
        QCOMPARE(value("ascender"), 750.); QCOMPARE(value("xHeight"), 750.);
        QVERIFY(note->isVisible() && note->text().contains("Raise Height"));
        QVERIFY(!validateWorkspace(preview->definition()));
        height->setValue(2048);
        QCOMPARE(value("ascender"), 1536.); QCOMPARE(value("xHeight"), 1024.);
        QVERIFY(!note->isVisible());
        proportional->setChecked(true);
        // Only contradictory order blocks, naming and highlighting the fields; the label is not clipped.
        setGuide("ascender", 1400);
        QVERIFY(error->isVisible());
        QVERIFY(error->text().startsWith("Cap height (1434) is above Ascender (1400)."));
        QVERIFY(window.findChild<QDoubleSpinBox *>("capHeight")->property("invalid").toBool());
        QVERIFY(window.findChild<QDoubleSpinBox *>("ascender")->property("invalid").toBool());
        QCOMPARE(preview->invalidGuides(), QStringList({"Cap height", "Ascender"}));
        QVERIFY(error->geometry().left() >= 20);
        QVERIFY(error->mapTo(&window, QPoint(error->width(), 0)).x() <= window.width());
        QTest::mouseClick(window.findChild<QPushButton *>("createProject"), Qt::LeftButton);
        QVERIFY(!window.hasProject());
        setGuide("ascender", 1536);
        QVERIFY(!error->isVisible());
        QVERIFY(!window.findChild<QDoubleSpinBox *>("capHeight")->property("invalid").toBool());
        // Dragging lines in the preview updates fields live and can only produce valid guides.
        window.findChild<QScrollArea *>("creationScroll")->ensureWidgetVisible(preview);
        const auto drag = [&](const char *name, double dy) {
            const auto start = preview->guidePoint(name).toPoint();
            QTest::mousePress(preview, Qt::LeftButton, {}, start);
            QTest::mouseMove(preview, start + QPoint(0, int(dy / 2)));
            QTest::mouseMove(preview, start + QPoint(0, int(dy)));
            QTest::mouseRelease(preview, Qt::LeftButton, {}, start + QPoint(0, int(dy)));
        };
        const double before = value("xHeight");
        drag("x-height", -10);
        QVERIFY(value("xHeight") > before && value("xHeight") < value("capHeight"));
        QCOMPARE(preview->definition().xHeight, value("xHeight"));
        drag("Ascender", -500);
        QCOMPARE(value("ascender"), 1536.);
        drag("x-height", -500);
        QCOMPARE(value("xHeight"), value("capHeight"));
        drag("Baseline", 500);
        QCOMPARE(value("baseline"), value("descender"));
        drag("Descender", 500);
        QCOMPARE(value("descender"), -512.);
        QVERIFY(!validateWorkspace(preview->definition()));
        QVERIFY(!error->isVisible());
        // The side bearing scales with Width, clamps to half of it, and drags horizontally from either line.
        auto *width = window.findChild<QSpinBox *>("workspaceWidth");
        QCOMPARE(value("sideBearing"), 205.);
        width->setValue(4096);
        QCOMPARE(value("sideBearing"), 410.);
        width->setValue(2048);
        QCOMPARE(value("sideBearing"), 205.);
        setGuide("sideBearing", 5000);
        QCOMPARE(value("sideBearing"), 1024.);
        QVERIFY(note->isVisible() && note->text().contains("Raise Width"));
        setGuide("sideBearing", 205);
        const auto side = preview->sideBearingPoint().toPoint();
        QTest::mousePress(preview, Qt::LeftButton, {}, side);
        QTest::mouseMove(preview, side + QPoint(5, 0));
        QTest::mouseMove(preview, side + QPoint(10, 0));
        QTest::mouseRelease(preview, Qt::LeftButton, {}, side + QPoint(10, 0));
        QVERIFY(value("sideBearing") > 205);
        QCOMPARE(preview->definition().sideBearing, value("sideBearing"));
        QVERIFY(!validateWorkspace(preview->definition()));
        window.findChild<QCheckBox *>("sideBearingEnabled")->setChecked(false);
        QVERIFY(!preview->definition().guideEnabled.sideBearing);
        window.findChild<QCheckBox *>("ascenderEnabled")->setChecked(false);
        QVERIFY(!preview->definition().guideEnabled.ascender);
        QTest::mouseClick(window.findChild<QPushButton *>("createProject"), Qt::LeftButton);
        QVERIFY(window.hasProject());
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
        window.findChild<QDoubleSpinBox *>("ascender")->setValue(600); // Below cap height: contradictory, so blocked.
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
    void exportOtfKeepsDirtyStateAndShowsWarnings() {
        QTemporaryDir directory;
        ProjectWindow window(nullptr, directory.filePath("fontu"));
        prepare(window);
        QVERIFY(window.findChild<QAction *>("exportOtf")->isEnabled());
        auto snapshot = window.canvas().projectSnapshot();
        snapshot.glyphs[0].shapes.push_back({1, makeShape(ShapeKind::Rectangle, {-200, 100, 100, 100}), ShapePaint::Filled});
        window.canvas().restoreProject(snapshot);
        chooseFile(directory.filePath("project.fontu"));
        QVERIFY(window.saveProject());
        chooseFile(directory.filePath("Clean Face"), false, directory.path());
        QVERIFY(window.exportFontAs());
        QVERIFY(!window.isProjectDirty());
        QCOMPARE(window.projectPath(), directory.filePath("project.fontu"));
        QRawFont clean(directory.filePath("Clean Face.otf"), 1000);
        QCOMPARE(clean.familyName(), QString("Clean Face"));
        QCOMPARE(clean.glyphIndexesForString("0"), QList<quint32>{1});
        QCOMPARE(clean.pathForGlyph(1).boundingRect(), QRectF(300, -200, 100, 100));

        snapshot = window.canvas().projectSnapshot();
        snapshot.glyphs[1].shapes.push_back({9, makePenPath({{{-100, 0}, {}}, {{100, 100}, {}}}), ShapePaint::Stroke});
        window.canvas().restoreProject(snapshot);
        QVERIFY(window.isProjectDirty());
        const auto before = projectBytes(window.canvas().projectSnapshot());
        QString shown;
        chooseFile(directory.filePath("Stroked.otf"));
        whenModal<QMessageBox>([&shown](QMessageBox *box) {
            shown = box->text() + "\n" + box->informativeText();
            box->accept();
        });
        QVERIFY(window.exportFontAs());
        QCOMPARE(shown, QString("Exported Stroked.otf with 1 warning(s).\n"
                                "“1”: skipped 1 stroke shape(s). Strokes have no width in the font, so only filled shapes export."));
        QVERIFY(window.isProjectDirty());
        QCOMPARE(projectBytes(window.canvas().projectSnapshot()), before);
        QVERIFY(QFile::exists(directory.filePath("Stroked.otf")));
        choosePrompt(QMessageBox::Discard);
        QVERIFY(window.close());
    }
};
QTEST_MAIN(ProjectWindowTests)
#include "project_window_tests.moc"
