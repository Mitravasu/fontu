#include "creation_page.h"
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
const std::array<GuideControl, 5> guideControls{{
    {"Baseline", "baseline", &GlyphDocumentDefinition::baseline, &GuideEnablement::baseline},
    {"x-height", "xHeight", &GlyphDocumentDefinition::xHeight, &GuideEnablement::xHeight},
    {"Cap height", "capHeight", &GlyphDocumentDefinition::capHeight, &GuideEnablement::capHeight},
    {"Ascender", "ascender", &GlyphDocumentDefinition::ascender, &GuideEnablement::ascender},
    {"Descender", "descender", &GlyphDocumentDefinition::descender, &GuideEnablement::descender}}};
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
    form->addRow("Units per em", unitsPerEm_);
    span_ = wrapped({}, workspace); span_->setObjectName("workspaceSpan"); form->addRow(span_);
    form->addRow(wrapped("Dimensions and UPM are independent. These drawing bounds are not glyph advance widths or exported line metrics. Presets change workspace and guides; characters stay selected.", workspace));
    layout->addWidget(workspace);
    auto *guides = new QGroupBox("Typography guides", content);
    auto *grid = new QGridLayout(guides);
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        const auto &row = guideControls[i];
        enabled_[i] = new QCheckBox(QString::fromUtf8(row.label), guides);
        enabled_[i]->setObjectName(QString::fromUtf8(row.name) + "Enabled");
        values_[i] = new QDoubleSpinBox(guides); values_[i]->setObjectName(row.name);
        values_[i]->setRange(-16384, 16384); values_[i]->setDecimals(3); values_[i]->setSuffix(" units");
        grid->addWidget(enabled_[i], int(i / 2), int(i % 2) * 2);
        grid->addWidget(values_[i], int(i / 2), int(i % 2) * 2 + 1);
    }
    grid->addWidget(wrapped("Checked guides are drawn and used by Geometry snapping. Values stay editable when unchecked. Keep all values ordered and inside the workspace.", guides), 3, 0, 1, 4);
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
    error_ = wrapped({}, this); error_->setObjectName("creationError"); error_->setStyleSheet("color: #b3261e;");
    outer->addWidget(error_);
    auto *buttons = new QHBoxLayout;
    auto *open = new QPushButton("Open existing project...", this); open->setObjectName("welcomeOpen");
    cancel_ = new QPushButton("Back to project", this); cancel_->setObjectName("cancelCreation");
    auto *create = new QPushButton("Create project", this); create->setObjectName("createProject"); create->setDefault(true);
    buttons->addWidget(open); buttons->addStretch(); buttons->addWidget(cancel_); buttons->addWidget(create); outer->addLayout(buttons);
    connect(open, &QPushButton::clicked, this, &CreationPage::openRequested);
    connect(cancel_, &QPushButton::clicked, this, &CreationPage::cancelRequested);
    connect(create, &QPushButton::clicked, this, &CreationPage::createRequested);
    connect(presets_, &QComboBox::activated, this, &CreationPage::applyPreset);
    connect(width_, &QSpinBox::valueChanged, this, &CreationPage::updateSpan);
    connect(height_, &QSpinBox::valueChanged, this, &CreationPage::updateSpan);
    connect(coverage_, &QComboBox::currentIndexChanged, this, &CreationPage::updateCoverage);
    connect(custom_, &QLineEdit::textChanged, this, &CreationPage::updateCoverage);
    applyPreset(0); updateCoverage(); setCanCancel(false);
}
ProjectCreationDefinition CreationPage::request() const {
    ProjectCreationDefinition result;
    auto &d = result.definition;
    d.bounds = {-width_->value() / 2., -height_->value() / 4., double(width_->value()), double(height_->value())};
    d.unitsPerEm = unitsPerEm_->value();
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        d.*(guideControls[i].value) = values_[i]->value();
        d.guideEnabled.*(guideControls[i].enabled) = enabled_[i]->isChecked();
    }
    if (!coverage_->currentData().isValid()) result.coverage = CustomCharacters{custom_->text()};
    else result.coverage = BuiltinCharacters(coverage_->currentData().toInt());
    return result;
}
void CreationPage::applyPreset(int index) {
    const auto &preset = creationPresets().at(std::size_t(index));
    const auto &d = preset.definition;
    width_->setValue(int(d.bounds.width())); height_->setValue(int(d.bounds.height())); unitsPerEm_->setValue(int(d.unitsPerEm));
    for (std::size_t i = 0; i < guideControls.size(); ++i) {
        values_[i]->setValue(d.*(guideControls[i].value));
        enabled_[i]->setChecked(d.guideEnabled.*(guideControls[i].enabled));
    }
    presetDescription_->setText(QString::fromUtf8(preset.description)); error_->clear(); updateSpan();
}
void CreationPage::updateSpan() {
    span_->setText(QString("Workspace Y spans %1 to %2. Positive Y points upward. Width and height accept 16 to 8192 whole font units.")
        .arg(-height_->value() / 4.).arg(height_->value() * .75));
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
    error_->setText(error.message);
    QWidget *target = error.field == CreationField::Dimensions ? static_cast<QWidget *>(width_)
        : error.field == CreationField::UnitsPerEm ? static_cast<QWidget *>(unitsPerEm_)
        : error.field == CreationField::Characters ? static_cast<QWidget *>(custom_) : static_cast<QWidget *>(values_[0]);
    findChild<QScrollArea *>()->ensureWidgetVisible(target);
    target->setFocus();
}
void CreationPage::setCanCancel(bool enabled) { cancel_->setVisible(enabled); error_->clear(); }
}
