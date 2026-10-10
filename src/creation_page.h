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
namespace fontu {
class CreationPage : public QWidget {
    Q_OBJECT
public:
    explicit CreationPage(QWidget *parent = nullptr);
    ProjectCreationDefinition request() const;
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
    QSpinBox *width_;
    QSpinBox *height_;
    QSpinBox *unitsPerEm_;
    std::array<QDoubleSpinBox *, 5> values_;
    std::array<QCheckBox *, 5> enabled_;
    QComboBox *presets_;
    QComboBox *coverage_;
    QLineEdit *custom_;
    QLabel *presetDescription_;
    QLabel *span_;
    QLabel *coverageDescription_;
    QLabel *error_;
    QPushButton *cancel_;
};
}
