#include "creation_page.h"
#include "guide_preview.h"
#include <QIcon>
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>

namespace fontu {
namespace {
struct GuideControl {
    const char *label;
    const char *name;
    double GlyphDocumentDefinition::*value;
    bool GuideEnablement::*enabled;
};
// Top-to-bottom, matching the preview. Labels equal TypographyGuide names.
const std::array<GuideControl, 5> guideControls{{
    {"Ascender", "ascender", &GlyphDocumentDefinition::ascender, &GuideEnablement::ascender},
    {"Cap height", "capHeight", &GlyphDocumentDefinition::capHeight, &GuideEnablement::capHeight},
    {"x-height", "xHeight", &GlyphDocumentDefinition::xHeight, &GuideEnablement::xHeight},
    {"Baseline", "baseline", &GlyphDocumentDefinition::baseline, &GuideEnablement::baseline},
    {"Descender", "descender", &GlyphDocumentDefinition::descender, &GuideEnablement::descender}}};
const char *invalidFieldStyle = "border: 2px solid #e0352b; border-radius: 4px;";
QLabel *wrapped(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    return label;
}
}
CreationPage::CreationPage(QWidget *parent) : QWidget(parent) {
    setObjectName("creationPage");
    auto *outer = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("creationScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);
    auto *content = new QWidget(scroll);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 20, 28, 20);
    layout->setSpacing(12);
    auto *title = new QLabel("Start a glyph project", content);
    auto font = title->font(); font.setPointSize(24); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    layout->addWidget(wrapped("Choose your drawing workspace and characters, or open a saved .fontu project.", content));
    auto *workspace = new QGroupBox("Workspace", content);
    auto *form = new QFormLayout(workspace);
    presets_ = new QComboBox(workspace); presets_->setObjectName("creationPreset");
    for (const auto &preset : creationPresets()) presets_->addItem(QString::fromUtf8(preset.name));
    form->addRow("Example preset", presets_);
    presetDescription_ = wrapped({}, workspace); form->addRow(presetDescription_);
    const auto units = [workspace](const char *name) {
        auto *box = new QSpinBox(workspace); box->setObjectName(name);
        box->setRange(16, 16384); box->setSuffix(" units"); return box;
    };
    width_ = units("workspaceWidth"); height_ = units("workspaceHeight"); unitsPerEm_ = units("unitsPerEm");
    width_->setMaximum(8192); height_->setMaximum(8192);
    auto *sizes = new QHBoxLayout;
    sizes->addWidget(new QLabel("Width", workspace)); sizes->addWidget(width_);
    sizes->addWidget(new QLabel("Height", workspace)); sizes->addWidget(height_);
    form->addRow("Font units", sizes);
    proportional_ = new QCheckBox("Keep proportions", workspace); proportional_->setObjectName("keepProportions");
    proportional_->setChecked(true);
    proportional_->setToolTip("On: changing UPM scales width, height and guides together, and changing Height moves the guides with it.\nOff: every value is independent; guides stay put and are limited to the workspace.");
    auto *upmRow = new QHBoxLayout;
    upmRow->addWidget(unitsPerEm_, 1); upmRow->addWidget(proportional_);
    form->addRow("Units per em", upmRow);
    span_ = wrapped({}, workspace); span_->setObjectName("workspaceSpan"); form->addRow(span_);
    form->addRow(wrapped("With Keep proportions on, changing UPM resizes the workspace and guides together, and changing Height moves the guides with it. These drawing bounds are not glyph advance widths or exported line metrics. Presets change workspace and guides; characters stay selected.", workspace));
    layout->addWidget(workspace);
    auto *guides = new QGroupBox("Typography guides", content);
    auto *guidesLayout = new QVBoxLayout(guides);
    auto *guidesRow = new QHBoxLayout;
    auto *grid = new QGridLayout;
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        const auto &row = guideControls[i];
        const auto label = QString::fromUtf8(row.label);
        enabled_[i] = new QCheckBox(label, guides);
        enabled_[i]->setObjectName(QString::fromUtf8(row.name) + "Enabled");
        enabled_[i]->setIcon(QIcon(guideSwatch(label, devicePixelRatioF())));
        enabled_[i]->setIconSize({18, 12});
        values_[i] = new QDoubleSpinBox(guides); values_[i]->setObjectName(row.name);
        values_[i]->setRange(-16384, 16384); values_[i]->setDecimals(3); values_[i]->setSuffix(" units");
        grid->addWidget(enabled_[i], int(i), 0);
        grid->addWidget(values_[i], int(i), 1);
        connect(values_[i], &QDoubleSpinBox::valueChanged, this, [this, i] { guideEdited(i); });
        connect(enabled_[i], &QCheckBox::toggled, this, &CreationPage::refreshGuides);
    }
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    guidesRow->addLayout(grid, 1);
    preview_ = new GuidePreview(guides);
    preview_->setMinimumWidth(280);
    connect(preview_, &GuidePreview::guideDragged, this, &CreationPage::guideDragged);
    guidesRow->addWidget(preview_, 1);
    guidesLayout->addLayout(guidesRow);
    guideNote_ = wrapped({}, guides); guideNote_->setObjectName("guideNote");
    guideNote_->setForegroundRole(QPalette::PlaceholderText); guideNote_->hide();
    guidesLayout->addWidget(guideNote_);
    guidesLayout->addWidget(wrapped("Drag the lines in the preview or type exact values. Checked guides are drawn and used by Geometry snapping; unchecked values are kept. Guides stay inside the workspace.", guides));
    layout->addWidget(guides);
    auto *characters = new QGroupBox("Characters", content);
    auto *characterLayout = new QVBoxLayout(characters);
    coverage_ = new QComboBox(characters); coverage_->setObjectName("characterSet");
    coverage_->addItem("Numbers", int(BuiltinCharacters::Numbers));
    coverage_->addItem("Letters", int(BuiltinCharacters::Letters));
    coverage_->addItem("Programming", int(BuiltinCharacters::Programming));
    coverage_->addItem("Custom"); characterLayout->addWidget(coverage_);
    coverageDescription_ = wrapped({}, characters); coverageDescription_->setObjectName("characterCoverage"); characterLayout->addWidget(coverageDescription_);
    custom_ = new QLineEdit(characters); custom_->setObjectName("customCharacters");
    custom_->setPlaceholderText("Type or paste characters. Repeated characters become one glyph.");
    custom_->setMaxLength(200001); characterLayout->addWidget(custom_);
    layout->addWidget(characters);
    layout->addStretch();
    scroll->setWidget(content);
    // Footer shares the content's horizontal margins so the error is never flush with the window edge.
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *footer = new QVBoxLayout;
    footer->setContentsMargins(28, 8, 28, 16);
    footer->setSpacing(8);
    outer->addLayout(footer);
    error_ = wrapped({}, this); error_->setObjectName("creationError"); error_->setStyleSheet("color: #e0352b;");
    error_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    error_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    error_->hide();
    footer->addWidget(error_);
    auto *buttons = new QHBoxLayout;
    auto *open = new QPushButton("Open existing project...", this); open->setObjectName("welcomeOpen");
    cancel_ = new QPushButton("Back to project", this); cancel_->setObjectName("cancelCreation");
    auto *create = new QPushButton("Create project", this); create->setObjectName("createProject"); create->setDefault(true);
    buttons->addWidget(open); buttons->addStretch(); buttons->addWidget(cancel_); buttons->addWidget(create); footer->addLayout(buttons);
    connect(open, &QPushButton::clicked, this, &CreationPage::openRequested);
    connect(cancel_, &QPushButton::clicked, this, &CreationPage::cancelRequested);
    connect(create, &QPushButton::clicked, this, [this] {
        // Spin boxes commit on Enter or focus change; a click on a non-focus-taking button must commit too.
        for (auto *box : {width_, height_, unitsPerEm_}) box->interpretText();
        for (auto *box : values_) box->interpretText();
        emit createRequested();
    });
    connect(presets_, &QComboBox::activated, this, &CreationPage::applyPreset);
    connect(width_, &QSpinBox::valueChanged, this, &CreationPage::dimensionsEdited);
    connect(height_, &QSpinBox::valueChanged, this, &CreationPage::dimensionsEdited);
    connect(unitsPerEm_, &QSpinBox::valueChanged, this, &CreationPage::unitsPerEmEdited);
    connect(proportional_, &QCheckBox::toggled, this, &CreationPage::proportionalToggled);
    connect(coverage_, &QComboBox::currentIndexChanged, this, &CreationPage::updateCoverage);
    connect(custom_, &QLineEdit::textChanged, this, &CreationPage::updateCoverage);
    applyPreset(0); updateCoverage(); setCanCancel(false);
}
ProjectCreationDefinition CreationPage::request() const {
    ProjectCreationDefinition result;
    result.definition = definition();
    if (!coverage_->currentData().isValid()) result.coverage = CustomCharacters{custom_->text()};
    else result.coverage = BuiltinCharacters(coverage_->currentData().toInt());
    return result;
}
GlyphDocumentDefinition CreationPage::definition() const {
    GlyphDocumentDefinition d;
    d.bounds = {-width_->value() / 2., -height_->value() / 4., double(width_->value()), double(height_->value())};
    d.unitsPerEm = unitsPerEm_->value();
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        d.*(guideControls[i].value) = values_[i]->value();
        d.guideEnabled.*(guideControls[i].enabled) = enabled_[i]->isChecked();
    }
    return d;
}
void CreationPage::applyPreset(int index) {
    const auto &preset = creationPresets().at(std::size_t(index));
    const auto &d = preset.definition;
    syncing_ = true;
    width_->setValue(int(d.bounds.width())); height_->setValue(int(d.bounds.height())); unitsPerEm_->setValue(int(d.unitsPerEm));
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        values_[i]->setValue(d.*(guideControls[i].value));
        enabled_[i]->setChecked(d.guideEnabled.*(guideControls[i].enabled));
    }
    syncing_ = false;
    anchor_ = {width_->value(), height_->value(), unitsPerEm_->value()};
    rebaseGuides();
    guideNote_->clear(); guideNote_->hide();
    presetDescription_->setText(QString::fromUtf8(preset.description)); setError({}); updateSpan(); refreshGuides();
}
void CreationPage::setGuideValue(std::size_t index, double value) {
    const bool was = syncing_;
    syncing_ = true;
    values_[index]->setValue(value);
    syncing_ = was;
}
void CreationPage::rebaseGuides() {
    for (std::size_t i = 0; i < guideControls.size(); ++i) intent_[i] = {values_[i]->value(), height_->value()};
}
void CreationPage::dimensionsEdited() {
    if (syncing_) return;
    anchor_ = {width_->value(), height_->value(), unitsPerEm_->value()};
    syncGuides();
    updateSpan();
}
void CreationPage::unitsPerEmEdited() {
    if (syncing_) return;
    if (proportional_->isChecked() && anchor_.unitsPerEm > 0) {
        // Scale from the anchor, not the previous value, so repeated changes never drift.
        const double ratio = double(unitsPerEm_->value()) / anchor_.unitsPerEm;
        const auto scaled = [ratio](int extent) { return std::clamp(int(std::lround(extent * ratio)), 16, 8192); };
        syncing_ = true;
        width_->setValue(scaled(anchor_.width)); height_->setValue(scaled(anchor_.height));
        syncing_ = false;
        syncGuides();
    } else {
        anchor_ = {width_->value(), height_->value(), unitsPerEm_->value()};
    }
    updateSpan();
}
void CreationPage::proportionalToggled(bool) {
    anchor_ = {width_->value(), height_->value(), unitsPerEm_->value()};
    rebaseGuides();
}
void CreationPage::syncGuides() {
    const int height = height_->value();
    const double low = -height / 4., high = height * .75;
    QStringList limited;
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        const auto &intent = intent_[i];
        double wanted = intent.value;
        if (proportional_->isChecked() && intent.height != height && intent.height > 0)
            wanted = std::round(intent.value * height / intent.height);
        const double shown = std::clamp(wanted, low, high);
        if (shown != wanted && !proportional_->isChecked())
            limited << QString("%1 %2 → %3").arg(QString::fromUtf8(guideControls[i].label)).arg(wanted).arg(shown);
        setGuideValue(i, shown);
    }
    guideNote_->setText(limited.isEmpty() ? QString()
        : "Limited to the workspace for now: " + limited.join(", ") + ". Raise Height to restore.");
    guideNote_->setVisible(!limited.isEmpty());
    refreshGuides();
}
void CreationPage::guideEdited(std::size_t index) {
    if (syncing_) return;
    const int height = height_->value();
    const double low = -height / 4., high = height * .75;
    const double typed = values_[index]->value();
    const double shown = std::clamp(typed, low, high);
    if (shown != typed) {
        setGuideValue(index, shown);
        guideNote_->setText(QString("%1 was set to %2, the workspace %3. Raise Height for more room.")
            .arg(QString::fromUtf8(guideControls[index].label)).arg(shown).arg(typed > high ? "top" : "bottom"));
        guideNote_->show();
    } else {
        guideNote_->clear(); guideNote_->hide();
    }
    intent_[index] = {shown, height};
    refreshGuides();
}
void CreationPage::guideDragged(const QString &name, double value) {
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        if (QString::fromUtf8(guideControls[i].label) != name) continue;
        setGuideValue(i, value);
        intent_[i] = {value, height_->value()};
        guideNote_->clear(); guideNote_->hide();
        refreshGuides();
        return;
    }
}
void CreationPage::updateSpan() {
    span_->setText(QString("Workspace Y spans %1 to %2. Positive Y points upward. Width and height accept 16 to 8192 whole font units.")
        .arg(-height_->value() / 4.).arg(height_->value() * .75));
}
void CreationPage::refreshGuides() {
    if (!preview_) return; // Controls are still being built.
    const auto d = definition();
    const auto error = validateWorkspace(d);
    const QStringList invalid = error ? error->guides : QStringList();
    preview_->setDefinition(d, invalid);
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        const bool bad = invalid.contains(QString::fromUtf8(guideControls[i].label));
        values_[i]->setProperty("invalid", bad);
        values_[i]->setStyleSheet(bad ? invalidFieldStyle : "");
        values_[i]->setToolTip(bad ? error->message : QString());
    }
    // Keep a shown workspace error in step with the values: update it while it applies, clear it once fixed.
    if (error && error->field == CreationField::Guides) setError(error->message, true);
    else if (workspaceError_) setError({});
}
void CreationPage::setError(const QString &message, bool workspace) {
    error_->setText(message);
    error_->setVisible(!message.isEmpty());
    workspaceError_ = workspace && !message.isEmpty();
}
void CreationPage::updateCoverage() {
    const bool custom = !coverage_->currentData().isValid();
    custom_->setVisible(custom);
    if (!custom) {
        coverageDescription_->setText(characterCoverageDescription(BuiltinCharacters(coverage_->currentData().toInt())));
        return;
    }
    const auto text = custom_->text();
    if (text.size() > 200000) {
        coverageDescription_->setText("Custom text exceeds 200000 UTF-16 units. Shorten it before creating.");
        return;
    }
    QSet<char32_t> distinct;
    for (const auto scalar : text.toUcs4()) distinct.insert(scalar);
    coverageDescription_->setText(QString("%1 unique code points. One glyph per Unicode code point, in first appearance order. Spaces and combining marks count. No normalization. Up to 4096 unique characters and 200000 UTF-16 units.").arg(distinct.size()));
}
void CreationPage::showError(const CreationError &error) {
    setError(error.message, error.field == CreationField::Guides);
    QWidget *target = error.field == CreationField::Dimensions ? static_cast<QWidget *>(width_)
        : error.field == CreationField::UnitsPerEm ? static_cast<QWidget *>(unitsPerEm_)
        : error.field == CreationField::Characters ? static_cast<QWidget *>(custom_) : static_cast<QWidget *>(values_[0]);
    for (std::size_t i = 0; error.field == CreationField::Guides && i < guideControls.size(); ++i)
        if (error.guides.contains(QString::fromUtf8(guideControls[i].label))) { target = values_[i]; break; }
    findChild<QScrollArea *>()->ensureWidgetVisible(target);
    target->setFocus();
}
void CreationPage::setCanCancel(bool enabled) { cancel_->setVisible(enabled); setError({}); }
}
