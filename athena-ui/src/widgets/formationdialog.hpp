/**
 * @file formationdialog.hpp
 * @brief Dialog for formation parameters
 * 
 * v0.7.2: Initial implementation
 */

#ifndef ATHENA_UI_FORMATIONDIALOG_HPP
#define ATHENA_UI_FORMATIONDIALOG_HPP

#include <QDialog>
#include "utils/formationtemplates.hpp"

class QComboBox;
class QDoubleSpinBox;
class QDialogButtonBox;
class QLabel;

namespace athena::ui {

/**
 * @brief Dialog for selecting formation type and parameters
 */
class FormationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FormationDialog(int unitCount, QWidget* parent = nullptr);
    ~FormationDialog() override = default;

    FormationType formationType() const;
    double heading() const;
    double spacing() const;

private:
    void setupUi(int unitCount);
    void updatePreview();

    QComboBox* m_typeCombo = nullptr;
    QDoubleSpinBox* m_headingSpin = nullptr;
    QDoubleSpinBox* m_spacingSpin = nullptr;
    QLabel* m_previewLabel = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};

} // namespace athena::ui

#endif // ATHENA_UI_FORMATIONDIALOG_HPP
