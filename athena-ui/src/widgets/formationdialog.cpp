/**
 * @file formationdialog.cpp
 * @brief Formation dialog implementation
 * 
 * v0.7.2: Initial implementation
 */

#include "formationdialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>

namespace athena::ui {

FormationDialog::FormationDialog(int unitCount, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Formation Settings");
    setMinimumWidth(300);
    setupUi(unitCount);
}

FormationType FormationDialog::formationType() const
{
    return static_cast<FormationType>(m_typeCombo->currentIndex());
}

double FormationDialog::heading() const
{
    return m_headingSpin->value();
}

double FormationDialog::spacing() const
{
    return m_spacingSpin->value();
}

void FormationDialog::setupUi(int unitCount)
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Info label
    auto* infoLabel = new QLabel(QString("Arranging %1 units").arg(unitCount), this);
    infoLabel->setStyleSheet("font-weight: bold; margin-bottom: 10px;");
    mainLayout->addWidget(infoLabel);
    
    // Formation type
    auto* formLayout = new QFormLayout();
    
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Line (horizontal)", static_cast<int>(FormationType::Line));
    m_typeCombo->addItem("Column (vertical)", static_cast<int>(FormationType::Column));
    m_typeCombo->addItem("Wedge (V forward)", static_cast<int>(FormationType::Wedge));
    m_typeCombo->addItem("Vee (V open)", static_cast<int>(FormationType::Vee));
    m_typeCombo->addItem("Echelon (diagonal)", static_cast<int>(FormationType::Echelon));
    m_typeCombo->addItem("Box (grid)", static_cast<int>(FormationType::Box));
    m_typeCombo->addItem("Circle", static_cast<int>(FormationType::Circle));
    formLayout->addRow("Formation:", m_typeCombo);
    
    // Heading
    m_headingSpin = new QDoubleSpinBox(this);
    m_headingSpin->setRange(0.0, 359.9);
    m_headingSpin->setDecimals(1);
    m_headingSpin->setSuffix("°");
    m_headingSpin->setValue(0.0);
    m_headingSpin->setToolTip("Direction the formation faces (0° = North, 90° = East)");
    formLayout->addRow("Heading:", m_headingSpin);
    
    // Spacing
    m_spacingSpin = new QDoubleSpinBox(this);
    m_spacingSpin->setRange(0.1, 10.0);
    m_spacingSpin->setDecimals(1);
    m_spacingSpin->setSuffix(" km");
    m_spacingSpin->setValue(0.5);
    m_spacingSpin->setToolTip("Distance between units in kilometers");
    formLayout->addRow("Spacing:", m_spacingSpin);
    
    mainLayout->addLayout(formLayout);
    
    // Preview area (visual hint)
    auto* previewGroup = new QGroupBox("Preview", this);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(80);
    m_previewLabel->setStyleSheet("background-color: #f0f0f0; border: 1px solid #ccc;");
    previewLayout->addWidget(m_previewLabel);
    mainLayout->addWidget(previewGroup);
    
    // Connect for preview updates
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FormationDialog::updatePreview);
    connect(m_headingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &FormationDialog::updatePreview);
    
    // Buttons
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
    
    updatePreview();
}

void FormationDialog::updatePreview()
{
    FormationType type = formationType();
    double heading = m_headingSpin->value();
    
    // Simple ASCII art preview
    QString preview;
    QString arrow;
    
    // Direction indicator based on heading
    if (heading >= 337.5 || heading < 22.5) arrow = "↑ N";
    else if (heading < 67.5) arrow = "↗ NE";
    else if (heading < 112.5) arrow = "→ E";
    else if (heading < 157.5) arrow = "↘ SE";
    else if (heading < 202.5) arrow = "↓ S";
    else if (heading < 247.5) arrow = "↙ SW";
    else if (heading < 292.5) arrow = "← W";
    else arrow = "↖ NW";
    
    switch (type) {
        case FormationType::Line:
            preview = "● ● ● ● ●";
            break;
        case FormationType::Column:
            preview = "●\n●\n●\n●";
            break;
        case FormationType::Wedge:
            preview = "    ●\n  ● ●\n● ● ●";
            break;
        case FormationType::Vee:
            preview = "●     ●\n  ●  ●\n    ●";
            break;
        case FormationType::Echelon:
            preview = "●\n  ●\n    ●\n      ●";
            break;
        case FormationType::Box:
            preview = "● ● ●\n● ● ●\n● ● ●";
            break;
        case FormationType::Circle:
            preview = "  ● ●\n●     ●\n  ● ●";
            break;
    }
    
    m_previewLabel->setText(QString("%1\n\nFacing: %2").arg(preview).arg(arrow));
}

} // namespace athena::ui
