#pragma once
#include "project_creation.h"
#include <QWidget>

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
namespace fontu { class GuidePreview; }
namespace fontu {
class CreationPage : public QWidget {
    Q_OBJECT
public:
    explicit CreationPage(QWidget *parent = nullptr);
    ProjectCreationDefinition request() const;
    GlyphDocumentDefinition definition() const;
    void showError(const CreationError &error);
    void setCanCancel(bool enabled);
signals:
    void createRequested();
    void openRequested();
    void cancelRequested();
private:
    void applyPreset(int index);
    void updateSpan();
    void updateCoverage();
    void refreshGuides();
    void setError(const QString &message, bool workspace = false);
    void dimensionsEdited();
    void unitsPerEmEdited();
    void guideEdited(std::size_t index);
    void sideBearingEdited();
    void setSideBearingValue(double value);
    void guideDragged(const QString &name, double value);
    void proportionalToggled(bool on);
    void syncGuides();
    void rebaseGuides();
    void setGuideValue(std::size_t index, double value);
    // A guide value as the user last set it, with the workspace extent it scales with at that moment:
    // height for the horizontal guides, width for the side bearing.
    // Displayed values derive from these so shrinking then growing the workspace is lossless.
    struct GuideIntent { double value = 0; int extent = 0; };
    struct DimensionAnchor { int width = 0, height = 0, unitsPerEm = 0; };
    QSpinBox *width_;
    QSpinBox *height_;
    QSpinBox *unitsPerEm_;
    std::array<QDoubleSpinBox *, 5> values_;
    std::array<QCheckBox *, 5> enabled_;
    QDoubleSpinBox *sideBearing_ = nullptr;
    QCheckBox *sideBearingEnabled_ = nullptr;
    QComboBox *presets_;
    QComboBox *coverage_;
    QLineEdit *custom_;
    QLabel *presetDescription_;
    QLabel *span_;
    QLabel *coverageDescription_;
    QLabel *error_;
    QPushButton *cancel_;
    GuidePreview *preview_ = nullptr;
    QCheckBox *proportional_ = nullptr;
    QLabel *guideNote_ = nullptr;
    std::array<GuideIntent, 5> intent_{};
    GuideIntent sideBearingIntent_;
    DimensionAnchor anchor_;
    bool syncing_ = false;
    bool workspaceError_ = false;
};
}
